// SPDX-License-Identifier: GPL-3.0-or-later
// 图标探针 panshi-iconprobe（开发期专用，不安装）
//
// 用途：判定「应用没有图标」到底断在哪一环。图标链路有三段，任何一段断掉
//       都会表现成"没图标"，但修法完全不同：
//         ① desktop 的 Icon= 名字
//         ② 该名字能否被 XDG 图标主题解析（QIcon::fromTheme）
//         ③ 该名字能否被 DTK 的 DIconTheme 解析（findQIcon，应用实际用的就是这个）
//
// 背景：main.cpp 用 DIconTheme::findQIcon("panshi") 设置产品图标。
//       已知 DTK 的 findQIcon 依赖 DConfig 读图标主题名；本机
//       org.deepin.dtk.preference 这个 DConfig 无效（日志里反复出现
//       "DConfig is invalid ... org.deepin.dtk.preference"），
//       因此 ② 通不代表 ③ 通 —— 必须分开测。
//
// 用法：./panshi-iconprobe [图标名]    默认 panshi
//       退出码 0 = fromTheme 与 findQIcon 都能解析到非空图标

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QTextStream>

#include <DGuiApplicationHelper>
#include <DIconTheme>

DGUI_USE_NAMESPACE

/**
 * 把图标真渲染一遍，统计非透明像素占比与主色。
 * 「QIcon 非空」只能证明解析到了文件，证明不了肉眼能看见。
 */
static bool renderStat(const QIcon &ic, const QString &tag, QTextStream &out)
{
    if (ic.isNull()) {
        out << "   " << tag << " : <空图标，跳过渲染>\n";
        return false;
    }
    const int S = 24;
    QImage img(S, S, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    ic.paint(&p, QRect(0, 0, S, S));
    p.end();

    int opaque = 0, colored = 0;
    QRgb sum = 0;
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            const QRgb c = img.pixel(x, y);
            if (qAlpha(c) < 16)
                continue;
            ++opaque;
            const int mx = qMax(qRed(c), qMax(qGreen(c), qBlue(c)));
            const int mn = qMin(qRed(c), qMin(qGreen(c), qBlue(c)));
            if (mx - mn > 24)
                ++colored;
            sum += c;
        }
    }
    const int total = S * S;
    const double fill = double(opaque) / total;
    const double chroma = opaque ? double(colored) / opaque : 0.0;
    out << "   " << tag << " : 非透明 " << int(fill * 100) << "%  彩色占比 "
        << int(chroma * 100) << "%\n";

    // 判据沿用项目既有的双指标：填充太少看不见，彩色占比太低说明是灰底占位
    const bool ok = (fill >= 0.15) && (chroma >= 0.20);
    out << "   " << tag << " : " << (ok ? "可见 ✅" : "不可见 ❌") << "\n";
    return ok;
}

static QString desc(const QIcon &ic)
{
    if (ic.isNull())
        return QStringLiteral("<空图标>");
    const QList<QSize> sizes = ic.availableSizes();
    if (sizes.isEmpty())
        return QStringLiteral("<非空但无可用尺寸>");
    QStringList sl;
    for (const QSize &s : sizes)
        sl << QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    return QStringLiteral("可用尺寸: ") + sl.join(QStringLiteral(", "));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    const QString name = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("panshi");

    out << "图标名          : " << name << "\n";
    out << "当前图标主题    : " << QIcon::themeName() << "\n";
    out << "主题搜索路径    : " << QIcon::themeSearchPaths().join(QStringLiteral(", ")) << "\n";
    out << "\n";

    const QIcon fromTheme = QIcon::fromTheme(name);
    const bool themeOk = !fromTheme.isNull();
    out << "② QIcon::fromTheme   : " << (themeOk ? QStringLiteral("解析成功") : QStringLiteral("失败"))
        << "  " << desc(fromTheme) << "\n";

    const QIcon viaDtk = DIconTheme::findQIcon(name);
    const bool dtkOk = !viaDtk.isNull();
    out << "③ DIconTheme::findQIcon: " << (dtkOk ? QStringLiteral("解析成功") : QStringLiteral("失败"))
        << "  " << desc(viaDtk) << "\n";

    // ⚠️ 「解析成功」不等于「看得见」：QIcon 非空也可能渲染出全透明/纯白图形。
    //    必须真的画一遍再统计像素 —— 这是唯一能证明"用户能看到图标"的判据。
    out << "④ 像素级可见性（渲染 24px 后统计）：\n";
    const bool visTheme = renderStat(fromTheme, QStringLiteral("fromTheme"), out);
    const bool visDtk = renderStat(viaDtk, QStringLiteral("findQIcon "), out);

    out << "\n";
    if (!themeOk && !dtkOk) {
        out << "FAIL  两段都失败：图标名在图标主题里根本不存在\n";
        out << "      → 检查 /usr/share/icons/hicolor/*/apps/ 与 ~/.local/share/icons/hicolor/*/apps/\n";
        out << "        是否真有 " << name << ".svg/.png，以及 icon-theme.cache 是否已更新\n";
    } else if (themeOk && !dtkOk) {
        out << "FAIL  XDG 主题能解析、但 DTK 的 findQIcon 为空\n";
        out << "      → 典型原因：DTK 读不到图标主题名（DConfig org.deepin.dtk.preference 无效）。\n";
        out << "        此时应用窗口/任务栏仍会没有图标，尽管启动器里可能有。\n";
    } else if (!visTheme && !visDtk) {
        out << "FAIL  解析到了图标，但渲染出来没有可见内容（全透明或纯白）\n";
        out << "      → 图标文件本身有问题（如 .dci 内容为空 / SVG 无描边）。\n";
    } else {
        out << "PASS  两段都能解析、且渲染出可见内容\n";
    }

    return (themeOk && dtkOk && (visTheme || visDtk)) ? 0 : 1;
}
