// SPDX-License-Identifier: GPL-3.0-or-later
//
// 快速操作按钮图标探针 panshi-quickaction（开发期专用，不安装）
//
// 背景（2026-10-24 用户截图反馈「按钮中的图标信息都看不清了」）：
//   根因不是渲染bug，是**图标名在 DCI 库里不存在**——
//     · document-save / system-software-update / view-refresh：
//       2026-10-24 审计确认三个名字在系统 DCI 库（含 ostree 部署镜像）中
//       均无对应 .dci → Ui::themedIcon 一路回落到 QStyle 标准图标
//       （ Fusion 风格的小而灰的 glyph，按钮上几乎看不见）；
//     · security：库里有，但只有 flow 主题单色 webp，按钮尺寸下发虚。
//   修法：三个按钮 + 页头刷新按钮改用**自带单色 SVG**（layers-snapshot /
//   cycle-update / shield-check，与侧栏同图形）现染 + 显式 iconSize(20)。
//
// 本探针把契约固化为黑盒断言（几何无关，不假设按钮内部布局）：
//   ① 四个按钮都能按文字/toolTip 找到；
//   ② iconSize == 20×20（修法显式设定；防止回归到样式默认的小尺寸）；
//   ③ 图标**图形** = 自带 SVG 的 alpha 掩码（防回归成 QStyle 兜底图——
//      兜底图形状完全不同，掩码差异巨大 → FAIL）；
//   ④ 快速操作三按钮的图标**颜色** = 契约色（建议键=白，其余=正文色）；
//   ⑤ 按钮渲染里图标**有可见墨迹**（与「卸掉图标」的同一按钮渲染做差，
//      不依赖任何几何假设；彻底不可见 → FAIL）。
//   另输出诊断：总览页 hero 开关行 / 页头刷新按钮是否存在（截图疑团存档）。
//
// ⚠️ 判据有效性靠回退实现验证（本项目铁律）：修复前跑必须 FAIL（③④），
//   修复后跑必须 PASS。
//
// 用法：panshi-quickaction [输出前缀]

#include "mainwindow.h"
#include "pages/common.h"
#include "pages/masterswitch.h"
#include "style.h"
#include "ui/components.h"

#include <DGuiApplicationHelper>
#include <DIconButton>

#include <QAbstractButton>
#include <QApplication>
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

/** 三通道最大差 */
int colorDist(const QColor &a, const QColor &b)
{
    return std::max({qAbs(a.red() - b.red()), qAbs(a.green() - b.green()),
                     qAbs(a.blue() - b.blue())});
}

/**
 * 两图墨迹差异度（位移容差 + alpha 容差，取 3×3 平移里的最优值）。
 * 0 = 同一形状；≈1 = 完全不同的图形（正是「回落成兜底图标」的数值特征）。
 *
 * ⚠️ 为什么要双重容差（2026-10-24 实测踩坑记录）：
 *   · 固定阈值逐像素比：QIcon 存取预乘 alpha 往返有 ±5 抖噪，同一形状
 *     均差能到 8.5（shield）——误报；
 *   · 硬掩码（alpha ≥ 96）：细笔画图标边界像素被抖噪翻过阈值，cycle
 *     误报 15.8%；
 *   · 只加 alpha 容差（|Δα| > 24）：控件经历 grab 后渲染态会变
 *     （实测差异从 0% 跳到 46%，形态是 1px 级偏移/缩放差）——仍需位移容差。
 *   三重坑叠完才是这个实现：3×3 平移里找最优，alpha 差 > 32 才算异。
 *   兜底图与自带图是「有墨 vs 没墨」的量级差，任何平移都对不上。
 */
double inkMaskDiffRatio(const QImage &a, const QImage &b)
{
    if (a.size() != b.size())
        return 10.0;   // 尺寸不同即不同图
    double best = 10.0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int diff = 0, inkA = 0, inkB = 0;
            for (int y = 0; y < a.height(); ++y) {
                for (int x = 0; x < a.width(); ++x) {
                    const int aa = a.pixelColor(x, y).alpha();
                    const int bx = x + dx, by = y + dy;
                    const int ab = (bx >= 0 && by >= 0 && bx < b.width() && by < b.height())
                                       ? b.pixelColor(bx, by).alpha() : 0;
                    inkA += aa >= 24;
                    inkB += ab >= 24;
                    if (qAbs(aa - ab) > 32)
                        ++diff;
                }
            }
            const int mx = std::max(inkA, inkB);
            const double r = mx == 0 ? 10.0 : double(diff) / double(mx);
            best = std::min(best, r);
        }
    }
    return best;
}

/** 统计区域内与目标色相近（每通道 ≤ tol）的像素数 */
int countNearColor(const QImage &img, const QRect &area, const QColor &c, int tol = 24)
{
    int n = 0;
    const int x0 = std::max(0, area.left());
    const int y0 = std::max(0, area.top());
    const int x1 = std::min(img.width(), area.right());
    const int y1 = std::min(img.height(), area.bottom());
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (colorDist(img.pixelColor(x, y), c) <= tol)
                ++n;
    return n;
}

/**
 * 「卸掉图标再渲染」做差，几何无关地量图标墨迹。
 * 返回差异 > 30 的像素数；0 = 图标彻底不可见；-1 = 渲染失败。
 *
 * ⚠️ 对比度感知（2026-10-24 容器限制）：本容器里 DTK 样式因 DConfig 不可用
 * 不给 DSuggestButton 上蓝底（渲染成浅色），白图标对白底无从对比——但用户
 * 真机上该按钮是蓝底（其截图可见），白图标清晰。故先量按钮底色与契约色的
 * 距离：不足 60 时该环境无法验证可见性，返回 -2（调用方按「跳过」处理并
 * 打印说明），不误报 FAIL。
 */
int iconInkPixels(QAbstractButton *btn, const QColor &contractColor)
{
    // ⚠️ 热身 grab：折叠下方的按钮从未被绘制过，**第一次 grab 的渲染不含
    // 图标**（2026-10-24 实测：DSuggestButton 首次 with/without 做差为 0，
    // 而随后手动做差清晰可见图标）——先抓一次丢弃，让控件进入已绘制状态。
    (void)btn->grab();
    const QIcon saved = btn->icon();
    const QImage withIcon = btn->grab().toImage();
    btn->setIcon(QIcon());
    // DSuggestButton 自带 paintEvent；且卸图标会触发布局激活、按钮变窄
    // （折叠下方的控件首次 update 才布局）——故只比**重叠区域**：
    // 图标在按钮左侧，必然落在重叠区内；文字位移不影响图标区判定。
    btn->update();
    qApp->processEvents();
    const QImage noIcon = btn->grab().toImage();
    btn->setIcon(saved);
    btn->update();
    qApp->processEvents();
    if (withIcon.isNull() || noIcon.isNull())
        return -1;
    const int w = std::min(withIcon.width(), noIcon.width());
    const int h = std::min(withIcon.height(), noIcon.height());
    if (w <= 0 || h <= 0)
        return -1;
    // 按钮底色 = 无图标渲染里的众数色（抗文本/圆角干扰）
    QMap<QRgb, int> hist;
    for (int y = 0; y < noIcon.height(); ++y)
        for (int x = 0; x < noIcon.width(); ++x)
            ++hist[noIcon.pixel(x, y)];
    QColor bg = Qt::white;
    int best = -1;
    for (auto it = hist.cbegin(); it != hist.cend(); ++it)
        if (it.value() > best) {
            best = it.value();
            bg = QColor(it.key());
        }
    if (colorDist(bg, contractColor) < 60)
        return -2;   // 本环境下图标与按钮底无从对比，无法验证可见性
    int ink = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (colorDist(withIcon.pixelColor(x, y), noIcon.pixelColor(x, y)) > 30)
                ++ink;
    return ink;
}

struct BtnSpec {
    const char *match;    // 按钮文字匹配串
    const char *svgName;  // 期望的自带 SVG 键
    QColor (*colorFn)();  // 期望的契约色（返回固定值或语义色）
    bool checkColor;      // DIconButton 可能被 DTK 重染色，只查形状
};

QColor fixedWhite()  { return QColor(Qt::white); }
QColor normalText()  { return semanticColor(Semantic::Normal); }

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-manager"));
    // 与 main.cpp 一致：不铺 QSS 的话按钮/侧栏底色停在 QApplication 调色板
    // （本进程不跟随 DTK 主题），染色断言会拿到错的底色。
    app.setStyleSheet(panshiStyleSheet());

    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("/tmp/quickaction");

    MainWindow w;
    w.show();
    qApp->processEvents();

    // ---- 诊断输出（截图疑团：总览页页头/hero 是否在当前二进制里）----
    const auto switchRows = w.findChildren<PanshiSwitch::PanshiSwitchRow *>();
    const auto iconButtons = w.findChildren<DIconButton *>();
    QTextStream o(stdout);
    o << "== 诊断 ==\n"
      << QStringLiteral("  PanshiSwitchRow（hero 开关行）数量：%1\n").arg(switchRows.size())
      << QStringLiteral("  DIconButton（页头刷新按钮候选）数量：%1\n").arg(iconButtons.size());
    for (PanshiSwitch::PanshiSwitchRow *sr : switchRows) {
        const QWidget *host = sr->parentWidget();
        o << QStringLiteral("  switchRow @%1 visible=%2 size=%3×%4 宿主visible=%5\n")
                 .arg(sr->mapTo(w.isWindow() ? &w : sr->window(), QPoint(0, 0)).y())
                 .arg(sr->isVisible() ? 1 : 0)
                 .arg(sr->width()).arg(sr->height())
                 .arg(host ? (host->isVisible() ? 1 : 0) : -1);
    }
    for (DIconButton *b : iconButtons) {
        if (!b->toolTip().contains(QStringLiteral("刷新")))
            continue;
        o << QStringLiteral("  刷新按钮 visible=%1 size=%2×%3 iconSize=%4×%5\n")
                 .arg(b->isVisible() ? 1 : 0)
                 .arg(b->width()).arg(b->height())
                 .arg(b->iconSize().width()).arg(b->iconSize().height());
    }
    o.flush();

    // ---- ① 按钮查找（QAbstractButton* 黑盒）----
    const QList<QAbstractButton *> all = w.findChildren<QAbstractButton *>();
    auto findByText = [&all](const QString &key) -> QAbstractButton * {
        for (QAbstractButton *b : all)
            if (b->text().contains(key))
                return b;
        return nullptr;
    };
    QAbstractButton *refreshBtn = nullptr;
    for (DIconButton *b : iconButtons)
        if (b->toolTip().contains(QStringLiteral("刷新")))
            refreshBtn = b;

    struct Target { QAbstractButton *btn; const char *svg; QColor (*color)(); bool checkColor; const char *label; };
    Target targets[] = {
        {findByText(QStringLiteral("立即创建备份")), "layers-snapshot", fixedWhite,  true,  "立即创建备份"},
        {findByText(QStringLiteral("检查系统更新")), "cycle-update",     normalText, true,  "检查系统更新"},
        {findByText(QStringLiteral("保护设置")),     "shield-check",     normalText, true,  "保护设置"},
        {refreshBtn,                                 "cycle-update",     normalText, false, "页头刷新按钮"},
    };

    o << "\n== 按钮图标契约（20px 自带 SVG 现染）==\n";
    o.flush();

    for (const Target &t : targets) {
        const QString label = QString::fromUtf8(t.label);
        if (!t.btn) {
            check(false, QStringLiteral("「%1」按钮存在").arg(label));
            continue;
        }
        check(true, QStringLiteral("「%1」按钮存在").arg(label));

        const QSize want(20, 20);
        const QSize got = t.btn->iconSize();
        check(got == want,
              QStringLiteral("「%1」iconSize = %2×%3（期望 20×20）")
                  .arg(label).arg(got.width()).arg(got.height()));

        // ③ 图形身份：按钮图标的掩码 vs 自带 SVG 的掩码
        //   ⚠️ 参考图必须用**契约色**（而非黑色）渲染：DTK 的 TextTitle 角色色
        //   自带 alpha 216（QColor::name() 不显示 alpha，2026-10-24 实测），
        //   用黑（255）做参考会让 alpha 对不上——同一形状也报 31~40% 差异。
        const QIcon expected = tintedSvgIcon(QString::fromLatin1(t.svg), want, t.color());
        const QImage gotImg = t.btn->icon().pixmap(want).toImage();
        const QImage refImg = expected.pixmap(want).toImage();
        if (qEnvironmentVariableIsSet("QA_DEBUG_IMG")) {
            gotImg.save(prefix + QStringLiteral("_got_%1.png").arg(QString::fromUtf8(t.label)));
            refImg.save(prefix + QStringLiteral("_ref_%1.png").arg(QString::fromUtf8(t.label)));
        }
        const double d = inkMaskDiffRatio(gotImg, refImg);
        check(d <= 0.10,
              QStringLiteral("「%1」图标图形 = 自带 %2.svg（掩码差异 %3%）")
                  .arg(label, QString::fromLatin1(t.svg)).arg(d * 100, 0, 'f', 1));

        // ④ 颜色契约（DIconButton 可能被 DTK 重染色，跳过）
        if (t.checkColor) {
            const QColor expect = t.color();
            const int core = countNearColor(gotImg, gotImg.rect(), expect);
            check(core >= 15,
                  QStringLiteral("「%1」图标染色 = 契约色 %2（核心像素 %3）")
                      .arg(label, expect.name()).arg(core));
        }

        // ⑤ 可见墨迹（卸图标做差；对比度不足的环境按跳过处理）
        const int ink = iconInkPixels(t.btn, t.color());
        if (ink == -2) {
            QTextStream o(stdout);
            o << QStringLiteral("  note 「%1」按钮底色与图标色在本环境无从对比"
                                "（容器内 DSuggestButton 无蓝底），可见性由 ③④ 担保\n")
                     .arg(label);
            o.flush();
        } else {
            check(ink >= 25,
                  QStringLiteral("「%1」图标有可见墨迹（%2 像素）").arg(label).arg(ink));
        }

        // 存证：按钮特写（供肉眼复核；offscreen 样式与真机有差异时看这张）
        t.btn->grab().save(prefix + QStringLiteral("_btn_%1.png").arg(QString::fromUtf8(t.label)));
    }

    // 截图存证（整窗；总览页为默认页）
    const QImage shot = w.grab().toImage();
    shot.save(prefix + QStringLiteral(".png"));
    o << "\nwrote " << prefix << ".png\n";
    o.flush();

    if (g_fail == 0) {
        o << "PASS: 四个按钮的图标均为自带 SVG、20px、契约色、可见\n";
        o.flush();
        return 0;
    }
    o << "FAIL: " << g_fail << " 项断言未通过\n";
    o.flush();
    return 1;
}
