// SPDX-License-Identifier: GPL-3.0-or-later
//
// 侧栏渲染探针 panshi-navshot（开发期专用，不安装）
//
// 目的：侧栏图标「看不清」这件事，**只看图标文件存在是不够的** ——
//      名字存在但取回的是单色 symbolic、或在 20px 下糊成一团，
//      肉眼在真机上一眼就能看出，探针却可能全绿。
//      本探针直接用**真实的 PanshiSidebar**（同一份 navIcon + 同一个 delegate）
//      离屏渲染出图，亮/暗各一张 —— 所见即用户所见。
//
// 附带断言：每个导航项的图标**实测饱和度**达标（单色 = 用户 complaint 重现），
//           这是把「好看」变成可回归的数字，而不是靠人盯。
//
// 用法：panshi-navshot [输出前缀]

#include "ui/sidebar.h"

#include <DGuiApplicationHelper>
#include <DIconTheme>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <algorithm>

#include <cstdlib>

using namespace PanshiUi;

namespace {

int g_fail = 0;

void check(bool ok, const QString &what)
{
    QTextStream o(stdout);
    o << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    o.flush();
    if (!ok)
        ++g_fail;
}

/** 与 DIconTheme 无关地按文件定位（容器内 DConfig 不可用，主题名读不到） */
QString resolveDci(const QString &name)
{
    // 与 sidebar.cpp::navIcon() 同一优先级：自带 SVG 优先，其次 DCI
    const QString bundled = QStringLiteral(":/icons/nav/%1.svg").arg(name);
    if (QFileInfo::exists(bundled))
        return bundled;
    static const QStringList roots = DIconTheme::dciThemeSearchPaths();
    QStringList themes;
    for (const QString &p : roots)
        for (const QString &sub : QDir(p).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            themes << sub;
    themes << QStringLiteral("bloom") << QStringLiteral("deepin");
    for (const QString &th : themes) {
        const QString f = DIconTheme::findDciIconFile(name, th);
        if (!f.isEmpty() && QFileInfo::exists(f))
            return f;
    }
    return QString();
}

/**
 * 20px 下的可读性实测，返回 {饱和度90分位, 彩占比}。
 *
 * ⚠️ **两个指标都要看，少一个就会漏**（本项目已踩过）：
 *   只看饱和度 90 分位，会被「大面积白底 + 少量高饱和 accent」的图标骗过去 ——
 *   实测 uos-installtool 的 S90 = 0.94（看着很艳），但彩占比只有 30%、
 *   彩像素平均明度 0.83，20px 下几乎看不见，用户照样会说「图标看不清」。
 *   → 必须加「彩占比」（着色像素 / 全部不透明像素）。
 *   判据：S90 ≥ 0.15（有颜色，不是单色 symbolic）且 彩占比 ≥ 0.40（有足够色块）。
 */
struct Legibility { qreal s90; qreal colorRatio; bool ok; };

Legibility legibilityAt20(const QString &iconName)
{
    const QString f = resolveDci(iconName);
    if (f.isEmpty())
        return { -1.0, -1.0, false };
    QImage img(20, 20, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QIcon(f).paint(&p, QRect(0, 0, 20, 20), Qt::AlignCenter, QIcon::Normal);
    p.end();

    int ink = 0, colored = 0;
    QList<qreal> sats;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() < 24 || c.value() < 24)
                continue;
            ++ink;
            const qreal sat = c.toHsv().saturationF();
            sats << sat;
            if (sat > 0.15)
                ++colored;
        }
    if (sats.isEmpty())
        return { -1.0, -1.0, false };
    std::sort(sats.begin(), sats.end());
    const qreal s90 = sats.at(int(sats.size() * 0.9));
    const qreal ratio = double(colored) / double(ink);
    return { s90, ratio, s90 >= 0.15 && ratio >= 0.40 };
}

void shoot(PanshiSidebar *sb, bool dark, const QString &path)
{
    DGuiApplicationHelper::instance()->setPaletteType(
        dark ? DGuiApplicationHelper::DarkType : DGuiApplicationHelper::LightType);
    // 侧栏主体是 QListView，它的 sizeHint 不含「7 行内容」的高度，
    // 直接用 sizeHint 会只露出两三行（截图里就只看到 3 项）。
    // 按「行数 × 行高 + 分组间距」给足高度，确保 7 个导航项全部入镜。
    sb->setFixedWidth(200);
    sb->setFixedHeight(480);
    sb->show();
    qApp->processEvents();
    sb->grab().save(path);
    QTextStream o(stdout);
    o << "wrote " << path << "\n";
    o.flush();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-manager"));

    const QString prefix = argc > 1 ? QString::fromLatin1(argv[1])
                                    : QStringLiteral("/tmp/navshot");

    // 与 mainwindow.cpp 保持同一份清单（名字改了这里也要改）
    struct Item { const char *icon; const char *fallback; const char *text; };
    static const Item kNav[] = {
        {"deepin-system-monitor", "deepin-home",         "总览"},
        {"drive-harddisk",         "deepin-diskmanager",  "磁盘空间"},
        {"shield-check",           "deepin-defender",     "系统保护"},
        {"layers-snapshot",        "application-x-archive","备份与版本"},
        {"cycle-update",           "upgrade-tool",        "系统更新"},
        {"preferences-system",     "deepin-home",         "策略与设置"},
        {"info-badge",             "dialog-information",  "关于"},
    };

    QTextStream o(stdout);
    o << "== 侧栏图标饱和度断言（20px 实测）==\n";
    o << QString("%1 %2 %3\n").arg(QStringLiteral("导航项"), -14)
                                 .arg(QStringLiteral("图标名"), -26)
                                 .arg(QStringLiteral("饱和度/判定"), -18);
    for (const Item &it : kNav) {
        const Legibility L = legibilityAt20(QString::fromLatin1(it.icon));
        const QString verdict = L.s90 < 0 ? QStringLiteral("**图标不存在**")
                                           : (L.ok ? QStringLiteral("清晰 ✅")
                                                   : QStringLiteral("看不清 ❌"));
        o << QString("%1 %2 %3\n")
              .arg(QString::fromLatin1(it.text), -14)
              .arg(QString::fromLatin1(it.icon), -26)
              .arg(QString("S=%1 彩=%2%  %3")
                       .arg(L.s90 < 0 ? QStringLiteral(" — ")
                                      : QString::number(L.s90, 'f', 2), -7)
                       .arg(L.colorRatio < 0 ? QStringLiteral("—")
                                              : QString::number(L.colorRatio * 100, 'f', 0), -5)
                         .arg(verdict), -22);
        check(L.ok, QStringLiteral("%1 的图标「%2」在 20px 下清晰（S=%3 彩占比=%4）")
                         .arg(QString::fromLatin1(it.text),
                              QString::fromLatin1(it.icon),
                              L.s90 < 0 ? QStringLiteral("不存在")
                                        : QString::number(L.s90, 'f', 2),
                              L.colorRatio < 0 ? QStringLiteral("不存在")
                                               : QString::number(L.colorRatio * 100, 'f', 0)
                                                     + QStringLiteral("%")));
    }

    // ---- 截图可信度分两类，别一概而论 ----
    // · **自带 SVG 的 4 项**（shield-check / layers-snapshot / cycle-update /
    //   info-badge）：走 qrc，不经 DIconTheme → 本机渲染**就是真实效果**，可看。
    // · **走 DCI 的 3 项**（总览 / 磁盘空间 / 策略与设置）：本机 DConfig 不可用 →
    //   DTK 读不到图标主题名 → findQIcon() 返回空 → 回落到 QStyle 标准图标，
    //   截图里那 3 行是**兜底图**，不是真实图标（判据：换图标名后该行 md5 不变）。
    // 断言值两者都可信（走 resolveDci() 直接读文件，不经 DIconTheme）。
    o << "\n[注意] 侧栏截图里「自带 SVG」的 4 项是真实效果；\n"
         "       「总览/磁盘空间/策略与设置」这 3 项走 DCI，本机 DConfig 不可用，\n"
         "       截图显示的是 QStyle 兜底（真实长相见 panshi-iconsheet）。\n";

    // 渲染真实侧栏（只看排版）
    auto *sb = new PanshiSidebar;
    sb->beginGroup(QObject::tr("概览"));
    sb->addItem(QString::fromLatin1(kNav[0].icon),
                QString::fromLatin1(kNav[0].fallback),
                QObject::tr(kNav[0].text), 1);
    sb->beginGroup(QObject::tr("系统维护"));
    for (int i = 1; i <= 4; ++i)
        sb->addItem(QString::fromLatin1(kNav[i].icon),
                    QString::fromLatin1(kNav[i].fallback),
                    QObject::tr(kNav[i].text), i + 1);
    sb->beginGroup(QObject::tr("设置"));
    for (int i = 5; i <= 6; ++i)
        sb->addItem(QString::fromLatin1(kNav[i].icon),
                    QString::fromLatin1(kNav[i].fallback),
                    QObject::tr(kNav[i].text), i + 1);

    shoot(sb, false, prefix + QStringLiteral("_light.png"));
    DGuiApplicationHelper::instance()->setPaletteType(
        DGuiApplicationHelper::DarkType);
    qApp->processEvents();
    shoot(sb, true, prefix + QStringLiteral("_dark.png"));

    o << "\n";
    if (g_fail == 0) {
        o << "PASS: 全部导航图标均为彩色且真实存在\n";
        o.flush();
        return 0;
    }
    o << "FAIL: " << g_fail << " 个导航项的图标不达标\n";
    o.flush();
    return 1;
}
