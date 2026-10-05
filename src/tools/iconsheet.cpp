// SPDX-License-Identifier: GPL-3.0-or-later
//
// 图标候选接触表（开发期工具，不安装）
//
// 目的：侧栏图标「看不清」不能靠猜图标名解决 —— 名录里有 1277 个 .dci，
//       哪些既有**颜色锚点**、又和「备份/更新/保护」的语义对得上，只有看图才知道。
//       本工具把候选按**侧栏真实尺寸 20px**（以及 3 倍放大便于细看）渲染到
//       浅底与深底两张对照表上，一眼筛掉「单色细线」「缩小后糊成一团」的。
//
// 为什么必须用 Qt 渲染而不是直接看 .dci 文件：
//   .dci 是 Qt 私有容器格式，工具链里没有解码器；且 DIconTheme 的查找链
//   （dci → builtin → XDG 主题）只有在 Qt 里才走得通，所见即应用所得。
//
// 用法：panshi-iconsheet [输出png前缀]
//   输出 <前缀>_light.png / <前缀>_dark.png

#include <DGuiApplicationHelper>
#include <DIconTheme>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <algorithm>

DGUI_USE_NAMESPACE

namespace {

struct Candidate {
    const char *name;
    const char *label;
};

// 本轮实测筛出的候选（饱和度由 panshi-dciquery 实测，见文件头说明）。
// 分组标注了**建议采用**的项 —— 判据是「语义对得上 + 饱和度高 + 20px 下不糊」。
const Candidate kCandidates[] = {
    // 本轮**新设计**的 4 个自带 SVG（data/icons/nav/），按侧栏顺序。
    // 本工具按**文件路径直接加载**，所以这些图标在任何环境下都能真实渲染 ——
    // 上一轮 DCI 图标在这台机器上是渲染不出来的（DConfig 不可用）。
    {"shield-check",     "系统保护"},
    {"layers-snapshot",  "备份与版本"},
    {"cycle-update",     "系统更新"},
    {"info-badge",       "关于"},
};

constexpr int kCellW = 150;
constexpr int kCellH = 104;
constexpr int kCols = 5;
constexpr int kIconReal = 20;   // 侧栏真实尺寸
constexpr int kIconZoom = 64;   // 放大细看

/**
 * 定位 .dci 文件。
 *
 * ⚠️ 不能用 DIconTheme::findQIcon()：本容器 DConfig 不可用 → DTK 读不到图标
 *    主题名 → 任何 dci 都返回空 QIcon，接触表会全是 NULL，
 *    「全是 NULL」是**环境问题**，据此判断「图标不存在」是假阴性。
 *    改为 findDciIconFile 定位文件 + QIcon(路径) 直接加载。
 */
QString resolveDci(const QString &name)
{
    // 与 sidebar.cpp::navIcon() 同一优先级：自带 SVG 优先，其次 DCI
    const QString bundled = QStringLiteral(":/icons/nav/%1.svg").arg(name);
    if (QFileInfo::exists(bundled))
        return bundled;
    static const QStringList searchPaths = DIconTheme::dciThemeSearchPaths();
    QStringList themes;
    for (const QString &p : searchPaths) {
        for (const QString &sub : QDir(p).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            themes << sub;
    }
    themes << QStringLiteral("bloom") << QStringLiteral("deepin");
    for (const QString &th : themes) {
        const QString f = DIconTheme::findDciIconFile(name, th);
        if (!f.isEmpty() && QFileInfo::exists(f))
            return f;
    }
    return QString();
}

/** 浅/深底取自 DTK 语义：Base=容器底，TextTitle=文字 */
void renderSheet(bool dark, const QString &outPath)
{
    const int rows = (int(sizeof(kCandidates) / sizeof(kCandidates[0])) + kCols - 1) / kCols;
    QImage img(kCols * kCellW, rows * kCellH, QImage::Format_ARGB32_Premultiplied);
    img.fill(dark ? QColor(0x2b, 0x2b, 0x2b) : QColor(0xf7, 0xf7, 0xf7));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f;
    f.setPointSizeF(7.5);
    p.setFont(f);

    int n = 0;
    for (const Candidate &c : kCandidates) {
        const int col = n % kCols;
        const int row = n / kCols;
        const int x = col * kCellW;
        const int y = row * kCellH;
        ++n;

        const QString dci = resolveDci(QString::fromLatin1(c.name));
        const QIcon ic = dci.isEmpty() ? QIcon() : QIcon(dci);

        // 格线，便于肉眼分辨列
        p.setPen(dark ? QColor(0x3d, 0x3d, 0x3d) : QColor(0xe2, 0xe2, 0xe2));
        p.drawRect(x + 2, y + 2, kCellW - 5, kCellH - 5);

        if (ic.isNull()) {
            p.setPen(dark ? QColor(0xd0, 0x60, 0x60) : QColor(0xc0, 0x30, 0x30));
            p.drawText(QRect(x + 6, y + 6, kCellW - 12, 20), Qt::AlignLeft | Qt::AlignTop,
                       QStringLiteral("未找到"));
        } else {
            // 上：真实 20px（放在白/深底上，模拟侧栏观感）
            // 下：放大 56px，看清图形本身
            ic.paint(&p, QRect(x + 10, y + 10, kIconReal, kIconReal),
                     Qt::AlignCenter, QIcon::Normal);
            ic.paint(&p, QRect(x + 40, y + 6, kIconZoom, kIconZoom),
                     Qt::AlignCenter, QIcon::Normal);
        }

        p.setPen(dark ? QColor(0xb0, 0xb0, 0xb0) : QColor(0x50, 0x50, 0x50));
        p.drawText(QRect(x + 6, y + 68, kCellW - 12, 16),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString::fromLatin1(c.name));
        p.drawText(QRect(x + 6, y + 84, kCellW - 12, 16),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString::fromLatin1(c.label));
    }
    p.end();
    img.save(outPath);
    QTextStream(stdout) << "wrote " << outPath << "\n";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLatin1(argv[1])
                                    : QStringLiteral("/tmp/iconsheet");

    // ⚠️ DTK6 已移除 setThemeType()（头文件里被 `#if DTK_VERSION < 6` 包掉，
    //    写上去是**编译错误**不是警告）；亮暗切换用 setPaletteType()。
    DGuiApplicationHelper::instance()->setPaletteType(
        DGuiApplicationHelper::LightType);
    renderSheet(false, prefix + QStringLiteral("_light.png"));

    DGuiApplicationHelper::instance()->setPaletteType(
        DGuiApplicationHelper::DarkType);
    renderSheet(true, prefix + QStringLiteral("_dark.png"));
    return 0;
}
