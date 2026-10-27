// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file components.h
 * 磐石设置 UI 组件层：结构 / 语义 + 自绘（不含 QSS、不含 hex 色值）
 *
 * 设计原则（UI 重构方案 §2.2）：
 *  - 全部颜色取自 DPalette / DGuiApplicationHelper，元素自身只表达"语义"；
 *  - 层次 = 页面底(window) 与 分组底(base) 的色差 + 8px 圆角，不用描边、不用阴影；
 *  - 图标全部走 DIconTheme::findQIcon()，缺失时回落到 QStyle 标准图标；
 *  - 字体全部经 DFontSizeManager::bind()；
 *  - 本文件默认不使用 Q_OBJECT（不需要 moc），避免给组件层引入 moc 依赖。
 *    例外只有两处（新增前请先确认确有此需要，并确认 AUTOMOC 已覆盖本文件）：
 *      · PanshiRow：搜索高亮需要一个可动画的 qreal 属性，
 *        QPropertyAnimation 必须走 Q_PROPERTY；
 *      · PanshiInlineBanner：关闭按钮要对外发 `dismissed()`，
 *        让调用方能区分「用户主动关掉」与「数据变化后自动消失」
 *        （2026-10-10：告警关闭后要落进告警历史，就必须有这个信号）。
 */

#include "tokens.h"

#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DIconTheme>
#include <DLabel>
#include <DPalette>
#include <DProgressBar>
#include <DPushButton>
#include <DSpinner>
#include <DSuggestButton>
#include <DWarningButton>

#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

class QGridLayout;
// 前置声明必须在 namespace 外：写在 PanshiUi 内会声明出 PanshiUi::QPropertyAnimation
// 这种不存在的嵌套类型，cpp 里按完整类型使用时报 incomplete type（阶段 5 编译修正）
class QPropertyAnimation;
class QTableWidget;
class QTableWidgetItem;
class QMouseEvent;
class QResizeEvent;

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace PanshiUi {

/**
 * 【已弃用 · 2026-10-06 起为空操作】历史上用于关闭 DTK 对话框的窗口模糊效果。
 *
 * ⚠️ 不要再调用它做「关模糊」——实测关掉模糊后 DDialog 的半透明背景会把
 * 底层页面文字透出来（用户报「透明弹窗」），且 exec() 前强行 winId() + 挂
 * DPlatformHandle 会与冻结机制叠加导致弹窗完全无响应。现保留 DTK 原生渲染
 * （毛玻璃即平台标准外观），本函数仅为兼容旧调用点而保留，内部直接返回。
 */
void disableDialogTransparency(QWidget *dialog);

/**
 * 让确认类对话框**不阻断主窗口拖拽**（2026-10-06 新增）。
 *
 * 背景 —— 「窗口拖不动」的真正根因：
 *   无条件状态下主窗口拖拽是好的（标题栏 13 点位实测 12 可拖，仅窗口圆角除外）。
 *   用户之所以看到「弹了提示之后窗口就拖不动」，是因为 `QDialog::exec()`
 *   建立**模态 grab**，主窗口（含其 DTitlebar 子控件）收不到标题栏鼠标事件，
 *   拖动状态机无从启动。
 *
 * ⚠️ 走过的弯路（勿重复）：曾先把 ApplicationModal 降为 **WindowModal**，
 *   探针 panshi-dragmodalprobe 实测无效 —— DTitlebar 是主窗口的子控件，
 *   WindowModal 的 grab 范围恰好覆盖父窗口，标题栏照样收不到事件。
 *   ⇒ 任何形式的"模态"都不解决问题，必须让主窗口**根本不是模态目标**。
 *
 * 采用方案：对话框设为 `Qt::NonModal`，改为「手动冻结」主窗口交互：
 *   · `setEnabled(false)` 冻结 centralWidget（页面内所有按钮/开关不可点）
 *   · **不 disable 主窗口本身** ⇒ 标题栏保持可用，窗口随时可拖
 *   · 对话框 Show 时冻结、Hide / destroyed 时自动恢复
 * 确认框的语义保住了（用户不能越过对话框去操作页面），且窗口不再"卡住"。
 *
 * ⚠️ 冻结必须挂在 **Show** 事件上，而不是本函数调用时立即生效：
 *   各页面在启动时即被整体构造（QStackedWidget 全量 new），部分对话框
 *   是**构造期创建、用时才 show** 的成员（如磁盘空间页的设置窗口）。
 *   若在构造时冻结，对话框「只构造不显示」则冻结永不解除 ——
 *   整个内容区永久点不动（2026-10-06 点击失效回归的根因）。
 *
 * ⚠️ **重挂父对象**（2026-10-06 新增）：对话框原本挂在页面（在 centralWidget
 *   子树内），冻结 = `centralWidget->setEnabled(false)` 会沿对象树传播，
 *   把对话框自身也禁用 —— 表现为弹窗完全不响应悬停/点击/按键（用户报
 *   「奇怪的透明弹窗，无法关闭，软件其它位置也无法操作」）。本函数会把
 *   对话框改挂到**顶层窗口**（不在 centralWidget 子树内），冻结不再波及它。
 *
 * ⚠️ 无 parent、或父窗口不是 QMainWindow（取不到 centralWidget）时**不做任何
 * 改动** —— 认不出来就不动，避免"看起来改了其实没改"的静默失效。
 *
 * 重复调用幂等。在对话框 show/exec 之前调用即可（构造期调用也安全）。
 */
void makeDialogNonBlockingWindowModal(QWidget *dialog);

/**
 * 语义：唯一允许表达"颜色意图"的方式。
 * 映射（重构方案 §2.3，语义收敛为 4 类）：
 *   Muted → TextTips；Normal → TextTitle；
 *   Accent/Success → Highlight（跟随用户主题色，不再自备绿色）；
 *   Warning / Danger → TextWarning。
 *
 * ⚠️ Danger 曾映射到 `TextLively`，**实测其默认主题取值是中性黑**
 *   （2026-10-03 V-23 调色板探针）—— 危险语义完全失效。DTK 里
 *   `TextLively` 的语义是「活跃式文本（不受活跃色影响）」，本就不是危险色。
 *   现改为复用 `TextWarning`。详见 components.cpp 的 semanticColor()。
 *
 * 颜色不是信息的唯一载体：Danger 与 Warning 同色后，两级语义仍由
 * **图标**（dialog-error / dialog-warning）、**文案**与**控件类型**区分
 * —— 既满足 WCAG 1.4.1，也让主题色异常时语义不失效。
 * 提示条（PanshiInlineBanner）即按此实现——图标承载语义、正文用中性色。
 */
enum class Semantic
{
    Muted,     // 说明 / 次要文字
    Normal,    // 正文 / 标签（标题色）
    Accent,    // 强调值（跟随主题色）
    Success,   // 成功 / 正常
    Warning,   // 警告
    Danger,    // 危险
};

/** 字体层级 → DFontSizeManager 映射 */
enum class TextLevel
{
    PageTitle,    // T2
    Value,        // T2（大数值，刻意与页面标题同级）
    SectionTitle, // T5
    Body,         // T5（行标签）
    ValueSmall,   // T4
    Desc,         // T6
    Aux,          // T7（徽章 / 图例 / 辅助）
    Mono,         // T8（命令 / ID）
};

/**
 * 磁盘使用率分档阈值（%）：≥ 该值取警示语义，否则正常。
 * ⚠️ 唯一定义点 —— 页面（updateOverview）与配色探针（C7 自动断言）共用，
 * 改动阈值时两边同时生效，不会出现「页面已改、断言仍按旧阈值」的假绿。
 */
constexpr double kUsageWarnPct = 75.0;

/** 使用率 → 语义。与 kUsageWarnPct 配套，供页面与探针共用 */
Semantic usageSemanticForPct(double pct);

// ---------------------------------------------------------------- 颜色查询
DPalette appPalette();
QColor semanticColor(Semantic s);
/**
 * 语义色的柔和底。alpha 默认 ≈15%（徽章等小面积元素）；
 * 整条提示条等大面积元素传 Alpha::BannerFill，以拉开与页面底的差别。
 */
QColor softFill(Semantic s, int alpha = Alpha::SoftFill);
QColor baseColor();
QColor windowColor();
QColor itemBackground();
QColor borderColor();

// ---------------------------------------------------------------- 图标 / 字体
/** 主题图标；主题缺失时回落到 QStyle 标准图标，保证始终有图可显示 */
QIcon themedIcon(const QString &name, QStyle::StandardPixmap fallback = QStyle::SP_FileIcon);

/**
 * 把单色源图染成目标色（保留 alpha 通道的抗锯齿边缘）。
 *
 * 为什么需要它（2026-10-24 线性图标风格）：
 *   QtSvg **不支持 currentColor**，SVG 里写死的颜色不会随主题/状态变化；
 *   而侧栏选中态、按钮图标都要求「图标与文字一起染成主题色」。故把 QIcon
 *   渲成 pixmap 后用 SourceIn 换色：色值取自 appPalette()，主题切换天然跟随
 *   （比原先「切主题重查图标」更彻底——连重查都不需要）。
 *
 * ⚠️ 为什么不给 QIcon 传 QIcon::Selected 让主题自己染：
 *   部分主题会对 Selected 状态做**整图替换**（把彩色图标压成单色 symbolic），
 *   颜色失控（2026-10-03 教训）。自己染，彩色/明暗都受控。
 */
QPixmap tintedPixmap(const QIcon &ic, const QSize &size, const QColor &color);

/**
 * 把自带单色 SVG（data/icons/nav/<name>.svg，纯黑源图）染成目标色，
 * 生成指定尺寸的 QIcon（按钮/图标按钮用）。
 *
 * ⚠️ pixmap 是快照：调用方若把 QIcon 挂到长期存活的控件上，切主题后要
 *    重新调用本函数换图标（参考 common.h iconBadge 的
 *    applicationPaletteChanged 重查，2026-10-03 教训）。
 */
QIcon tintedSvgIcon(const QString &name, const QSize &size, const QColor &color);
DLabel *makeLabel(const QString &text, TextLevel lv, Semantic s = Semantic::Normal);
void applySemantic(DLabel *l, Semantic s);
/**
 * 主题切换后重算所有「落色式」标签的字色。
 *
 * ⚠️ 必调（2026-10-05 暗色故障）：applySemantic 现在把颜色**快照**进控件调色板
 * （不再用 setForegroundRole 让 DLabel 自己去解析角色），而 DLabel 收到
 * PaletteChange 时会重走 initPainter() 覆盖掉这份快照。切主题后若不重算，
 * 字色会留在旧主题 —— 亮→暗就是「黑字压深底」，对比 1.4，字直接消失。
 * 在 `applicationPaletteChanged` 处理里调一次即可。
 */
void refreshExactColorLabels();
/** 系统"减少动态效果"开关（DConfig org.deepin.dde.appearance / enable-animations） */
bool reduceMotionRequested();

// ---------------------------------------------------------------- 分组容器
/**
 * 自绘容器：依次是 window(页面底) < base(分组底) < item(分组内条)。
 * 层次靠底色差 + 圆角，不画描边。
 */
class PanshiPanel : public QWidget
{
public:
    enum class Level
    {
        Surface,   // 分组容器：base 底 + 8px 圆角
        Subtle,    // 分组内条：item 底 + 8px 圆角
        Flat,      // 透明，仅提供布局
    };

    explicit PanshiPanel(QWidget *parent = nullptr, Level level = Level::Surface);

    Level level() const { return m_level; }
    /** 内容布局（已带统一内边距） */
    QVBoxLayout *body() const { return m_body; }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    Level m_level;
    QVBoxLayout *m_body;
};

/** 带标题的分组：标题行左上角，右上角可挂组级操作 */
class PanshiSection : public PanshiPanel
{
public:
    explicit PanshiSection(const QString &title, const QString &desc = QString(),
                           QWidget *parent = nullptr);

    /** 往标题行右侧加控件（组级操作） */
    void addHeaderWidget(QWidget *w);
    QVBoxLayout *content() const { return body(); }

private:
    QHBoxLayout *m_headerRow;
};

// ---------------------------------------------------------------- 设置行
/**
 * 行式设置项：左侧「标题 + 说明」，右侧控件。
 *
 * 挤压行为：左侧文本列承担拉伸（宽裕时吸收多余宽度）；
 * 右侧控件区有显式最小宽度，空间不足时**不会先塌**。
 *
 * 但「控件区下限」不是一刀切的常量 —— 要区分两类内容（阶段 3-8 实测修正）：
 *   - **交互控件**（spinbox / 下拉 / 按钮）：被压塌即**不可用**（值看不见、
 *     选项截断），必须保底 → 传默认的 Size::RowControlMinWidth（200）。
 *   - **纯文本值**（信息行的 value label）：退化是**优雅**的 —— 折行 / 省略
 *     都仍可读，给它 200 的地板反而会把整页最小宽顶上去，导致外层
 *     ScrollBarAlwaysOff 静默裁剪（总览页实测：info 网格把 scrollContent
 *     顶到 806px，窄窗下裁掉第 3 卡右缘与开关）。
 *     这类内容请显式传 minW = 0，并优先用 Ui::ElidedLabel / 允许 wordWrap。
 */
class PanshiRow : public QWidget
{
    Q_OBJECT
    /** 高亮强度 0..1（QPropertyAnimation 驱动；组件层其余类不需要 moc，本类因动画需要） */
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight)
public:
    explicit PanshiRow(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setDescription(const QString &desc);
    /** 供阶段 4 设置搜索索引读取（不依赖控件反射） */
    QString title() const;
    QString description() const;
    /**
     * 加右侧控件（按加入顺序排列）。
     * minW < 0 时用默认下限 Size::RowControlMinWidth（交互控件）；
     * 纯文本值请传 0（让它可以自由收缩，靠折行/省略退化）。
     */
    void addControl(QWidget *w, int minW = -1);
    void addStretch(int stretch = 1);

    /**
     * 短暂高亮本行（设置搜索命中后的落点提示，阶段 4）。
     *
     * 用 Accent 柔和底闪一下再淡出——设置页不是列表，没有"当前项"概念，
     * 因此不做常驻选中态。动画时长取 Motion::Slow，且尊重系统
     * "减少动态效果"开关（reduceMotionRequested()）：关闭动画时直接
     * 显示后定时清除，不做渐隐。
     */
    void flashHighlight();

    /** Q_PROPERTY 访问器（动画驱动用，业务侧请调 flashHighlight()） */
    qreal highlight() const;
    void setHighlight(qreal v);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    DLabel *m_title;
    DLabel *m_desc;
    /** 控件区宿主：承载最小宽度约束（布局只在 widget 上认 minimumWidth） */
    QWidget *m_controlsHost = nullptr;
    QHBoxLayout *m_controls;
    /** 记录 addControl 加入的控件，便于把行标题作为无障碍名称回填（O9） */
    QList<QWidget *> m_rowControls;
    /** 控件区当前生效的下限；取所有 addControl 请求的最大值（初值 0 = 尚无控件） */
    int m_controlMinWidth = 0;
    /** 高亮强度 0..1；>0 时绘制 Accent 柔和底 */
    qreal m_highlight = 0.0;
    QPropertyAnimation *m_highlightAnim = nullptr;
    QTimer *m_highlightClear = nullptr;
};

// ---------------------------------------------------------------- 标签 / 徽章
/** 状态徽章：柔和语义底 + 语义色文字（T7） */
class PanshiBadge : public QWidget
{
public:
    explicit PanshiBadge(const QString &text, Semantic s = Semantic::Muted,
                         QWidget *parent = nullptr);

    void setText(const QString &text);
    void setSemantic(Semantic s);
    QString text() const;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void refresh();

    DLabel *m_label;
    Semantic m_semantic;
};

// ---------------------------------------------------------------- 反馈
/**
 * 内联提示条：替代现在的 infoDialog / errorDialog 与三种样式各异的提示条。
 * 固定在内容区顶部，不覆盖内容。
 *
 * 语义由「图标 + 柔和底」承载，正文一律用中性文字色：
 * 彩色小字在浅色柔和底上对比度不足（WCAG AA 需 4.5:1），
 * 且一旦主题色异常（如 TextWarning/TextLively 被替换）正文会一起失去可读性。
 */
class PanshiInlineBanner : public QWidget
{
    Q_OBJECT
public:
    explicit PanshiInlineBanner(QWidget *parent = nullptr);

    void setSemantic(Semantic s);
    void setText(const QString &text);
    /**
     * 是否显示右上角关闭按钮。
     *
     * ⚠️ 关闭 = 用户主动 dismiss，会先发 `dismissed()` 再隐藏；
     *    而 `setText(QString())` 是「内容没了自动收起」，不发信号。
     *    调用方需要「关闭后记进历史/不再重复弹」时，
     *    **必须连 `dismissed()` 而不是靠 hide 事件** —— 否则
     *    自动消失也会被误记成用户关掉的（告警历史会平白多出条目）。
     */
    void setClosable(bool closable);

    /** 追加一个操作按钮（如「立即重启」「重试」）；返回按钮以便调用方 connect */
    DPushButton *addAction(const QString &text, bool suggest = false);

    /**
     * 挂载「原始输出」折叠块（方案 §4.4：原始输出折叠在「详情」里）。
     *
     * 为什么需要：后端失败时 errorMessage 往往是多行原始输出（ostree/deepin-immutable-ctl
     * 的 stderr），直接拼进正文会让 Banner 变成一大坨日志，用户读不到可行动的那句话。
     * → 正文只留一行可行动说明，原始输出收进默认收起的详情。
     *
     * 传空字符串则移除详情区（回到单行形态）。
     * 复用 PanshiCollapsible 组件，不新造折叠控件。
     */
    void setDetails(const QString &title, const QString &rawText);
    /**
     * ms 毫秒后自动隐藏（成功类反馈默认 3000）。
     *
     * 定时器可取消（阶段 4）：重复调用会**重置**倒计时而不是叠加——
     * 否则先前的定时器会在新内容刚显示时把整条 Banner 隐藏掉。
     * setText() 展示新内容时同样会取消待定隐藏。
     */
    void autoDismiss(int ms);

signals:
    /** 用户点了右上角关闭按钮（自动消失不触发） */
    void dismissed();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void refresh();

    DLabel *m_icon;
    DLabel *m_text;
    /** 文本 + 详情块的竖排容器（构造时创建，setDetails 只往里加折叠块） */
    QWidget *m_textCol = nullptr;
    class QVBoxLayout *m_textColLay = nullptr;
    class PanshiCollapsible *m_details = nullptr;
    QWidget *m_actions;
    QHBoxLayout *m_actionLay;
    QPushButton *m_close;
    Semantic m_semantic;
    class QTimer *m_dismissTimer = nullptr;
};

// ---------------------------------------------------------------- 数据展示
/** 状态卡：大数值(T2) + 标签(T6) + 可选状态徽章(T7) */
class PanshiStateCard : public PanshiPanel
{
public:
    explicit PanshiStateCard(const QString &title, QWidget *parent = nullptr);

    /**
     * @param level 值的字号层级，默认 T2 大数值。
     *   纯日期/元数据类内容应传 TextLevel::ValueSmall(T4)：
     *   大字号数值区只留给容量/数量结论（2026-10-09 修正）——
     *   日期跟 T2 放大后在 168px 最小卡宽下必然截断。
     */
    void setValue(const QString &value, Semantic s = Semantic::Normal,
                  TextLevel level = TextLevel::Value);
    /** status 为空时隐藏徽章 */
    void setStatus(const QString &status, Semantic s = Semantic::Muted);

private:
    DLabel *m_value;
    PanshiBadge *m_status;
};

// ---------------------------------------------------------------- 响应式卡片行
/**
 * 状态卡「响应式行」：按可用宽度在最多 maxCols 列之间自动切换列数。
 *
 * 背景（阶段 3-8）：多张 PanshiStateCard 若用固定列数的 QHBoxLayout/QGridLayout
 * 硬排，窄窗下各卡被压到 minimumSizeHint 后整行横向溢出；而承载页面的
 * Ui::makeScrollArea 是 ScrollBarAlwaysOff —— 溢出部分被**静默裁掉且不可达**
 * （总览页实测：第 4 卡数值被截、同容器内的开关被推出视口）。
 * 这是「外层容器静态裁剪」，与 3-6 表格问题同一层，调列宽/省略无效。
 *
 * 解法 = 两道防线：
 *   1. 每张卡有显式最小宽度（PanshiStateCard 已带 Size::StateCardMinWidth）；
 *   2. 本容器按可用宽度算「放得下的最大列数」，放不下就折行 —— 永不横向溢出。
 *
 * 用法：
 *   auto *row = new PanshiResponsiveCardRow(4, parent);
 *   row->addCard(card1); ... row->addCard(card4);
 *   row->setAvailableWidth(viewport->width() - 内边距*2);   // 尺寸变化时调用
 *
 * 调用方负责在 viewport / 页面 resize 时喂入新的可用宽度（本类不做事件监听，
 * 避免在组件层假设上层是 QScrollArea 还是普通容器）。
 */
class PanshiResponsiveCardRow : public QWidget
{
public:
    /** maxCols：宽裕时最多分成几列（如 4 张卡就给 4） */
    explicit PanshiResponsiveCardRow(int maxCols = 4, QWidget *parent = nullptr);

    void addCard(QWidget *card);

    /**
     * 设置可用宽度并重排。
     * 列数 = 「宽度放得下的最大列数」（下限 1 列，上限 addCard 的数量）。
     * 列数未变化时不重排，避免 resize 期间反复触发重绘。
     */
    void setAvailableWidth(int w);

    int columnCount() const { return m_cols; }

private:
    /** 按当前 m_cols 把卡片放回网格（摘出→重放→重设列/行拉伸） */
    void relayout();

    QGridLayout *m_grid;
    QList<QWidget *> m_cards;
    int m_maxCols;
    int m_cols = 0;
};

/** 空状态：图标 + 标题 + 说明 + 明确的主操作 */
class PanshiEmptyState : public QWidget
{
public:
    explicit PanshiEmptyState(QWidget *parent = nullptr);

    void setIcon(const QString &iconName);
    void setTitle(const QString &title);
    void setDescription(const QString &desc);
    DPushButton *addAction(const QString &text, bool suggest = true);

private:
    void refreshIcon();

    DLabel *m_icon;
    DLabel *m_title;
    DLabel *m_desc;
    QVBoxLayout *m_lay;
    QHBoxLayout *m_actionLay = nullptr;
    QString m_iconName;
};

// ---------------------------------------------------------------- 加载态（三档）
/** L2 局部加载：小 spinner + 文字，放目标容器内部，其余内容保持可读 */
class PanshiLocalLoading : public QWidget
{
public:
    explicit PanshiLocalLoading(const QString &text = QString(), QWidget *parent = nullptr);

    void setText(const QString &text);
    void start();
    void stop();

private:
    DSpinner *m_spinner;
    DLabel *m_label;
};

/** L3 顶部细进度条：长任务（扫描/还原/升级）使用 */
class PanshiFlatProgress : public DProgressBar
{
public:
    explicit PanshiFlatProgress(QWidget *parent = nullptr);
    /** 不确定进度；busy=false 时可正常使用 setValue */
    void setBusy(bool busy);
};

/** L1 骨架屏：首次进入且无任何数据时显示与最终结构同形的占位块 */
class PanshiSkeleton : public QWidget
{
public:
    explicit PanshiSkeleton(QWidget *parent = nullptr);

    void setRows(int rows);
    void startPulse();
    void stopPulse();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    int m_rows = 3;
    int m_tick = 0;
    class QTimer *m_timer = nullptr;
};

// ---------------------------------------------------------------- 折叠区
/**
 * 可折叠内容区：▸/▾ 标题行 + 默认收起的内容。
 * 用于「技术参考」「常用命令」「危险操作」等高级内容（方案 §3.3 P6/P2），
 * 替代"把长文本平铺在主界面"的旧做法。
 */
class PanshiCollapsible : public QWidget
{
public:
    explicit PanshiCollapsible(const QString &title, QWidget *parent = nullptr);

    /** 设置折叠区内容（只能设置一次；内容默认收起） */
    void setContent(QWidget *content);

    bool isExpanded() const;
    void setExpanded(bool expanded);

private:
    void updateMark();

    DPushButton *m_toggle;
    QWidget *m_content = nullptr;
};

// ---------------------------------------------------------------- 参考信息卡
/**
 * 特性 / 参考用的图标：`:res/icons/nav/<key>.svg` 优先（自带单色源图，
 * **必须按 tint 染色**，否则纯黑源图在暗色主题下等于隐形），
 * 没有自带 SVG 时才用 DIconTheme 的彩色主题图标（着色会破坏其配色，故原样返回）。
 *
 * 存在的理由：featureRow / 参考网格 / 图标图块三处都要「同一套取图与染色规则」，
 * 各写一份必然分叉（其中一处忘了染色 → 暗色下图标消失且极难发现）。
 */
QIcon tintedFeatureIcon(const QString &key, const QColor &tint);

/**
 * 图块：柔和强调底 + 染色图标，作参考卡的「身份标记」。
 * 主题切换自动重取图标（pixmap 是快照，见 iconBadge 的同类教训）。
 */
class PanshiIconTile : public QWidget
{
public:
    PanshiIconTile(const QString &iconKey, int size = 32, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void reloadIcon();

    QString m_iconKey;
    int m_size;
    QPixmap m_pixmap;
};

/** 展开/收起折线指示器：自绘（不依赖图标库），QAbstractButton 提供点击与键盘语义 */
class PanshiCaret : public QAbstractButton
{
public:
    explicit PanshiCaret(QWidget *parent = nullptr);

    void setExpanded(bool expanded);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    bool m_expanded = true;
};

/**
 * 命令条：等宽命令 + item 底 + 圆角，右端可挂 trailing 控件（复制按钮）。
 * 命令过长时中部省略（tooltip / 无障碍名给全文）。
 *
 * 为什么自绘而不套 QLabel：QLabel 只能「裁切」不能「省略」，长命令会把整列
 * 顶宽；而命令是自由长度文本，绝不该决定行宽（同 ElidedLabel 的教训）。
 */
class PanshiCommandChip : public QWidget
{
public:
    explicit PanshiCommandChip(const QString &command, QWidget *parent = nullptr);

    /** 挂右端控件（复制按钮）；文字区自动让位 */
    void setTrailing(QWidget *w);
    QString command() const { return m_command; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_command;
    QWidget *m_trailing = nullptr;
    QHBoxLayout *m_lay;
};

/**
 * 参考信息卡：图块 + 标题 + 副标题 +（可折叠的）内容区。
 *
 * 与 PanshiCollapsible 的分工：
 *   · PanshiCollapsible 是「一行文字开关」，用在已有分组内部（更新详情 / 原始输出）；
 *   · 本类是**独立分组**，自己就是一张卡：标题行可点、右侧可挂组级操作，
 *     内容落在卡片内。
 * 之所以要分开：把「▸ 标题」这种纯文字开关直接摆在页面上时，既没有容器暗示
 * 它是可展开的分组，也没有层级（2026-10-27 关于页实证：两条居中细字
 * 悬在一片留白里，看起来像没做完的占位）。
 *
 * 默认展开（组件本身的中性默认），**但两处真实调用都显式 setExpanded(false)**：
 * 关于页（2026-10-27 定稿）与自绘关于对话框。理由一致 —— 这两块是查阅型内容，
 * 页面主体是应用信息，默认铺开一整屏清单会把主体稀释掉；折叠态靠标题+副标题
 * 交代内含什么，caret 与「标题行整行可点」负责可发现性。
 * 组级 header 按钮（如「复制全部」）挂在标题行、**不随内容收起**。
 */
class PanshiRefCard : public PanshiPanel
{
public:
    PanshiRefCard(const QString &iconKey, const QString &title, const QString &subtitle = QString(),
                  QWidget *parent = nullptr);

    /** 往标题行右侧加组级操作（默认展开态也可见） */
    void addHeaderWidget(QWidget *w);
    /** 内容布局（已带 0 内边距，调用方自行加条目） */
    QVBoxLayout *content() const { return m_content; }

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

protected:
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    void syncExpanded();

    QWidget *m_header = nullptr;
    QHBoxLayout *m_headerRow = nullptr;
    QWidget *m_sep = nullptr;
    QWidget *m_contentHost = nullptr;
    QVBoxLayout *m_content = nullptr;
    PanshiCaret *m_caret = nullptr;
    bool m_expanded = true;
};

// ---------------------------------------------------------------- 杂项
/** 分隔线：1px，颜色取 DPalette::FrameBorder */
class PanshiSeparator : public QWidget
{
public:
    explicit PanshiSeparator(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
};

// ---- 表格无障碍（O9 收尾 / QTableWidgetItem 单元格级）----
// 见 components.cpp 实现注释：Qt 原生 Qt::AccessibleTextRole 作用在 item 上即可被读屏读出，
// 无需自定义 delegate。列标题由 horizontalHeaderItem 提供，单元格值由 item->text() 提供。
void setTableAccessibleName(QTableWidget *t, const QString &name);
// 遍历所有已填充的单元格：非空设「列标题: 值」，空 item 设中性占位「列标题: -」。
void finalizeTableAccessible(QTableWidget *t);

} // namespace PanshiUi
