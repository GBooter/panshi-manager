// SPDX-License-Identifier: GPL-3.0-or-later

#include "components.h"

#include <DConfig>
#include <DGuiApplicationHelper>
#include <DPlatformHandle>

#include <QWindow>

#include <QAbstractButton>
#include <QEasingCurve>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QList>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointer>
#include <QProcess>
#include <QPropertyAnimation>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVariant>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace PanshiUi {

namespace {

/**
 * L1 骨架屏的脉冲步数：一个明暗周期由几步构成。
 *
 * 写成具名 constexpr（而不是内联的 `PulsePeriod / PulseStep` 整数除法）：
 *  1. 表达「这两个 token 相除得到的是步数」这一意图，读代码不必心算；
 *  2. 消掉 `-Warithmetic` 对整数除法的诊断（阶段 5 复测附带发现）。
 * 两 token 均为 int 常量，除法在编译期完成。
 */
constexpr int pulseSteps()
{
    return Motion::PulsePeriod / Motion::PulseStep;
}

/** 把带透明度的 over 叠在 under 上，得到不透明的混合色 */
QColor alphaBlend(const QColor &under, const QColor &over)
{
    if (over.alpha() >= 255)
        return over;
    if (over.alpha() <= 0)
        return under;
    const int a = over.alpha();
    const int ia = 255 - a;
    return QColor((over.red() * a + under.red() * ia) / 255,
                  (over.green() * a + under.green() * ia) / 255,
                  (over.blue() * a + under.blue() * ia) / 255, 255);
}

void fillRounded(QPainter *p, const QRectF &rect, qreal radius, const QColor &c)
{
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(c);
    p->drawRoundedRect(rect, radius, radius);
}

/**
 * 对话框可见性守卫：Show 时冻结主窗口内容区、Hide 时恢复。
 *
 * ⚠️ 为什么不能「在 makeDialogNonBlockingWindowModal 调用时立即冻结」：
 *   各页面在启动时即被整体构造（QStackedWidget 全量 new），磁盘空间页的
 *   设置对话框就是在构造期创建的 —— 若在构造时冻结，对话框「只构造不显示」
 *   则冻结永远不会被解除，整个内容区永久点不动（2026-10-06 用户报
 *   「软件里所有内容都没法点击」的根因）。故冻结必须挂到 Show 事件上。
 *
 * 用事件过滤而非 `QDialog::finished`：参数只需 `QWidget*`，对 DDialog
 * （继承 DAbstractDialog）与 PanshiCountdownDialog 等**并非 QDialog 子类**的
 * DTK 弹窗同样成立 —— 它们没有 `finished` 信号。
 * Show/Hide 是所有 QWidget 都有的判据，统一、不漏。
 *
 * 生命周期：`installEventFilter` 不会接管对象寿命，故本守卫把自己
 * 挂在被监视的 dialog 上（setParent(dialog)），随 dialog 一起销毁。
 */
class DialogFreezeGuard : public QObject
{
public:
    explicit DialogFreezeGuard(std::function<void()> freeze,
                               std::function<void()> restore,
                               QObject *parent = nullptr)
        : QObject(parent), m_freeze(std::move(freeze)), m_restore(std::move(restore))
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *e) override
    {
        if (e->type() == QEvent::Show) {
            m_freeze();          // setEnabled(false) 幂等，重复显示可重复冻结
        } else if (e->type() == QEvent::Hide) {
            m_restore();         // setEnabled(true) 幂等，重复关闭可重复恢复
        }
        return QObject::eventFilter(watched, e);
    }

private:
    std::function<void()> m_freeze;
    std::function<void()> m_restore;
};

DFontSizeManager::SizeType sizeTypeFor(TextLevel lv){
    switch (lv) {
    case TextLevel::PageTitle:
    case TextLevel::Value:
        return DFontSizeManager::T2;
    case TextLevel::SectionTitle:
    case TextLevel::Body:
        return DFontSizeManager::T5;
    case TextLevel::ValueSmall:
        return DFontSizeManager::T4;
    case TextLevel::Desc:
        return DFontSizeManager::T6;
    case TextLevel::Aux:
        return DFontSizeManager::T7;
    case TextLevel::Mono:
        return DFontSizeManager::T8;
    }
    return DFontSizeManager::T6;
}

/** 语义 → 提示图标名（成功/警示/错误靠形状区分，不依赖颜色） */
QString bannerIconName(Semantic s)
{
    switch (s) {
    case Semantic::Success:
        return QStringLiteral("dialog-ok");
    case Semantic::Warning:
        return QStringLiteral("dialog-warning");
    case Semantic::Danger:
        return QStringLiteral("dialog-error");
    default:
        return QStringLiteral("dialog-information");
    }
}

QStyle::StandardPixmap bannerIconFallback(Semantic s)
{
    switch (s) {
    case Semantic::Success:
        return QStyle::SP_DialogApplyButton;
    case Semantic::Warning:
        return QStyle::SP_MessageBoxWarning;
    case Semantic::Danger:
        return QStyle::SP_MessageBoxCritical;
    default:
        return QStyle::SP_MessageBoxInformation;
    }
}

} // namespace

// ---------------------------------------------------------------- 使用率分档
Semantic usageSemanticForPct(double pct)
{
    // 与 diskusagepage.cpp::updateOverview 同源（阈值常量 kUsageWarnPct 在此唯一定义）。
    // ≥75% 与 ≥90% 两档**同语义**（都是 Warning）：V-23 决策 —— 复用 TextWarning
    // 后不再区分，两档差异由图标与文案承载，不靠颜色。
    return (pct >= kUsageWarnPct) ? Semantic::Warning : Semantic::Normal;
}

// ---------------------------------------------------------------- 颜色查询
DPalette appPalette()
{
    return DGuiApplicationHelper::instance()->applicationPalette();
}

// ---------------------------------------------------------------- 成功 / 安全色
//
// ⚠️ 下面两个 hex 是**全项目仅有的「非 DPalette 来源色」**，与重构方案 §2.3
//    「语义收敛：强调色跟随用户主题色，不再自备绿色」**有意相反** —— 2026-10-05 改回。
//    `Semantic::Accent` 仍走 DPalette::Highlight（蓝 = 可交互/选中，语义正确，不动）。
//
// 为什么 Success 必须自备绿色（三条实测依据，不是凭偏好）：
//
//   ① **语义正确**：Success 的 11 处用法**全部**是「操作成功 / 正常运行 / 默认安全态」
//      （成功横幅、监控服务正常、检查完成、磐石开关正常、状态卡常态值）。
//      绿色是「安全、无需动作」的通用信号；主题蓝在暗色下偏「可交互 / 选中」，
//      会诱发用户「这里是不是能点」的冲动 —— 恰是状态卡要避免的。
//   ② **暗色下主题蓝不合规**：实测 DPalette::Highlight 暗色取 #024cca，
//      对 Base #282828 只有 **2.01:1**，低于 WCAG 1.4.3 大字号 3:1 的门槛。
//      状态卡主值是 T2 大字号，正落在这一条上 —— 换成绿的候选值反而把缺陷修掉。
//   ③ **调色板里根本没有绿色**：实测 20 个角色（11 个 DPalette::ColorType + 9 个
//      QPalette 标准角色）× 亮暗两套主题，按色相 90~150 且饱和 ≥60 判定，
//      **无一命中**。所以「跟随主题」也不可能拿到绿，只能自备。
//
// 两档按底色明度切换，各自都过 WCAG（由 panshi-protectionprobe 的对比度断言守护）。
//   亮底 #00875a → 对 #ffffff 4.55:1
//   暗底 #00a05a → 对 #282828 4.34:1
// ⚠️ 暗底那档 4.34 **差一点点**到 4.5（Success 在状态卡里是 T2 大字号，门槛 3.0
//    本就够，但小字号用法（行状态副标题、aboutdialog 描述）要 4.5）。
//    故 successGreen() 再过一道 ensureContrast(…, 4.5) 沿明度补足 —— 与 Accent
//    同一套机制，色相/饱和度不变，仍是同一支绿。
// 对照：主题蓝 亮 3.76 / 暗 2.01
const QColor kSuccessGreenOnLight(QStringLiteral("#00875a"));
const QColor kSuccessGreenOnDark(QStringLiteral("#00a05a"));
constexpr int kGreenBaseLightnessSplit = 128;

// successGreen() 要用 ensureContrast() 补足暗底对比度，而后者定义在下方
// （它依赖 contrastOf）。前置声明，避免为了顺序去重排这两个函数 ——
// 重排会把 ensureContrast 那段「二分方向必须相反」的重要注释推离它所属的代码。
static QColor ensureContrast(QColor fg, const QColor &bg, double minRatio);

/**
 * Success 绿 —— 含**开发期取色口子** `PANSHI_SUCCESS_GREEN=#RRGGBB`。
 *
 * 该变量存在是因为「绿的饱和度/明度偏好」是主观的，而看色块猜不准：得用**真实控件**
 * 在**真实底色**上渲染出来才能判断（同一套做法见 `PANSHI_CONFIG_QSETTINGS` 诊断开关）。
 * 生产环境不设此变量 ⇒ 行为与硬编码完全一致，故不构成风险。
 */
static QColor successGreen()
{
    const QByteArray ov = qgetenv("PANSHI_SUCCESS_GREEN");
    if (ov.size() == 7) {
        const QColor c(QString::fromLatin1(ov));
        if (c.isValid())
            return c;
    }
    // ⚠️ 必须用 applicationPalette()（DTK 的 DPalette），**不能**用
    //    QGuiApplication::palette()（2026-10-05 暗色故障实测）。
    //    后者在本进程里不跟随 DTK 主题切换：暗色下它仍返回亮色的 Base #ffffff，
    //    于是这里判成「亮底」→ 用了亮色档的 #00875a，压在暗色卡片底 #282828 上
    //    只有 3.24:1（实测），比暗色档 #00a05a 的 4.34:1 差一截。
    //    这与 applySemantic 踩的是同一个坑：凡「判断当前主题」的地方，
    //    都必须走 applicationPalette()，不能走 QGuiApplication::palette()。
    const QColor base = appPalette().color(DPalette::Base);
    const QColor green = base.lightness() < kGreenBaseLightnessSplit
                             ? kSuccessGreenOnDark
                             : kSuccessGreenOnLight;
    // 暗底那档 #00a05a 对 #282828 只有 4.34:1（小字号差一口气），
    // 沿明度补足到 4.5 —— 与 Accent 走同一个 ensureContrast，色相不变。
    return ensureContrast(green, base, 4.5);
}

/** WCAG 2.1 相对亮度（与 tools/probe_protection_card.cpp 里的同名函数刻意各写一份：
    那边要独立复算、不想依赖生产代码，才守得住「实测优于信任」）。 */
static double relLum(const QColor &c)
{
    auto f = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * f(c.red()) + 0.7152 * f(c.green()) + 0.0722 * f(c.blue());
}

static double contrastOf(const QColor &a, const QColor &b)
{
    const double la = relLum(a), lb = relLum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

/**
 * 保持**色相与饱和度**、只沿明度调整，直到与背景的对比度达标。
 *
 * 用途：`Semantic::Accent`（强调 / 可交互 / 品牌色）。§2.3 的「跟随用户主题色」原则要保住
 * —— 那是**用户自己挑的颜色**，不能换成别的色相。但用户挑的色可能不达标，实测 deepin
 * 默认主题 `Highlight` 在**暗色下只有 2.03:1**（对 Base #282828），而 Accent 被当作
 * T2 大字号数值用在磁盘页 4 张汇总卡上；`aboutdialog.cpp` 还用 `TextLevel::Desc`
 * （小字号，门槛 4.5:1）——亮色 3.76、暗色 2.03 **双双不达标**。
 *
 * 所以不是改色相（那会丢掉「这是我的主题色」），而是**压暗/提亮到刚好达标**：
 * 亮底压暗、暗底提亮，色相不变，用户认得出是自己选的颜色。
 */
static QColor ensureContrast(QColor fg, const QColor &bg, double minRatio)
{
    // ⚠️ 留一点余量：理论值刚好卡在门槛上时，**抗锯齿后的实测核心像素会略低**
    //    （实测理论 4.53:1 → 采样 4.38:1），差这一点就够判 FAIL。
    //    余量只在内部目标上放大，断言仍按真实门槛（4.5）判。
    constexpr double kMargin = 0.3;
    const double target = minRatio + kMargin;

    if (contrastOf(fg, bg) >= target)
        return fg;
    const bool onDarkBg = bg.lightness() < kGreenBaseLightnessSplit;
    // ⚠️ `getHslF()` 要的是 **float***，用 qreal(=double) 接收会写坏一半字节
    //    （实测 hsl 三元全 0，二分随即收敛到纯黑 —— 「能编过 ≠ 写对了」）。
    float hf = 0.f, sf = 0.f, lf = 0.f;
    fg.getHslF(&hf, &sf, &lf);
    const qreal h = hf, s = sf, l = lf;

    // 在 HSL 明度上二分，**色相与饱和度保持不变** ⇒ 用户仍认得出是自己选的主题色。
    //
    // ⚠️ 两边的二分方向**必须相反**：
    //   亮底：L 越大对比度越低 ⇒ 取「最大的达标 L」，达标就把**下界**往上抬；
    //   暗底：L 越大对比度越高 ⇒ 取「最小的达标 L」，达标就把**上界**往下压。
    // 早先两边共用一套更新规则，亮色底一路收敛到纯黑 #000000（contrast 21，能过但
    // 把强调色变成了黑，等于丢了品牌色）。
    // ⚠️ 另：**不要用 `QColor::darker()/lighter()` 迭代** —— 实测对 #0081ff 连续
    //    darker(5)×60 会收敛到 **#ffffff**（contrast 1），与直觉完全相反。
    const bool keepUpperOnPass = onDarkBg;
    qreal lo = onDarkBg ? l : 0.0;
    qreal hi = onDarkBg ? 1.0 : l;
    for (int i = 0; i < 40; ++i) {
        const qreal mid = (lo + hi) / 2.0;
        const bool pass = contrastOf(QColor::fromHslF(h, s, mid), bg) >= target;
        if (pass == keepUpperOnPass)
            hi = mid;
        else
            lo = mid;
    }
    QColor out = QColor::fromHslF(h, s, keepUpperOnPass ? hi : lo);
    out.setAlpha(fg.alpha());
    return out;
}

QColor semanticColor(Semantic s)
{
    const DPalette pa = appPalette();
    switch (s) {
    case Semantic::Muted:
        return pa.color(DPalette::TextTips);
    case Semantic::Accent:
        // 强调色 = 可交互 / 选中 / 品牌色，**色相跟随用户主题色**（§2.3 原则保留），
        // 但沿明度拉到 WCAG 4.5:1 —— 用户挑的色可能不达标（实测默认主题暗色 2.03:1），
        // 而 Accent 被当 T2 大字号数值 + aboutdialog 的小字号文字用，两处都硬性要求达标。
        // 门槛取 4.5（较严的一档）以同时满足大小字号。理由见 ensureContrast 注释。
        return ensureContrast(pa.color(DPalette::Highlight),
                              pa.color(DPalette::Base), 4.5);
    case Semantic::Success:
        // 成功 / 安全 / 默认正常态 = 绿（不跟随主题色，理由见上方 kSuccessGreenOn* 注释）
        return successGreen();
    case Semantic::Warning:
        return pa.color(DPalette::TextWarning);
    case Semantic::Danger:
        // ⚠️ 曾用 DPalette::TextLively，**实测其默认主题取值是中性黑**
        // （2026-10-03 V-23 调色板探针实证，非降级环境的灰色）——
        //   危险语义因此完全失效：Banner 错误态、状态卡 Danger 徽章、
        //   磁盘页 ≥90% 的「已用」大数值都渲染成普通正文黑。
        //   根因是角色语义错配：DTK 里 `TextLively` 是「活跃式文本
        //   （不受活跃色影响）」，本就不是危险色。
        //
        // 修法：Danger 复用 `TextWarning` —— DTK 真实存在、跟随主题、
        //   亮暗自适应，零硬编码。代价是 Danger 与 Warning 同色，
        //   但两级语义仍由**图标 + 文案 + 控件类型**区分
        //   （dialog-error / dialog-warning，见 bannerIconName），
        //   WCAG 1.4.1「颜色非唯一载体」依然成立。
        return pa.color(DPalette::TextWarning);
    default:
        return pa.color(DPalette::TextTitle);
    }
}

QColor baseColor()
{
    return appPalette().color(DPalette::Base);
}

QColor windowColor()
{
    return appPalette().color(DPalette::Window);
}

QColor itemBackground()
{
    return appPalette().color(DPalette::ItemBackground);
}

QColor borderColor()
{
    return appPalette().color(DPalette::FrameBorder);
}

QColor softFill(Semantic s, int alpha)
{
    QColor fg = semanticColor(s);
    fg.setAlpha(alpha);
    return alphaBlend(baseColor(), fg);
}

// ---------------------------------------------------------------- 图标 / 字体
QIcon themedIcon(const QString &name, QStyle::StandardPixmap fallback)
{
    const QIcon ic = DIconTheme::findQIcon(name);
    if (!ic.isNull())
        return ic;
    return QApplication::style()->standardIcon(fallback);
}

QPixmap tintedPixmap(const QIcon &ic, const QSize &size, const QColor &color)
{
    QPixmap px = ic.pixmap(size);
    if (px.isNull())
        return px;
    QPainter tp(&px);
    tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tp.fillRect(px.rect(), color);
    tp.end();
    return px;
}

QIcon tintedSvgIcon(const QString &name, const QSize &size, const QColor &color)
{
    const QIcon src(QStringLiteral(":/icons/nav/%1.svg").arg(name));
    const QPixmap px = tintedPixmap(src, size, color);
    // ⚠️ QIcon(QPixmap) 只注册 (Normal, Off) 一个条目；DSuggestButton 的
    // 绘制路径会取 (Selected, On) 之类的组合，取不到就**整图不画**
    // （2026-10-24 实测：同图标的 DPushButton 正常、DSuggestButton 完全
    // 不显示）。四个模式 × 两个状态都补上，任何绘制路径都有图可取。
    QIcon ic;
    const QList<QPair<QIcon::Mode, QIcon::State>> states = {
        {QIcon::Normal, QIcon::Off},   {QIcon::Normal, QIcon::On},
        {QIcon::Active, QIcon::Off},   {QIcon::Active, QIcon::On},
        {QIcon::Selected, QIcon::Off}, {QIcon::Selected, QIcon::On},
        {QIcon::Disabled, QIcon::Off}, {QIcon::Disabled, QIcon::On},
    };
    for (const auto &s : states)
        ic.addPixmap(px, s.first, s.second);
    return ic;
}

/**
 * 把**任意颜色**（非 DPalette 角色）落到 DLabel 上。
 *
 * ⚠️ 必须同时做两件事，缺一不可：
 *   ① 写控件调色板的 WindowText / Text；
 *   ② **把 foreground 角色也切到 WindowText** —— DLabel 内部记着此前
 *      `setForegroundRole()` 设过的角色（状态卡里 `makeLabel` 先给 Normal 态设了
 *      `TextTitle`），它收到 `PaletteChange` 会**重新套用那个旧角色**，把直接写进
 *      调色板的颜色覆盖成 `#272727` 近黑。实测：独立 DLabel 正常、真实状态卡发黑，
 *      差别就在这个角色残留。
 *
 * Accent 与 Success 两处都走这里 —— 它们是仅有的两个「非 DPalette 角色」语义，
 * 各自写一份必然分叉（本次就先分叉过一次：Accent 漏改，探针当场 FAIL 才抓到）。
 */
static void applyExactColor(DLabel *l, const QColor &c)
{
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, c);
    p.setColor(QPalette::Text, c);         // 兜底：部分样式走 Text 角色
    l->setPalette(p);
    l->setForegroundRole(QPalette::WindowText);
}

/**
 * 主题切换后重刷所有落色标签。
 *
 * ⚠️ 为什么需要它（2026-10-05 暗色故障实测，勿删）：
 *   DLabel 的字色走 applyExactColor —— 它把颜色**快照**进控件自己的 QPalette。
 *   而 DLabel 收到 PaletteChange 时会重走 initPainter()，只有 foregroundRole
 *   指向 WindowText 时才会保留这份快照。于是切主题后必须**主动重算一遍**，
 *   否则标签会留着旧主题的字色（亮→暗后是黑字压深底，对比 1.4，肉眼等于消失）。
 *
 *   为什么不能靠 `applicationPaletteChanged` + unpolish/polish 自动解决：
 *   那套只解决「控件自绘」（本文件里用 QPainter 直接取 applicationPalette() 的
 *   地方），而 DLabel 的字色是 Qt 样式/QPainter 管线按**角色**解析的，
 *   角色 → 颜色的映射由 DLabel 私有实现决定，外部无法干预，只能重算。
 *
 * 做法：登记所有落色标签，主题一变就按当前 applicationPalette() 重算。
 * 用 QPointer 兜住控件中途被销毁的情况。
 */
Q_GLOBAL_STATIC(QList<QPointer<DLabel>>, g_exactColorLabels)

void refreshExactColorLabels()
{
    const auto labels = *g_exactColorLabels;
    for (const QPointer<DLabel> &lp : labels) {
        DLabel *l = lp.data();
        if (!l)
            continue;
        const QVariant v = l->property("panshiSemantic");
        if (v.isValid())
            applySemantic(l, static_cast<Semantic>(v.toInt()));
    }
}

/**
 * 给标签落色：**一律走「算出来的具体色」**，不再用 setForegroundRole(DPalette::…)。
 *
 * ⚠️ 这是暗色故障的核心修法（2026-10-05 实测，勿改回角色写法）：
 *
 *   DLabel 收到 PaletteChange 时会拿 foregroundRole 去**控件自己的 QPalette**
 *   里解析颜色。而控件的 QPalette 继承自 QApplication::palette()，后者在
 *   「只有 applicationPalette() 变了」的情况下**不会跟着变**（实测：
 *   setPaletteType(Dark) 后 dpal.Base=#282828 而 qpal.Base 仍是 #ffffff）。
 *   于是角色解析出的 TextTitle 是**亮色主题的近黑 #000000**，
 *   压在同一张深色卡片底 #282828 上 → 对比度 1.4，字「消失」了。
 *
 *   改成「按当前 applicationPalette() 算出具体色再落进去」之后，
 *   字色与卡片底**必然同源同主题**，不再依赖 QApplication 调色板是否同步。
 *   代价是失去「角色自动跟随」的 laziness —— 用下面的登记表补回来：
 *   主题一变就整体重算一遍。
 *
 *   之前只有 Accent / Success 走落色（因为它们是 ensureContrast 算出来的、
 *   本来就不是原始角色值）；现在**全部语义统一走落色**，消掉「两套写法」
 *   这一长期分叉源。
 */
void applySemantic(DLabel *l, Semantic s)
{
    // 记住语义，供 refreshExactColorLabels() 在主题切换后重算。
    l->setProperty("panshiSemantic", static_cast<int>(s));
    applyExactColor(l, semanticColor(s));

    if (!g_exactColorLabels->contains(l))
        g_exactColorLabels->append(l);
}

DLabel *makeLabel(const QString &text, TextLevel lv, Semantic s)
{
    auto *l = new DLabel(text);
    DFontSizeManager::instance()->bind(l, sizeTypeFor(lv));
    applySemantic(l, s);
    if (lv == TextLevel::Desc || lv == TextLevel::Mono)
        l->setWordWrap(true);
    return l;
}

bool reduceMotionRequested()
{
    static int cached = -1;
    if (cached >= 0)
        return cached == 1;

    // ⚠️ 这里是 **GSettings 键，不是 DConfig 键**（缺陷 A2-3 第二次定位，2026-10-05）。
    //
    // 原实现用 DConfig 读 `com.deepin.wrap.gnome.desktop.interface /
    // enable-animations`，而该键只存在于 glib gschema
    // （/usr/share/glib-2.0/schemas/com.deepin.wrap.gnome.desktop.interface.gschema.xml，
    //  type=b，默认 true）。DSG 的查找路径 /usr/share/dsg/configs/ 下**没有**它 →
    //  dde-dconfig 实测报 `not exist resouce` → 旧实现恒读到缺省值，
    //  「减少动态效果」对本应用**完全无效**，且无任何编译告警。
    //
    // 为什么不用 Dtk::Gui::DGuiApplicationHelper::HasAnimations（2026-10-05 实测）：
    //  它的语义是「DTK 自身动画是否启用」，来源是环境变量 D_DTK_DISABLE_ANIMATIONS
    //  与 DConfig `org.deepin.dtk.preference` 的 enableDtkAnimations，
    //  **不读** GNOME 兼容键。实测（gsettings 为 true 时）它返回 false，
    //  因为该 DConfig schema 在本机未安装 → 若拿它当判据会把动画全部误关。
    //
    // 为什么不用 GSettings C API：本项目工具链无 glib-2.0/gio-2.0 开发包，
    //  引入会新增构建依赖。改用系统自带的 gsettings 命令读取
    //  （deepin 必带 glib），且本函数进程内只执行一次并缓存，开销可忽略。
    //
    // 读不到时的取值：保持动画开启（reduce=false）。宁可少优化也不能
    //  在探测失败时把全应用动效关掉 —— 静默降级比不降级更糟。
    bool reduce = false;
    QProcess proc;
    proc.start(QStringLiteral("gsettings"),
               {QStringLiteral("get"),
                QStringLiteral("com.deepin.wrap.gnome.desktop.interface"),
                QStringLiteral("enable-animations")});
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        const QString v = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (v == QLatin1String("true") || v == QLatin1String("false")) {
            reduce = (v == QLatin1String("false"));
        } else {
            qWarning() << "reduceMotionRequested: unexpected gsettings output" << v
                       << "-keeping animations enabled";
        }
    } else {
        qWarning() << "reduceMotionRequested: gsettings query failed (exit"
                   << proc.exitCode() << ")- keeping animations enabled";
    }

    cached = reduce ? 1 : 0;
    return reduce;
}

// ---------------------------------------------------------------- PanshiPanel
PanshiPanel::PanshiPanel(QWidget *parent, Level level)
    : QWidget(parent)
    , m_level(level)
{
    m_body = new QVBoxLayout(this);
    m_body->setContentsMargins(Space::L, Space::M, Space::L, Space::M);
    m_body->setSpacing(Space::M);
}

void PanshiPanel::paintEvent(QPaintEvent *)
{
    if (m_level == Level::Flat)
        return;

    QPainter p(this);
    const QColor c = (m_level == Level::Surface) ? baseColor() : itemBackground();
    fillRounded(&p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Radius::Frame, c);
}

// ---------------------------------------------------------------- PanshiSection
PanshiSection::PanshiSection(const QString &title, const QString &desc, QWidget *parent)
    : PanshiPanel(parent, Level::Surface)
{
    m_headerRow = new QHBoxLayout();
    m_headerRow->setSpacing(Space::S);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(Space::XS);
    auto *t = makeLabel(title, TextLevel::SectionTitle, Semantic::Normal);
    titles->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = makeLabel(desc, TextLevel::Desc, Semantic::Muted);
        titles->addWidget(d);
    }
    m_headerRow->addLayout(titles, 1);
    body()->addLayout(m_headerRow);
}

void PanshiSection::addHeaderWidget(QWidget *w)
{
    m_headerRow->addWidget(w, 0, Qt::AlignRight | Qt::AlignVCenter);
}

// ---------------------------------------------------------------- PanshiRow
PanshiRow::PanshiRow(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::L);

    auto *texts = new QVBoxLayout();
    texts->setSpacing(Space::XS);
    m_title = makeLabel(QString(), TextLevel::Body, Semantic::Normal);
    m_desc = makeLabel(QString(), TextLevel::Desc, Semantic::Muted);
    m_desc->setWordWrap(true);
    m_desc->hide();   // 未设置说明时不占行高
    texts->addWidget(m_title);
    texts->addWidget(m_desc);
    // 文本列承担拉伸 —— 空间宽裕时它吸收多余宽度
    lay->addLayout(texts, 1);

    // 控件列给「显式最小宽度」，保证挤压时它们不会先塌。
    //
    // 背景：本类此前只 addLayout(m_controls) 不设任何约束。空间不足时
    // Qt 会把所有子项按比例压到各自 minimumSizeHint —— DSpinBox / DComboBox
    // 的 minimumSizeHint 很小（几十像素），于是一排 spinbox 被压成「…」甚至
    // 2px 竖缝，当前值完全不可读（磁盘空间页设置窗口实测）。
    // 而左侧说明因 wordWrap=true，sizeHint 巨大，反而"抢"走了宽度。
    //
    // 与表格列宽同一课：**显式最小宽度是布局挤压下唯一可靠的防线**
    // （参见 SnapshotsPage 的 setMinimumWidth / QSizePolicy::Ignored 处置）。
    //
    // 下限由 addControl() 逐个累积（取最大值），构造时先设 0：
    // 交互控件会给 200，纯文本值给 0 —— 见 addControl 的说明与 components.h 类注释。
    m_controlsHost = new QWidget(this);
    m_controlsHost->setMinimumWidth(0);
    m_controlsHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *hostLay = new QHBoxLayout(m_controlsHost);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(0);
    m_controls = new QHBoxLayout();
    m_controls->setSpacing(Space::S);
    // 内层无 stretch：让 addStretch() 决定是否把控件推到一端/撑满
    hostLay->addLayout(m_controls, 1);
    lay->addWidget(m_controlsHost, 0, Qt::AlignVCenter);

    setMinimumHeight(Size::RowMinHeight);
}

void PanshiRow::setTitle(const QString &title)
{
    m_title->setText(title);
    // O9：行标题是右侧交互控件的天然标签，回填为无障碍名称，
    // 让读屏念得出「已用空间：<输入控件>」而非沉默的 spinbox/combobox。
    // 只回填尚未显式设置名称的控件，避免覆盖页面侧的专属命名。
    // 用 const 引用遍历而非 qAsConst()：后者在 Qt 6.8 已标记 deprecated。
    for (QWidget *w : m_rowControls) {
        if (w && w->accessibleName().isEmpty() && !title.isEmpty())
            w->setAccessibleName(title);
    }
}

void PanshiRow::setDescription(const QString &desc)
{
    m_desc->setVisible(!desc.isEmpty());
    m_desc->setText(desc);
}

void PanshiRow::addControl(QWidget *w, int minW)
{
    if (!w)
        return;
    // 下限取「所有控件请求的最大值」，并且一经抬高不再回落——
    // 同一行里可能既有文本值（请求 0）又有交互控件（请求 200），
    // 后者才是决定性的：文本可以退化，控件不能。
    const int requested = (minW < 0) ? Size::RowControlMinWidth : minW;
    if (requested > m_controlMinWidth) {
        m_controlMinWidth = requested;
        m_controlsHost->setMinimumWidth(m_controlMinWidth);
    }
    m_controls->addWidget(w, 0, Qt::AlignVCenter);
    m_rowControls.append(w);
    // O9：若标题已先设置且本控件尚未命名，立即回填（覆盖 setTitle 早于 addControl 的调用顺序）。
    if (w->accessibleName().isEmpty() && !m_title->text().isEmpty())
        w->setAccessibleName(m_title->text());
}

void PanshiRow::addStretch(int stretch)
{
    m_controls->addStretch(stretch);
}

QString PanshiRow::title() const
{
    return m_title->text();
}

QString PanshiRow::description() const
{
    return m_desc->isVisible() ? m_desc->text() : QString();
}

void PanshiRow::flashHighlight()
{
    // 设置搜索命中后的落点提示：Accent 柔和底闪一下再淡出。
    // 设置页不是列表、没有"当前项"概念，故不做常驻选中态。
    const int peakMs = Motion::HighlightHold;
    const int fadeMs = Motion::HighlightFade;

    // 尊重系统"减少动态效果"：不做渐隐，直接显示后定时清除
    if (reduceMotionRequested()) {
        if (m_highlightAnim)
            m_highlightAnim->stop();
        m_highlight = 1.0;
        update();
        QTimer::singleShot(peakMs, this, [this] {
            m_highlight = 0.0;
            update();
        });
        return;
    }

    if (!m_highlightAnim) {
        m_highlightAnim = new QPropertyAnimation(this, "highlight", this);
    }
    m_highlightAnim->stop();
    m_highlightAnim->setDuration(fadeMs);
    m_highlightAnim->setStartValue(1.0);
    m_highlightAnim->setEndValue(0.0);
    m_highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_highlight = 1.0;
    update();

    // 先保持峰值，再启动淡出（复用同一动画对象，避免叠加）。
    // connect 必须只建一次且不能用 Qt::UniqueConnection：UniqueConnection 仅支持
    // 成员函数指针，对 lambda 会在运行时拒绝（实测告警"unique connections require
    // a pointer to member function"），导致 peak 后无人 start 动画、高亮卡在满亮。
    if (!m_highlightClear) {
        m_highlightClear = new QTimer(this);
        m_highlightClear->setSingleShot(true);
        QObject::connect(m_highlightClear, &QTimer::timeout, this,
                         [this] { m_highlightAnim->start(); });
    }
    m_highlightClear->stop();
    m_highlightClear->start(peakMs);
}

qreal PanshiRow::highlight() const
{
    return m_highlight;
}

void PanshiRow::setHighlight(qreal v)
{
    m_highlight = v;
    update();
}

void PanshiRow::paintEvent(QPaintEvent *e)
{
    if (m_highlight <= 0.0) {
        QWidget::paintEvent(e);
        return;
    }
    // 高亮底：Accent 柔和底按强度插值（自绘，不走 QSS）
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = softFill(Semantic::Accent);
    c.setAlphaF(c.alphaF() * m_highlight);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    // 略微内缩，避免与相邻行/分组边界粘连
    p.drawRoundedRect(QRectF(rect()).adjusted(-Space::S, 0, Space::S, 0),
                      Radius::Frame, Radius::Frame);
    QWidget::paintEvent(e);
}

// ---------------------------------------------------------------- PanshiBadge
PanshiBadge::PanshiBadge(const QString &text, Semantic s, QWidget *parent)
    : QWidget(parent)
    , m_semantic(s)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(Space::S, Space::XS, Space::S, Space::XS);
    lay->setSpacing(0);
    m_label = makeLabel(text, TextLevel::Aux, m_semantic);
    m_label->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_label);
    setMinimumHeight(Size::Icon);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
}

void PanshiBadge::setText(const QString &text)
{
    m_label->setText(text);
}

QString PanshiBadge::text() const
{
    return m_label->text();
}

void PanshiBadge::setSemantic(Semantic s)
{
    m_semantic = s;
    refresh();
}

void PanshiBadge::refresh()
{
    applySemantic(m_label, m_semantic);
    update();
}

void PanshiBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fillRounded(&p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Radius::Element,
                softFill(m_semantic));
}

// ---------------------------------------------------------------- PanshiInlineBanner
PanshiInlineBanner::PanshiInlineBanner(QWidget *parent)
    : QWidget(parent)
    , m_semantic(Semantic::Accent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(Space::M, Space::S, Space::S, Space::S);
    lay->setSpacing(Space::S);

    // 语义图标：承载"成功/提示/警告/错误"，是颜色之外的第二条信息通道
    m_icon = new DLabel(this);
    m_icon->setFixedSize(Size::Icon, Size::Icon);
    m_icon->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_icon, 0, Qt::AlignVCenter);

    // 正文 + 详情块共用一个竖排容器：默认只有 m_text（单行观感不变），
    // 调 setDetails() 后才把折叠块加到它下面（O11）
    m_textCol = new QWidget(this);
    m_textColLay = new QVBoxLayout(m_textCol);
    m_textColLay->setContentsMargins(0, 0, 0, 0);
    m_textColLay->setSpacing(Space::XS);

    m_text = makeLabel(QString(), TextLevel::Desc, Semantic::Normal);
    m_text->setWordWrap(true);
    m_textColLay->addWidget(m_text);
    lay->addWidget(m_textCol, 1);

    m_actions = new QWidget(this);
    m_actionLay = new QHBoxLayout(m_actions);
    m_actionLay->setContentsMargins(0, 0, 0, 0);
    m_actionLay->setSpacing(Space::S);
    m_actions->setVisible(false);
    lay->addWidget(m_actions, 0, Qt::AlignVCenter);

    m_close = new DPushButton(this);
    m_close->setIcon(themedIcon(QStringLiteral("dialog-close"), QStyle::SP_DialogCloseButton));
    m_close->setFlat(true);
    m_close->setFixedSize(Size::MinTouchTarget, Size::MinTouchTarget);
    m_close->setIconSize(QSize(Size::IconSmall, Size::IconSmall));
    m_close->setToolTip(QObject::tr("关闭"));
    m_close->setAccessibleName(QObject::tr("关闭提示"));
    m_close->hide();
    lay->addWidget(m_close, 0, Qt::AlignVCenter);
    // 先发信号再隐藏：调用方据此把这条告警记进「已关闭 / 告警历史」，
    // 与「内容清空后自动收起」区分开（详见 components.h::setClosable 注释）。
    QObject::connect(m_close, &DPushButton::clicked, this, [this] {
        Q_EMIT dismissed();
        hide();
    });

    refresh();   // 初始化图标与正文语义
}

void PanshiInlineBanner::setSemantic(Semantic s)
{
    m_semantic = s;
    refresh();
}

void PanshiInlineBanner::setText(const QString &text)
{
    // 换内容 = 换一次反馈：取消上一条待定的自动隐藏，
    // 否则旧定时器会把刚显示的新内容一起隐藏（阶段 4）
    if (!text.isEmpty() && m_dismissTimer)
        m_dismissTimer->stop();
    m_text->setText(text);
    setVisible(!text.isEmpty());
}

void PanshiInlineBanner::setClosable(bool closable)
{
    m_close->setVisible(closable);
}

DPushButton *PanshiInlineBanner::addAction(const QString &text, bool suggest)
{
    m_actions->setVisible(true);
    DPushButton *b = suggest ? new DSuggestButton(text) : new DPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    m_actionLay->addWidget(b);
    return b;
}

void PanshiInlineBanner::setDetails(const QString &title, const QString &rawText)
{
    if (!m_textColLay)
        return;
    // 空内容 = 回到单行形态。先摘掉旧块，避免复用同一 Banner 时详情叠加。
    if (m_details) {
        m_textColLay->removeWidget(m_details);
        m_details->deleteLater();
        m_details = nullptr;
    }
    if (rawText.isEmpty())
        return;

    auto *raw = makeLabel(rawText, TextLevel::Mono, Semantic::Muted);
    raw->setWordWrap(true);
    // 原始输出要能选中复制（排障时用户要拿这段文字去搜/提 issue）
    raw->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details = new PanshiCollapsible(title, m_textCol);
    m_details->setContent(raw);
    m_details->setExpanded(false);
    m_textColLay->addWidget(m_details);
}

void PanshiInlineBanner::autoDismiss(int ms)
{
    // 单发定时器只在首次调用时创建，之后 start() 会重置已有倒计时
    if (!m_dismissTimer) {
        m_dismissTimer = new QTimer(this);
        m_dismissTimer->setSingleShot(true);
        // 隐藏而非清空文本：保留内容，调用方再次 setText 仍可复用同一条 Banner
        QObject::connect(m_dismissTimer, &QTimer::timeout, this, [this] { hide(); });
    }
    m_dismissTimer->start(ms);
}

void PanshiInlineBanner::refresh()
{
    m_icon->setPixmap(themedIcon(bannerIconName(m_semantic), bannerIconFallback(m_semantic))
                          .pixmap(QSize(Size::Icon, Size::Icon)));
    // 正文恒为中性色：语义只由「图标 + 柔和底」表达，保证任何主题下都可读
    applySemantic(m_text, Semantic::Normal);
    update();
}

void PanshiInlineBanner::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fillRounded(&p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Radius::Frame,
                softFill(m_semantic, Alpha::BannerFill));
}

// ---------------------------------------------------------------- PanshiStateCard
PanshiStateCard::PanshiStateCard(const QString &title, QWidget *parent)
    : PanshiPanel(parent, Level::Surface)
{
    auto *lay = body();
    lay->setContentsMargins(Space::L, Space::M, Space::L, Space::M);
    lay->setSpacing(Space::XS);

    m_value = makeLabel(QStringLiteral("-"), TextLevel::Value, Semantic::Normal);
    m_value->setWordWrap(true);

    // ⚠️ 顺序即信息层级，勿改回「值 → 标签」（2026-10-05 修正，原实现是值在最顶）：
    // 扫视路径应是「先确认这张卡在讲什么 → 再读它的状态」。状态值字号最大(T2)且带
    // 语义色，浮在顶端时读者先撞上答案却不知道说的是「只读保护 / 可写层 / 重启后状态」，
    // 而功能名最小最灰、落在中间退化成副标题。
    // 正确层级：标签(这是什么，Muted 弱化) → 值(现在什么状态，唯一状态色) → 徽标(补充说明)。
    auto *titleLabel = makeLabel(title, TextLevel::Desc, Semantic::Muted);
    titleLabel->setWordWrap(true);
    lay->addWidget(titleLabel);
    lay->addWidget(m_value);

    m_status = new PanshiBadge(QString(), Semantic::Muted, this);
    m_status->hide();
    // 标签↔值 4px（Space::XS，同属一组）；值↔徽标 8px（补 Space::S-XS，拉开层次）。
    // 原先三段一律 4px，标签与值之间没有任何分组暗示。
    lay->addSpacing(Space::S - Space::XS);
    lay->addWidget(m_status, 0, Qt::AlignLeft);

    // 显式最小宽度：卡片行的防塌底线（见 tokens.h 的 StateCardMinWidth 说明）。
    // 横向 SizePolicy 保持 Expanding——卡片在宽裕时应撑满分配的格子，
    // 下限只作用于「空间不足时不许继续缩小」。
    setMinimumWidth(Size::StateCardMinWidth);
}

void PanshiStateCard::setValue(const QString &value, Semantic s, TextLevel level)
{
    m_value->setText(value);
    // 字号随内容性质重绑（默认 T2；日期类调用方传 ValueSmall→T4）。
    // 与 makeLabel 同一入口 DFontSizeManager::bind，重复 bind 为幂等替换。
    DFontSizeManager::instance()->bind(m_value, sizeTypeFor(level));
    applySemantic(m_value, s);
}

void PanshiStateCard::setStatus(const QString &status, Semantic s)
{
    m_status->setVisible(!status.isEmpty());
    m_status->setText(status);
    m_status->setSemantic(s);
}

// ---------------------------------------------------------------- PanshiResponsiveCardRow
PanshiResponsiveCardRow::PanshiResponsiveCardRow(int maxCols, QWidget *parent)
    : QWidget(parent)
    , m_maxCols(qMax(1, maxCols))
{
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(Space::M);
    m_grid->setVerticalSpacing(Space::M);
}

void PanshiResponsiveCardRow::addCard(QWidget *card)
{
    if (!card || m_cards.contains(card))
        return;
    m_cards.append(card);
    card->setParent(this);

    // 首帧：setAvailableWidth() 尚未调用过，先按「上限与实际卡数取小」排一版，
    // 避免卡片未入布局时出现零尺寸闪烁。真实列数由 setAvailableWidth() 决定。
    if (m_cols == 0) {
        const int c = qMin(m_maxCols, m_cards.size());
        m_cols = qMax(1, c);
        for (int i = 0; i < m_cards.size(); ++i)
            m_grid->addWidget(m_cards.at(i), i / m_cols, i % m_cols);
        for (int i = 0; i < m_maxCols; ++i)
            m_grid->setColumnStretch(i, i < m_cols ? 1 : 0);
    } else {
        relayout();
    }
}

void PanshiResponsiveCardRow::setAvailableWidth(int w)
{
    if (m_cards.isEmpty())
        return;

    const int n = m_cards.size();
    const int upper = qMin(m_maxCols, n);

    // 列数 = 「宽度放得下的最大列数」。minimumWidth 已在卡片自身设置（
    // Size::StateCardMinWidth），这里用它做预算；若调用方塞入的是别的控件，
    // 其 minimumWidth 也被读取，保证结果对该控件同样成立。
    int need = 0;
    for (QWidget *c : m_cards)
        need = qMax(need, c->minimumWidth());
    if (need <= 0)
        need = Size::StateCardMinWidth;

    int cols = 1;
    for (int c = upper; c >= 1; --c) {
        const int total = need * c + Space::M * (c - 1);
        if (w >= total) {
            cols = c;
            break;
        }
    }
    if (cols == m_cols)
        return;   // 未变化 → 不重排

    m_cols = cols;
    relayout();
}

void PanshiResponsiveCardRow::relayout()
{
    for (QWidget *c : m_cards) {
        if (m_grid->indexOf(c) >= 0)
            m_grid->removeWidget(c);
    }
    for (int i = 0; i < m_cards.size(); ++i)
        m_grid->addWidget(m_cards.at(i), i / m_cols, i % m_cols);

    // 只给实际用到的列拉伸，其余归零（否则末列会被空列拉伸抢走宽度）
    for (int i = 0; i < m_maxCols; ++i)
        m_grid->setColumnStretch(i, i < m_cols ? 1 : 0);
    // 行不吸收多余纵向空间（页面底部由调用方的 addStretch() 负责）
    for (int i = 0; i < m_maxCols; ++i)
        m_grid->setRowStretch(i, 0);
}

// ---------------------------------------------------------------- PanshiEmptyState
PanshiEmptyState::PanshiEmptyState(QWidget *parent)
    : QWidget(parent)
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(Space::XL, Space::XXL, Space::XL, Space::XXL);
    m_lay->setSpacing(Space::S);
    m_lay->setAlignment(Qt::AlignHCenter);

    m_icon = new DLabel(this);
    m_icon->setAlignment(Qt::AlignHCenter);
    m_icon->setFixedSize(Size::IconHero, Size::IconHero);
    m_lay->addWidget(m_icon, 0, Qt::AlignHCenter);

    // 主题切换后重查图标（2026-10-03）：pixmap 是**快照**，QIcon 换主题
    // 不会让已画的 pixmap 跟着变 → 切暗色后空状态图标仍是亮色那版。
    // 与 Ui::iconBadge / 侧栏导航同因，三处都要各自挂这个信号。
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::applicationPaletteChanged,
                     this, [this] { refreshIcon(); });

    m_title = makeLabel(QString(), TextLevel::SectionTitle, Semantic::Normal);
    m_title->setAlignment(Qt::AlignHCenter);
    m_title->setWordWrap(true);
    m_lay->addWidget(m_title);

    m_desc = makeLabel(QString(), TextLevel::Desc, Semantic::Muted);
    m_desc->setAlignment(Qt::AlignHCenter);
    m_desc->setWordWrap(true);
    m_lay->addWidget(m_desc);
}

void PanshiEmptyState::setIcon(const QString &iconName)
{
    m_iconName = iconName;
    refreshIcon();
}

void PanshiEmptyState::setTitle(const QString &title)
{
    m_title->setText(title);
}

void PanshiEmptyState::setDescription(const QString &desc)
{
    m_desc->setVisible(!desc.isEmpty());
    m_desc->setText(desc);
}

DPushButton *PanshiEmptyState::addAction(const QString &text, bool suggest)
{
    if (!m_actionLay) {
        m_actionLay = new QHBoxLayout();
        m_actionLay->setSpacing(Space::S);
        m_actionLay->setAlignment(Qt::AlignHCenter);
        m_lay->addSpacing(Space::S);
        m_lay->addLayout(m_actionLay);
    }
    DPushButton *b = suggest ? new DSuggestButton(text) : new DPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    m_actionLay->addWidget(b);
    return b;
}

void PanshiEmptyState::refreshIcon()
{
    if (m_iconName.isEmpty()) {
        m_icon->setPixmap(QPixmap());
        return;
    }
    const QPixmap pm = themedIcon(m_iconName, QStyle::SP_DriveHDIcon)
                           .pixmap(QSize(Size::IconHero, Size::IconHero));
    m_icon->setPixmap(pm);
}

// ---------------------------------------------------------------- PanshiLocalLoading（L2）
PanshiLocalLoading::PanshiLocalLoading(const QString &text, QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(Space::S, Space::S, Space::S, Space::S);
    lay->setSpacing(Space::S);

    m_spinner = new DSpinner(this);
    m_spinner->setFixedSize(Size::Spinner, Size::Spinner);
    lay->addWidget(m_spinner, 0, Qt::AlignVCenter);

    m_label = makeLabel(text, TextLevel::Desc, Semantic::Muted);
    lay->addWidget(m_label, 0, Qt::AlignVCenter);
    lay->addStretch();
}

void PanshiLocalLoading::setText(const QString &text)
{
    m_label->setText(text);
}

void PanshiLocalLoading::start()
{
    m_spinner->start();
    show();
}

void PanshiLocalLoading::stop()
{
    m_spinner->stop();
    hide();
}

// ---------------------------------------------------------------- PanshiFlatProgress（L3）
PanshiFlatProgress::PanshiFlatProgress(QWidget *parent)
    : DProgressBar(parent)
{
    setFixedHeight(Size::ThinProgress);
    setTextVisible(false);
    setRange(0, 0);
    hide();
}

void PanshiFlatProgress::setBusy(bool busy)
{
    setVisible(busy);
    if (busy)
        setRange(0, 0);
    else
        setRange(0, 100);
}

// ---------------------------------------------------------------- PanshiSkeleton（L1）
PanshiSkeleton::PanshiSkeleton(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void PanshiSkeleton::setRows(int rows)
{
    m_rows = qMax(1, rows);
    setFixedHeight(m_rows * (Size::RowMinHeight / 2) + (m_rows - 1) * Space::S);
    update();
}

void PanshiSkeleton::startPulse()
{
    if (m_timer)
        return;
    if (reduceMotionRequested())
        return;
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] {
        // 一个明暗周期由 pulseSteps() 个步进组成，走到末步回到 0。
        // 步数写成 constexpr 常量（而非内联的 PulsePeriod / PulseStep 整数除法），
        // 既表达意图，也消掉 -Warithmetic 对整数除法的诊断。
        m_tick = (m_tick + 1) % pulseSteps();
        update();
    });
    m_timer->start(Motion::PulseStep);
}

void PanshiSkeleton::stopPulse()
{
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    update();
}

void PanshiSkeleton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QColor c = itemBackground();
    const int rowH = Size::RowMinHeight / 2;
    for (int i = 0; i < m_rows; ++i) {
        const QRectF r(0.0, static_cast<qreal>(i * (rowH + Space::S)),
                       static_cast<qreal>(width()), static_cast<qreal>(rowH));
        QColor cell = c;
        if (m_timer) {
            // 轻微的明暗起伏，遵循"减少动态效果"时不启动定时器
            const double phase = static_cast<double>(m_tick) / static_cast<double>(pulseSteps());
            cell = alphaBlend(c, QColor(255, 255, 255, static_cast<int>(20.0 * phase)));
        }
        fillRounded(&p, r, Radius::Element, cell);
    }
}

// ---------------------------------------------------------------- PanshiSeparator
PanshiSeparator::PanshiSeparator(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Size::Separator);
}

void PanshiSeparator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setBrush(borderColor());
    p.drawRect(rect());
}

// ---------------------------------------------------------------- PanshiCollapsible
PanshiCollapsible::PanshiCollapsible(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::S);

    m_toggle = new DPushButton(QStringLiteral("▸ ") + title, this);
    m_toggle->setFlat(true);
    m_toggle->setCursor(Qt::PointingHandCursor);
    m_toggle->setFocusPolicy(Qt::StrongFocus);
    lay->addWidget(m_toggle);

    connect(m_toggle, &DPushButton::clicked, this, [this] {
        setExpanded(!isExpanded());
    });
}

void PanshiCollapsible::setContent(QWidget *content)
{
    if (m_content || !content)
        return;
    m_content = content;
    content->hide();
    layout()->addWidget(content);
}

bool PanshiCollapsible::isExpanded() const
{
    return m_content && m_content->isVisible();
}

void PanshiCollapsible::setExpanded(bool expanded)
{
    if (!m_content || expanded == m_content->isVisible())
        return;
    m_content->setVisible(expanded);
    updateMark();
}

void PanshiCollapsible::updateMark()
{
    const QString title = m_toggle->text().mid(2);   // 去掉前缀「▸ 」/「▾ 」
    m_toggle->setText((isExpanded() ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + title);
}

// ---------------------------------------------------------------- 参考信息卡
QIcon tintedFeatureIcon(const QString &key, const QColor &tint)
{
    const QString bundled = QStringLiteral(":/icons/nav/%1.svg").arg(key);
    if (QFileInfo::exists(bundled)) {
        // 自带源图是纯黑单色：必须现染。渲染尺寸取 64 再缩到目标像素，
        // 缩小采样能保住抗锯齿边缘（16px 直接渲染会发毛）。
        return QIcon(tintedPixmap(QIcon(bundled), QSize(64, 64), tint));
    }
    const QIcon theme = themedIcon(key, QStyle::SP_FileIcon);
    if (!theme.isNull())
        return theme;   // 彩色主题图标：染色会毁掉它的配色，原样返回
    return themedIcon(QStringLiteral("dialog-information"));
}

PanshiIconTile::PanshiIconTile(const QString &iconKey, int size, QWidget *parent)
    : QWidget(parent)
    , m_iconKey(iconKey)
    , m_size(size)
{
    setFixedSize(size, size);
    reloadIcon();
    // pixmap 是快照：切主题后必须重取（同 iconBadge 的教训），
    // 否则暗色下仍是亮色主题那版图标。
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::applicationPaletteChanged,
                     this, [this] {
        reloadIcon();
        update();
    });
}

void PanshiIconTile::reloadIcon()
{
    const int isz = qMax(12, int(m_size * 0.56));
    // 图标用**正文色**而非主题色：底色已经是强调色的柔和版，图标再用同色相
    // 就是「浅蓝上的蓝」——对比度不足，缩到 18px 基本糊掉（实测）。
    // 正文色在亮/暗两套下都与柔和底形成强对比，且同样跟随主题。
    m_pixmap = tintedFeatureIcon(m_iconKey, semanticColor(Semantic::Normal)).pixmap(isz, isz);
}

void PanshiIconTile::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fillRounded(&p, QRectF(rect()), Radius::Element, softFill(Semantic::Accent, Alpha::SoftFill));
    if (!m_pixmap.isNull()) {
        // 居中绘制：图标尺寸随 size 走，不依赖布局的 AlignCenter（自绘控件没这个能力）
        p.drawPixmap((width() - m_pixmap.width()) / 2,
                     (height() - m_pixmap.height()) / 2, m_pixmap);
    }
}

PanshiCaret::PanshiCaret(QWidget *parent)
    : QAbstractButton(parent)
{
    setFixedSize(Size::IconLarge, Size::IconLarge);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QObject::tr("展开或收起"));
}

void PanshiCaret::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    update();
}

void PanshiCaret::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(semanticColor(m_expanded ? Semantic::Normal : Semantic::Muted));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // 折线而非三角形字符：字形随字体变化（中文字体的 ▸ 字宽/基线各不相同），
    // 自绘在任意主题字体下都稳定居中。
    const QPointF c = QRectF(rect()).center();
    const qreal w = 4.5;
    const qreal h = 2.6;
    QPainterPath path;
    if (m_expanded) {   // 展开 → 向下
        path.moveTo(c.x() - w, c.y() - h);
        path.lineTo(c.x(), c.y() + h);
        path.lineTo(c.x() + w, c.y() - h);
    } else {            // 收起 → 向右
        path.moveTo(c.x() - h, c.y() - w);
        path.lineTo(c.x() + h, c.y());
        path.lineTo(c.x() - h, c.y() + w);
    }
    p.drawPath(path);
}

PanshiCommandChip::PanshiCommandChip(const QString &command, QWidget *parent)
    : QWidget(parent)
    , m_command(command)
{
    // 系统等宽字体 + T8：与 makeCommandRow 的命令字号一致（两处必须同档，
    // 否则同一份命令在对话框和页面里大小不同）。
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(mono);
    DFontSizeManager::instance()->bind(this, DFontSizeManager::T8);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(80);
    setToolTip(command);
    setAccessibleName(command);

    m_lay = new QHBoxLayout(this);
    m_lay->setContentsMargins(Space::S + 2, Space::XS, Space::XS, Space::XS);
    m_lay->setSpacing(Space::XS);
    m_lay->addStretch();
}

void PanshiCommandChip::setTrailing(QWidget *w)
{
    if (!w || m_trailing)
        return;
    m_trailing = w;
    if (w->parentWidget() != this)
        w->setParent(this);
    m_lay->addWidget(w, 0, Qt::AlignVCenter);
    updateGeometry();
    update();
}

QSize PanshiCommandChip::sizeHint() const
{
    const QFontMetrics fm(font());
    int h = fm.height() + 2 * Space::XS + 4;
    int w = fm.horizontalAdvance(m_command) + 2 * (Space::S + 2) + 8;
    if (m_trailing) {
        h = qMax(h, m_trailing->sizeHint().height() + 2 * Space::XS);
        w += m_trailing->sizeHint().width() + Space::XS;
    }
    return QSize(w, h);
}

QSize PanshiCommandChip::minimumSizeHint() const
{
    const QFontMetrics fm(font());
    int h = fm.height() + 2 * Space::XS + 4;
    int w = 80;
    if (m_trailing) {
        h = qMax(h, m_trailing->sizeHint().height() + 2 * Space::XS);
        w += m_trailing->sizeHint().width();
    }
    return QSize(w, h);
}

void PanshiCommandChip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fillRounded(&p, QRectF(rect()), Radius::Element, itemBackground());

    // 文字区右边界让位给 trailing：几何取 layout 排好的位置（不猜按钮宽度）
    int right = width() - (Space::S + 2);
    if (m_trailing)
        right = m_trailing->geometry().left() - Space::XS;
    QRect textRect = rect().adjusted(Space::S + 2, 0, 0, 0);
    textRect.setRight(qMax(textRect.left(), right));

    p.setPen(semanticColor(Semantic::Normal));
    p.setFont(font());
    const QString shown = fontMetrics().elidedText(m_command, Qt::ElideMiddle, textRect.width());
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, shown);
}

PanshiRefCard::PanshiRefCard(const QString &iconKey, const QString &title,
                             const QString &subtitle, QWidget *parent)
    : PanshiPanel(parent, Level::Surface)
{
    auto *l = body();
    l->setContentsMargins(Space::L, Space::M, Space::L, Space::M);
    l->setSpacing(Space::M);

    // ---- 标题行（整行可点）----
    m_header = new QWidget(this);
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->installEventFilter(this);

    m_headerRow = new QHBoxLayout(m_header);
    m_headerRow->setContentsMargins(0, 0, 0, 0);
    m_headerRow->setSpacing(Space::M);

    if (!iconKey.isEmpty())
        m_headerRow->addWidget(new PanshiIconTile(iconKey, Space::XXL, m_header), 0,
                               Qt::AlignVCenter);

    auto *titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(Space::XS);
    auto *t = makeLabel(title, TextLevel::SectionTitle, Semantic::Normal);
    QFont tf = t->font();
    tf.setBold(true);
    t->setFont(tf);   // bind 之后再置粗：T5 只定字号，粗体是这一层的身份标记
    titles->addWidget(t);
    if (!subtitle.isEmpty()) {
        auto *d = makeLabel(subtitle, TextLevel::Desc, Semantic::Muted);
        d->setWordWrap(true);
        titles->addWidget(d);
    }
    m_headerRow->addLayout(titles, 1);   // 文本列吸收多余宽度，caret 恒在最右

    m_caret = new PanshiCaret(m_header);
    m_headerRow->addWidget(m_caret, 0, Qt::AlignVCenter);
    QObject::connect(m_caret, &QAbstractButton::clicked, this, [this] {
        setExpanded(!m_expanded);
    });

    l->addWidget(m_header);

    auto *sep = new PanshiSeparator(this);
    m_sep = sep;
    l->addWidget(sep);

    m_contentHost = new QWidget(this);
    m_content = new QVBoxLayout(m_contentHost);
    m_content->setContentsMargins(0, 0, 0, 0);
    m_content->setSpacing(Space::S);
    l->addWidget(m_contentHost);

    syncExpanded();
}

void PanshiRefCard::addHeaderWidget(QWidget *w)
{
    if (!w)
        return;
    // 插在 caret 之前：组级操作靠右，caret 永远是最右一格（位置稳定才可预期）
    m_headerRow->insertWidget(qMax(0, m_headerRow->count() - 1), w, 0, Qt::AlignVCenter);
}

void PanshiRefCard::setExpanded(bool expanded)
{
    m_expanded = expanded;
    syncExpanded();
}

void PanshiRefCard::syncExpanded()
{
    if (m_sep)
        m_sep->setVisible(m_expanded);
    if (m_contentHost)
        m_contentHost->setVisible(m_expanded);
    if (m_caret) {
        m_caret->setExpanded(m_expanded);
        m_caret->setAccessibleName(m_expanded ? QObject::tr("收起") : QObject::tr("展开"));
    }
}

bool PanshiRefCard::eventFilter(QObject *watched, QEvent *e)
{
    // 标题行整行可点：标签/图块默认忽略鼠标事件 → 会冒泡到 m_header。
    // 用 Release 而非 Press：按下后拖走（想选文字/拖窗口）不该被当成点击。
    if (watched == m_header && e->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton && m_header->rect().contains(me->pos())) {
            setExpanded(!m_expanded);
            return true;
        }
    }
    return PanshiPanel::eventFilter(watched, e);
}

// ---- 表格无障碍（O9 收尾 / QTableWidgetItem 单元格级）----
// 设计：Qt 原生 Qt::AccessibleTextRole 作用在 QTableWidgetItem 上即可被读屏软件读出，
//       无需自定义 delegate（前期记忆里"DTK 无单元格级接口"是错误的，已纠正）。
//       列标题由 horizontalHeaderItem 提供；单元格值由 item->text() 提供。
//
// 两条边界（都有探针 tools/probe_table_a11y.cpp 守着）：
//  1. **只补名字，不改模型**：item(r,c) == nullptr 的空格直接跳过，不代填新 item。
//     无障碍 helper 不该有「顺手改数据」的副作用；且这些格子在视觉上本来就是空的，
//     读屏念作空单元格正是预期。
//  2. **cellWidget 列遍历不到**：名称/路径/操作列用的是 setCellWidget 而非 item，
//     必须由页面侧在各自的工厂里单独 setAccessibleName，且要带上行标识
//     （路径 / 版本号 / 快照名），否则读屏逐格移动时听不出是哪一行。

void setTableAccessibleName(QTableWidget *t, const QString &name)
{
    if (t)
        t->setAccessibleName(name);
}

void finalizeTableAccessible(QTableWidget *t)
{
    if (!t)
        return;
    const int cols = t->columnCount();
    for (int r = 0; r < t->rowCount(); ++r) {
        for (int c = 0; c < cols; ++c) {
            QTableWidgetItem *it = t->item(r, c);
            if (!it)
                continue;   // 见上方边界 1
            const QString hdr = t->horizontalHeaderItem(c)
                                   ? t->horizontalHeaderItem(c)->text()
                                   : QString();
            // 用 isEmpty() 而非「值是否为空/null」组合判断：值为 "0" 或 " "
            // 都是有效内容，误判成「无」会让读屏把有效数据念丢。
            const QString val = it->text();
            const QString a11y = val.isEmpty()
                                     ? QStringLiteral("%1: -").arg(hdr)
                                     : QStringLiteral("%1: %2").arg(hdr, val);
            it->setData(Qt::AccessibleTextRole, a11y);
        }
    }
}

void disableDialogTransparency(QWidget *dialog)
{
    if (!dialog)
        return;

    // ------------------------------------------------------------------
    // ★ 2026-10-06 修复：本函数改为「有意保留 DTK 原生渲染」的空操作
    // ------------------------------------------------------------------
    // 曾经的实现：winId() + DPlatformHandle::setEnableBlurWindow(false) 关掉
    // 窗口模糊，想得到「不透明、无毛玻璃」的弹窗。实测（X11 + kwin + dxcb）：
    //
    //   ① 关掉模糊后，DDialog 背景是**半透明**的（圆角 + 阴影设计，见开发指南
    //      D2 同款教训），没有毛玻璃兜底时会把底层页面文字直接透出来 ——
    //      用户报「奇怪的透明弹窗，能看到页面文字」；
    //   ② 在 exec() 之前对未显示的弹窗强行 winId() + 挂 DPlatformHandle，
    //      与 makeDialogNonBlockingWindowModal 的冻结交互后弹窗完全无响应
    //      （悬停/点击/按键全失效，Esc 关不掉）。
    //
    // 结论：不要跟 DTK 的弹窗渲染对抗。保留默认模糊 = 平台原生外观，
    // 背景由毛玻璃兜底，弹窗正常不透明。因此本函数现在什么都不做，
    // 保留签名仅为兼容各调用点；新增代码请直接使用 DDialog 默认渲染。
    //
    // 若未来确需「无模糊实底」弹窗，正确做法是给 DDialog 的**内容区**
    // 补不透明底色（autoFillBackground + 调色板），而不是关窗口模糊。
    Q_UNUSED(dialog);
}

void makeDialogNonBlockingWindowModal(QWidget *dialog)
{
    if (!dialog)
        return;

    // ------------------------------------------------------------------
    // 「窗口拖不动」的真正根因（2026-10-06 X11 实证 + 探针 panshi-dragmodalprobe）
    // ------------------------------------------------------------------
    // 现象：用户报「能点、能最大化最小化，就是拖不动」，且**在弹出某个提示之后
    // 出现**。此前几轮怀疑 CSD / dxcb 钩子，均被反向验证推翻。
    //
    // 实测：标题栏 13 点位 12 可拖（唯一例外 x=5 是窗口圆角），与 CSD 无关
    // ⇒ 无条件状态下拖拽是好的。触发条件是**模态对话框打开期间**。
    //
    // ⚠️ 走过的弯路（勿重复）：先把 ApplicationModal 降为 WindowModal，
    //   看着像"降级"，探针一跑就露馅 —— **DTitlebar 是主窗口的子控件**，
    //   WindowModal 的 grab 范围恰好覆盖父窗口，标题栏照样收不到 MousePress，
    //   拖拽依旧失效。探针场景 4 实测：modality 已是 WindowModal，
    //   而 activeModalWidget() 仍非空 ⇒ 仍在阻断。
    //   ⇒ 任何形式的"模态"都不解决问题，必须让主窗口**根本不是模态目标**。
    //
    // 采用方案：对话框设为 NonModal，改为「手动冻结」主窗口交互：
    //   · 禁用 centralWidget（页面里所有按钮/开关都点不动）
    //   · **保留标题栏可用**（不 disable 主窗口本身，只冻内容区）
    //   · 对话框关闭时自动恢复
    // 这样既保住了确认框的语义（用户不能越过对话框去操作页面），
    // 又让窗口始终可拖 —— 用户报的现象彻底消失。
    //
    // ⚠️ 前提：必须有 parent，否则找不到要冻结的主窗口，直接不改。
    QWidget *parent = dialog->parentWidget();
    if (!parent)
        return;

    QWidget *top = parent->window();
    if (!top || top == dialog)
        return;

    // DMainWindow 继承 QMainWindow，centralWidget() 正是页面容器
    QWidget *content = qobject_cast<QMainWindow *>(top)
                           ? qobject_cast<QMainWindow *>(top)->centralWidget()
                           : nullptr;
    if (!content)
        return;   // 认不出来就不动，绝不盲改

    // ------------------------------------------------------------------
    // ★ 2026-10-06 修复：冻结内容区不得波及对话框本身
    // ------------------------------------------------------------------
    // 症状：用户报「点击按钮后出现奇怪的透明弹窗，无法关闭、无法操作，
    //       且软件其它位置也无法操作」。
    // 根因：对话框以页面为父对象（如 ProtectionPage），页面在 content 子树内。
    //       冻结 = content->setEnabled(false)，Qt 的禁用状态沿对象树传播，
    //       对话框自身的 isEnabled() 变 false（父链查询），于是悬停/点击/按键
    //       全部被忽略 —— 看起来就是「冻住的无响应弹窗」。
    //       同时 exec() 使对话框成为模态，主窗口内容区本就被模态 grab 阻断，
    //       两者叠加便呈现用户描述的全部症状。
    // 修复：把对话框改挂到**顶层窗口**（不在 content 子树内），
    //       冻结内容区时不会波及对话框；模态语义仍由「内容区禁用」保证。
    // ⚠️ 必须在安装冻结守卫之前重挂 —— 冻结守卫在 Show 时冻结，Show 之后
    //    对话框再搬出子树就来不及了（禁用已沿旧父链传播过）。
    dialog->setParent(top);
    // setParent(QWidget*) 不改变窗口标志；保险起见确认仍是独立窗口
    if (!dialog->isWindow())
        dialog->setWindowFlag(Qt::Window, true);

    dialog->setWindowModality(Qt::NonModal);
    // 标题栏不在 content 内，故不会被动到 —— 这正是本方案能拖的关键。
    // ⚠️ 冻结必须在对话框 **Show 之后** 才生效（见 DialogFreezeGuard 注释：
    //    构造期冻结且不显示会让内容区永久点不动 —— 本轮点击失效的根因）。
    // 用 QPointer 防悬垂：dialog 析构时 content 可能先没（父子顺序不保证）。
    QPointer<QWidget> guard(content);
    auto freeze = [guard] {
        if (guard)
            guard->setEnabled(false);
    };
    auto restore = [guard] {
        if (guard)
            guard->setEnabled(true);
    };
    dialog->installEventFilter(new DialogFreezeGuard(freeze, restore, dialog));
    // 未及 Hide 就被析构（提前 deleteLater / 父链销毁）也要恢复
    QObject::connect(dialog, &QObject::destroyed, dialog, [guard] {
        if (guard)
            guard->setEnabled(true);
    });
}

} // namespace PanshiUi
