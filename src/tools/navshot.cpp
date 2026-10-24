// SPDX-License-Identifier: GPL-3.0-or-later
//
// 侧栏渲染探针 panshi-navshot（开发期专用，不安装）
//
// 目的：侧栏图标「看不清/认不出」这件事，**只看图标文件存在是不够的** ——
//      名字存在但取回的是 QStyle 兜底图（7 个图标全长同一个样）、或 20px 下
//      糊成一团，肉眼在真机上一眼就能看出，探针却可能全绿。
//      本探针直接用**真实的 PanshiSidebar**（同一份 navIcon + 同一个 delegate）
//      离屏渲染出图，亮/暗各一张 —— 所见即用户所见。
//
// 断言分四组（2026-10-24 随「线性图标 + 平铺」风格重写，替代原饱和度判据）：
//   ① 存在性：7 个自带 SVG 都能解析到文件。
//      （原故障实录：7 个名字里 5 个在 DCI 库不存在，集体回落到同一个
//        QStyle 灰色箭头 —— 用户截图里「只有磁盘空间是彩色的」。）
//   ② 可读性：20px 实测墨迹覆盖率 ∈ [6%, 70%]。
//      低于 6% = 几乎看不见；高于 70% = 一团糊笔、形状不可辨。
//   ③ 差异度：任两图标 20px 墨迹差异度 ≥ 15%。
//      直接封死「回落成同一个兜底图标」这类故障（原饱和度判据只能间接暴露）。
//   ④ 染色契约（真实 sidebar 截图像素级，亮/暗各一遍）：
//      选中行图标带必须出现 Accent 色像素、未选中行图标带必须没有 ——
//      单色源图 + delegate 现染是本风格的核心，断了就是「图标不跟随选中态」。
//      色值取当前主题的 semanticColor()，不写死蓝/黑。
//
// ⚠️ 判据有效性靠**回退实现**验证（本项目铁律，勿省）：
//   · 把任一图标名改成不存在的名字 → ① 必须 FAIL；
//   · 把 delegate 的 tintedPixmap() 换回 ic.paint() → ④ 必须 FAIL。
//   两个方向都验过，判据才算数。
//
// 用法：panshi-navshot [输出前缀]

#include "ui/sidebar.h"

#include <DGuiApplicationHelper>
#include <DIconTheme>

// style.h 自身不含 DTK 头（依赖调用方先引入，见其文件头注释），须排在其后
#include "style.h"

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
 * 20px 渲染出的墨迹掩码图（原样渲染，不染色）。
 *
 * ⚠️ 与已废弃的饱和度判据的关键区别：**不能**按「明度 < 24 跳过」过滤像素 ——
 * 单色源图是纯黑（HSV 明度 0），那样过滤会把几乎所有墨迹跳过，判据反而
 * 「看不见」任何图标（假阴性，比 FAIL 更危险）。单色图只认 alpha；
 * 颜色由 delegate 现染，不关源图的事。
 */
QImage inkMaskAt20(const QString &iconName)
{
    QImage img(20, 20, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    const QString f = resolveDci(iconName);
    if (f.isEmpty())
        return img;   // 全透明 = 文件不存在
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QIcon(f).paint(&p, QRect(0, 0, 20, 20), Qt::AlignCenter, QIcon::Normal);
    p.end();
    return img;
}

/** 墨迹覆盖率 = 不透明像素 / 总像素 */
double inkRatio(const QImage &img)
{
    int ink = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixelColor(x, y).alpha() >= 24)
                ++ink;
    return double(ink) / double(img.width() * img.height());
}

/**
 * 两图墨迹差异度 = 对称差像素 / 较大一方墨迹数。
 * 1 = 完全不同；0 = 一模一样（正是「集体回落成同一兜底图标」的数值特征）。
 */
double inkDiff(const QImage &a, const QImage &b)
{
    int diff = 0, inkA = 0, inkB = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const bool ia = a.pixelColor(x, y).alpha() >= 24;
            const bool ib = b.pixelColor(x, y).alpha() >= 24;
            inkA += ia;
            inkB += ib;
            if (ia != ib)
                ++diff;
        }
    const int mx = std::max(inkA, inkB);
    return mx == 0 ? 0.0 : double(diff) / double(mx);
}

/** 统计区域内与目标色相近（每通道差 ≤ tol）的像素数 */
int countNearColor(const QImage &img, const QRect &area, const QColor &c, int tol = 24)
{
    int n = 0;
    const int x0 = std::max(0, area.left());
    const int y0 = std::max(0, area.top());
    const int x1 = std::min(img.width(), area.right());
    const int y1 = std::min(img.height(), area.bottom());
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const QColor p = img.pixelColor(x, y);
            if (qAbs(p.red() - c.red()) <= tol && qAbs(p.green() - c.green()) <= tol
                && qAbs(p.blue() - c.blue()) <= tol)
                ++n;
        }
    }
    return n;
}

/**
 * 染色契约断言（④，像素级）。
 *
 * 侧栏行几何（PanshiSidebar：布局 margin 0、行高 44、无 frame）：
 *   第 n 行 y ∈ [44n, 44n+44)；图标列 x ∈ [16, 44)（图标 20px 在 x=20..40，
 *   文字从 x=52 起 —— 故到 44 为止不会掺入文字像素）。
 *
 * ⚠️ 判据为什么用「相对背景」而不是「绝对近色」：
 *   20px 下 1.4px 笔画经抗锯齿，核心像素 alpha≈84% —— 叠在底色上得到的是
 *   「85% 语义色 + 15% 底色」的混合值（亮色主题实测 #252525，距纯黑 37），
 *   固定 tol 的绝对近色判定会漏计（初版就栽在这：norNormal=0 假 FAIL）。
 *   且单色源图本身就是黑的：亮色主题下「染 Normal」与「不染」同色，
 *   只有暗色主题（Normal=白）才能区分染/不染 —— 故暗色那半必须是硬判据。
 *
 * 断言（色值取当前主题 semanticColor()，亮/暗各自成立）：
 *   选中行（第 0 行，调用方已 setCurrentData(1)）图标带：
 *     · Accent 像素 ≥ 20 —— 图标随选中态染色（选中态有全 alpha 的笔画段，
 *       绝对近色在这里成立：实测亮/暗各 30+ 纯色像素）。
 *   未选中行（第 1 行）图标带：
 *     · 「比背景更接近 Normal」的像素 ≥ 15 —— 常规态图标朝正文色移动；
 *       暗色主题下若不染（源图纯黑直出），这些像素距 Normal≈255，
 *       远于背景（≈215）→ 必然 FAIL，正是要的区分度。
 */
void assertTintContract(const QImage &shot, const QString &theme)
{
    const QColor accent = semanticColor(Semantic::Accent);
    const QColor normal = semanticColor(Semantic::Normal);
    const QRect bandSel(16, 10, 28, 24);            // 第 0 行图标带
    const QRect bandNor(16, 44 + 10, 28, 24);       // 第 1 行图标带
    // 背景取样点：行左边距（x=2），无圆角底、无图标 —— 就是纯底色
    const QColor bgNor = shot.pixelColor(2, 46);

    const int selAccent = countNearColor(shot, bandSel, accent);

    // 未选中行：统计「比背景更接近 Normal」的像素（三通道最大差为距离）
    auto dist = [](const QColor &a, const QColor &b) {
        return std::max({qAbs(a.red() - b.red()), qAbs(a.green() - b.green()),
                         qAbs(a.blue() - b.blue())});
    };
    const int dBg = dist(bgNor, normal);
    int norTowardNormal = 0;
    const int x0 = std::max(0, bandNor.left());
    const int y0 = std::max(0, bandNor.top());
    const int x1 = std::min(shot.width(), bandNor.right());
    const int y1 = std::min(shot.height(), bandNor.bottom());
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (dist(shot.pixelColor(x, y), normal) < dBg)
                ++norTowardNormal;

    QTextStream o(stdout);
    o << QStringLiteral("  [%1] Accent=%2 Normal=%3 | 选中行 Accent 像素=%4 | "
                        "未选中行背景=%5（距 Normal %6），朝 Normal 的像素=%7\n")
             .arg(theme).arg(accent.name()).arg(normal.name())
             .arg(selAccent).arg(bgNor.name()).arg(dBg).arg(norTowardNormal);
    o.flush();

    check(selAccent >= 20,
          QStringLiteral("%1：选中行图标已染成 Accent 色（像素 %2）").arg(theme).arg(selAccent));
    check(norTowardNormal >= 15,
          QStringLiteral("%1：未选中行图标朝正文色移动（像素 %2，背景距 Normal %3）")
              .arg(theme).arg(norTowardNormal).arg(dBg));
}

/** 离屏渲染真实侧栏并存图，返回截图像素（供染色契约断言） */
QImage shoot(PanshiSidebar *sb, bool dark, const QString &path)
{
    DGuiApplicationHelper::instance()->setPaletteType(
        dark ? DGuiApplicationHelper::DarkType : DGuiApplicationHelper::LightType);
    qApp->processEvents();
    // 与 main.cpp 的 applicationPaletteChanged 处理一致：拆旧 QSS → 按新调色板
    // 重建。QSS 里的色值是**生成时快照**的，不重建的话暗色截图侧栏会停在
    // 亮色白底（既骗眼睛，也让「朝 Normal 移动」判据拿到错的背景值）。
    qApp->setStyleSheet(QString());
    qApp->setStyleSheet(panshiStyleSheet());
    // 侧栏主体是 QListView，它的 sizeHint 不含「7 行内容」的高度，
    // 直接用 sizeHint 会只露出两三行（旧版截图里就只看到 3 项）。
    // 2026-10-24 起无分组平铺：7 行 × 44 = 308，给 330 刚好装下不拖尾。
    sb->setFixedWidth(200);
    sb->setFixedHeight(330);
    sb->show();
    qApp->processEvents();
    const QImage shot = sb->grab().toImage();
    shot.save(path);
    QTextStream o(stdout);
    o << "wrote " << path << "\n";
    o.flush();
    return shot;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-manager"));

    // ⚠️ 必须铺应用同款 QSS（与 main.cpp 一致）：QListView 的底色走
    // QPalette::Base，而本进程的 QApplication 调色板不跟随 DTK 主题切换
    // （2026-10-05 暗色故障根因）—— 不铺 QSS 的话暗色截图里侧栏仍是浅灰，
    // 既骗眼睛，也让「朝 Normal 移动」的判据拿错背景值。
    app.setStyleSheet(panshiStyleSheet());

    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("/tmp/navshot");

    // 与 mainwindow.cpp 保持同一份清单（名字改了这里也要改）。
    // 2026-10-24 起 7 项全部走自带单色 SVG（无分组平铺）。
    struct Item { const char *icon; const char *fallback; const char *text; };
    static const Item kNav[] = {
        {"deepin-system-monitor", "deepin-home",          "总览"},
        {"drive-harddisk",         "deepin-diskmanager",  "磁盘空间"},
        {"shield-check",           "deepin-defender",     "系统保护"},
        {"layers-snapshot",        "application-x-archive","备份与版本"},
        {"cycle-update",           "upgrade-tool",        "系统更新"},
        {"preferences-system",     "deepin-home",         "策略与设置"},
        {"info-badge",             "dialog-information",  "关于"},
    };
    const int kNavCount = int(sizeof(kNav) / sizeof(kNav[0]));

    QTextStream o(stdout);
    o << "== 线性导航图标实测（20px 墨迹覆盖率 + 差异度）==\n";
    o << QString("%1 %2 %3 %4\n").arg(QStringLiteral("导航项"), -14)
                                 .arg(QStringLiteral("图标名"), -26)
                                 .arg(QStringLiteral("墨迹"), -8)
                                 .arg(QStringLiteral("判定"), -10);
    o.flush();

    // ①② 存在性 + 墨迹覆盖率
    QList<QImage> masks;
    masks.reserve(kNavCount);
    for (int i = 0; i < kNavCount; ++i) {
        const QString name = QString::fromLatin1(kNav[i].icon);
        const QString file = resolveDci(name);
        const QImage mask = inkMaskAt20(name);
        const double ratio = inkRatio(mask);
        masks << mask;

        o << QString("%1 %2 %3\n")
                 .arg(QString::fromLatin1(kNav[i].text), -14)
                 .arg(name, -26)
                 .arg(QString(QStringLiteral("%1%")).arg(ratio * 100, 0, 'f', 1), -8);
        o.flush();

        check(!file.isEmpty(),
              QStringLiteral("%1 的图标文件「%2」可解析").arg(
                  QString::fromLatin1(kNav[i].text), name));
        check(ratio >= 0.06 && ratio <= 0.70,
              QStringLiteral("%1 的图标 20px 墨迹覆盖率 %2% 在 [6%,70%]（可见且不成团）")
                  .arg(QString::fromLatin1(kNav[i].text))
                  .arg(ratio * 100, 0, 'f', 1));
    }

    // ③ 两两差异度（封死「集体回落成同一兜底图标」）
    for (int i = 0; i < kNavCount; ++i) {
        for (int j = i + 1; j < kNavCount; ++j) {
            const double d = inkDiff(masks.at(i), masks.at(j));
            check(d >= 0.15,
                  QStringLiteral("「%1」与「%2」形状可区分（墨迹差异 %3%）")
                      .arg(QString::fromLatin1(kNav[i].text),
                           QString::fromLatin1(kNav[j].text))
                      .arg(d * 100, 0, 'f', 0));
        }
    }

    // ④ 真实侧栏截图 + 染色契约（亮/暗各一遍）
    auto *sb = new PanshiSidebar;
    for (int i = 0; i < kNavCount; ++i)
        sb->addItem(QString::fromLatin1(kNav[i].icon),
                    QString::fromLatin1(kNav[i].fallback),
                    QObject::tr(kNav[i].text), i + 1);
    // 选中第 0 行（总览）：染色契约的「选中态」样本
    sb->setCurrentData(1);

    const QImage light = shoot(sb, false, prefix + QStringLiteral("_light.png"));
    assertTintContract(light, QStringLiteral("亮色"));
    const QImage dark = shoot(sb, true, prefix + QStringLiteral("_dark.png"));
    assertTintContract(dark, QStringLiteral("暗色"));

    o << "\n";
    if (g_fail == 0) {
        o << "PASS: 7 个线性图标均存在、可读、互异，染色契约亮暗双主题成立\n";
        o.flush();
        return 0;
    }
    o << "FAIL: " << g_fail << " 项断言未通过\n";
    o.flush();
    return 1;
}
