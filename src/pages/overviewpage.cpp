// SPDX-License-Identifier: GPL-3.0-or-later
#include "overviewpage.h"
#include "common.h"
#include "masterswitch.h"
#include "../mainwindow.h"
#include "../backend.h"
#include "protectioncards.h"

#include <DIconButton>
#include <functional>
#include <QAbstractButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QVariantMap>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace Panshi;
using namespace PanshiSwitch;
using namespace PanshiUi;

namespace {

/** 系列身份色 → DPalette 角色（条与图例共用，保证同色且跟随主题切换） */
QColor barColor(BarRole r)
{
    const DPalette pa = DGuiApplicationHelper::instance()->applicationPalette();
    switch (r) {
    case BarRole::Accent: return pa.color(DPalette::Highlight);      // 继承自 QPalette（主题强调色）
    case BarRole::Warn:   return pa.color(DPalette::TextWarning);    // DTK ColorType
    case BarRole::Tips:   return pa.color(DPalette::TextTips);
    case BarRole::Track:  return pa.color(DPalette::ItemBackground);
    }
    return pa.color(DPalette::TextTips);
}

/**
 * 给按钮装「跟随主题」的自带 SVG 图标（2026-10-24 修「按钮图标看不清」）。
 *
 * 背景：原实现用 Ui::themedIcon("document-save" / "system-software-update"
 * / "view-refresh" / "security")—— 前三个名字**在 DCI 图标库里不存在**
 * （2026-10-24 审计，含系统 ostree 部署镜像），一路回落到 QStyle 标准图标：
 * 尺寸只有样式默认的 12~16px、灰色细线，白底按钮上几乎看不见
 * （用户截图实录）。security 虽在库里，但只有 flow 主题单色 webp，同样发虚。
 *
 * 修法（与侧栏同一套机制）：
 *   ① 改用自带单色 outline SVG（data/icons/nav/*.svg，纯黑源图），
 *      形状在 20px 下清晰，且与侧栏同图形——「备份与版本/系统更新/系统保护」
 *      一个语义一个 glyph，全应用一致；
 *   ② 颜色由 tintedSvgIcon 现染：建议键（蓝底）用白，常规键用正文色；
 *   ③ 显式 setIconSize(20)——DTK 按钮默认只给 12px，不设就白搭；
 *   ④ pixmap 是快照：切主题后必须重染（iconBadge 同一课，2026-10-03），
 *      故挂 applicationPaletteChanged；固定色（白）重染结果相同，无害。
 */
void setSvgButtonIcon(QAbstractButton *btn, const QString &name,
                      const std::function<QColor()> &colorFn, int px = Size::Icon)
{
    const QSize sz(px, px);
    auto apply = [btn, name, sz, colorFn] {
        btn->setIcon(tintedSvgIcon(name, sz, colorFn()));
        btn->setIconSize(sz);
    };
    apply();
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::applicationPaletteChanged,
                     btn, apply);
}

/**
 * 图例色点：按 BarRole 取色（绘制时实时取色，自动跟随主题切换）。
 *
 * 为什么不用 common.h 的 Ui::LegendDot（它吃 Semantic）：存储分布是**图表**，
 * 分段色 = 系列身份（与 usagechart 同源），不是语义状态色；Semantic 的
 * Accent/Success 会收敛到同一个 Highlight，表达不了三段。
 * 本控件与 StorageBar 共用 barColor()，保证「图例色 == 条上分段色」。
 */
class RoleDot : public QWidget
{
public:
    explicit RoleDot(BarRole role, QWidget *parent = nullptr)
        : QWidget(parent), m_role(role)
    {
        setFixedSize(10, 10);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(barColor(m_role));
        p.drawEllipse(rect());
    }

private:
    BarRole m_role;
};

} // namespace

/**
 * 存储分布堆叠条（设计文档 P0 ③）：一条横向占比条回答"空间去哪了"。
 *
 * 三段已用（/ostree ｜ /persistent ｜ 快照）+ 余量（轨道底色）。颜色取
 * DPalette 角色（Highlight / TextWarning / TextTips），与 usagechart 同源——
 * 图表系列色不属于"语义状态色"范畴，故不经过 Semantic 映射（见 RoleDot 注释）。
 *
 * 绘制：先画整条圆角轨道（ItemBackground），再以圆角轨道的裁剪路径
 * 画各分段矩形——分段与轨道圆角自然吻合，免去"首段左圆、末段右圆"的
 * 逐角计算。m_total<=0（未取数）时只画空轨道作为占位。
 *
 * ⚠️ 定义在**全局**作用域（不在匿名 namespace）：overviewpage.h 里
 *   `class StorageBar;` 前置声明的是全局类型，放进匿名 namespace 会变成
 *   `(anonymous)::StorageBar`，成员指针永远 incomplete（编译实测踩过）。
 */
class StorageBar : public QWidget
{
public:
    explicit StorageBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(Size::UsageBarHeight);
    }

    void setSegments(const QList<qint64> &bytes, qint64 total)
    {
        m_bytes = bytes;
        m_total = total;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(barColor(BarRole::Track));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);

        if (m_total <= 0 || m_bytes.isEmpty())
            return;   // 占位：只有轨道

        QPainterPath clip;
        clip.addRoundedRect(r, r.height() / 2, r.height() / 2);
        p.save();
        p.setClipPath(clip);
        const BarRole roles[] = {
            BarRole::Accent,  // /ostree
            BarRole::Warn,    // /persistent
            BarRole::Tips,    // 快照（通常≈0，灰=可忽略）
        };
        qreal x = r.left();
        for (int i = 0; i < m_bytes.size() && i < 3; ++i) {
            if (m_bytes.at(i) <= 0)
                continue;
            const qreal w = r.width() * m_bytes.at(i) / m_total;
            if (w <= 0)
                continue;
            p.setBrush(barColor(roles[i]));
            p.drawRect(QRectF(x, r.top(), w, r.height()));
            x += w;
        }
        p.restore();
    }

private:
    QList<qint64> m_bytes;
    qint64 m_total = 0;
};

OverviewPage::OverviewPage(MainWindow *win)
    : BasePage(win)
    , m_win(win)
{
    // 内容放入滚动容器，窗口尺寸不足时可滚动，避免底部内容被裁剪
    auto *scroll = Ui::makeScrollArea(this);
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(scroll);

    auto *lay = new QVBoxLayout(scrollContent);
    lay->setContentsMargins(Space::XL, 20, Space::XL, 20);
    lay->setSpacing(Space::L);

    // ---- 页头 + 右上角刷新（设计文档 P0 [刷新]：旧版无手动刷新入口）----
    auto *headRow = new QWidget;
    auto *headLay = new QHBoxLayout(headRow);
    headLay->setContentsMargins(0, 0, 0, 0);
    headLay->setSpacing(Space::M);
    headLay->addWidget(Ui::pageHeader(tr("总览"), tr("磐石系统运行状态一览"),
                                      QStringLiteral("utilities-system-monitor")), 1);
    auto *refreshBtn = new DIconButton(headRow);
    // view-refresh 同样不在 DCI 库 → 原回落 QStyle 灰色小箭头（12px）。
    // 改自带 cycle-update.svg（环形箭头）现染正文色，20px。
    setSvgButtonIcon(refreshBtn, QStringLiteral("cycle-update"),
                     [] { return semanticColor(Semantic::Normal); });
    refreshBtn->setToolTip(tr("刷新状态"));
    refreshBtn->setFixedSize(36, 36);
    connect(refreshBtn, &DIconButton::clicked, this, [this] { refresh(); });
    headLay->addWidget(refreshBtn, 0, Qt::AlignTop);
    lay->addWidget(headRow);

    // ---- L2 局部加载：本页分两步取数，但永不盖整页遮罩（见 refresh()）----
    m_busy = new PanshiLocalLoading(QString(), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // ---- 页级反馈条：开关切换的结果写在这里（阶段 4，替代模态 info/error 对话框）----
    m_feedback = new PanshiInlineBanner(scrollContent);
    m_feedback->hide();
    lay->addWidget(m_feedback);
    setFeedbackBanner(m_feedback);

    // ---- ① 状态 hero（设计文档 P0 ①）----
    // 旧版把「磐石功能」包进 PanshiSection，造成"section 标题"与"开关行内标题"
    // 同屏重复（截图可见两处"磐石功能"）。现在去掉 section 包装，开关行
    // （图标 + 标题 + 状态副标题 + 详情 + 总开关）直接由 Surface 面板承载，
    // 成为首屏视觉锚点 —— 开关本身就是保护状态的结论。
    // 开关逻辑不在此重写：仍复用 PanshiSwitchRow（唯一数据源，保护页同款）。
    auto *hero = new PanshiPanel(scrollContent, PanshiPanel::Level::Surface);
    auto *heroLay = hero->body();
    heroLay->setContentsMargins(Space::XL, Space::L, Space::XL, Space::L);
    m_switchRow = new PanshiSwitchRow(this, true, hero);
    connect(m_switchRow, &PanshiSwitchRow::switchChanged, this,
            [this] { m_win->refreshPanshiPages(); });
    connect(m_switchRow, &PanshiSwitchRow::detailRequested, this,
            [this] { m_win->gotoPage(PageId::Protection); });
    heroLay->addWidget(m_switchRow);
    lay->addWidget(hero);

    // ---- 状态卡：响应式列数（PanshiResponsiveCardRow）。
    //     旧实现是固定 4 列 QHBoxLayout —— 窄窗下 4 卡硬挤后整行横向溢出，
    //     被外层 makeScrollArea（ScrollBarAlwaysOff）静默裁掉，第 4 卡与
    //     行内开关因此不可达。容器按可用宽度自动折行，永不溢出。
    m_statRow = new PanshiResponsiveCardRow(4, scrollContent);
    m_cardProtect = new PanshiStateCard(tr("磐石保护"), m_statRow);
    m_cardWritable = new PanshiStateCard(tr("系统可写层"), m_statRow);
    m_cardSnap = new PanshiStateCard(tr("系统快照"), m_statRow);   // 与页签同名，见 versionpage.cpp
    m_cardFree = new PanshiStateCard(tr("系统盘剩余"), m_statRow);
    m_statRow->addCard(m_cardProtect);
    m_statRow->addCard(m_cardWritable);
    m_statRow->addCard(m_cardSnap);
    m_statRow->addCard(m_cardFree);
    lay->addWidget(m_statRow);
    // 宽度喂入交给 BasePage：它同时监听 viewport Resize 与 showEvent，
    // 覆盖「窗口缩放」与「隐藏页首次 show」两条路径（后者是 3-8 踩到的坑）。
    registerResponsiveCardRow(m_statRow, scroll, Space::XL * 2);

    // L1 骨架屏：占位在同一位置，与状态卡互斥显示
    m_skeleton = new PanshiSkeleton(scrollContent);
    m_skeleton->setRows(3);
    m_skeleton->hide();
    lay->addWidget(m_skeleton);

    // ---- ③ 存储分布（设计文档 P0 ③，替换旧的 2×2 纯数字网格）----
    // 为什么用"堆叠条 + 图例"而不是 4 个孤立数字：数字只能回答"各占多少"，
    // 条能一眼回答"空间去哪了、还剩多少比例"。卡片里的「系统盘剩余」与
    // 旧网格的「系统盘可用」是同一数字（截图可见两处 47.89GB），
    // 重设计后可用空间只在图例出现一次（卡片仍保留快捷速览）。
    auto *secStore = new PanshiSection(tr("存储空间"), tr("磐石系统各仓库占用（物理）"),
                                       scrollContent);
    auto *storeLay = secStore->content();
    storeLay->setSpacing(Space::M);
    m_storageBar = new StorageBar(secStore);
    storeLay->addWidget(m_storageBar);
    auto *legend = new QGridLayout;
    legend->setHorizontalSpacing(Space::XL);
    legend->setVerticalSpacing(Space::M);
    legend->addWidget(makeLegendRow(tr("系统仓库 /ostree"), BarRole::Accent, &m_valRepo), 0, 0);
    legend->addWidget(makeLegendRow(tr("数据仓库 /persistent"), BarRole::Warn, &m_valPrepo), 0, 1);
    legend->addWidget(makeLegendRow(tr("快照数据"), BarRole::Tips, &m_valSnap), 1, 0);
    legend->addWidget(makeLegendRow(tr("可用空间"), BarRole::Track, &m_valFree), 1, 1);
    legend->setColumnStretch(0, 1);
    legend->setColumnStretch(1, 1);
    storeLay->addLayout(legend);
    lay->addWidget(secStore);

    // ---- ④ 系统信息 ----
    auto *secSys = new PanshiSection(tr("系统信息"), tr("当前运行的系统与内核"), scrollContent);
    auto *g1 = new QGridLayout;
    g1->setHorizontalSpacing(Space::XL);
    g1->setVerticalSpacing(Space::S);
    g1->addWidget(makeInfoRow(tr("系统名称"), &m_valSys), 0, 0);
    g1->addWidget(makeInfoRow(tr("系统版本"), &m_valVer), 0, 1);
    g1->addWidget(makeInfoRow(tr("内核版本"), &m_valKernel), 1, 0);
    g1->addWidget(makeInfoRow(tr("磐石部署"), &m_valDeploy), 1, 1);
    g1->setColumnStretch(0, 1);
    g1->setColumnStretch(1, 1);
    secSys->content()->addLayout(g1);
    lay->addWidget(secSys);

    // ---- 快速操作 ----
    // 三个入口都是「跳页 + 到达后动作」：光跳页会让用户到了目标页还得再找一次
    // 按钮，语义上这是一次动作。动作挂到目标页 idleReached() 上，等 refresh()
    // 结束再执行（详见 MainWindow::gotoPage(PageId, afterArrive) 的说明）。
    auto *secQuick = new PanshiSection(tr("快速操作"), tr("常用功能快捷入口"), scrollContent);
    auto *btns = new QHBoxLayout;
    btns->setSpacing(Space::M);
    // 设计文档 P0 ⑤：图标 + 文字按钮。2026-10-24 起图标改用自带单色 SVG
    // 现染（document-save / system-software-update / security 在 DCI 库
    // 不存在或发虚，详见 setSvgButtonIcon 注释）：备份=层叠、更新=环形、
    // 保护=盾牌，与侧栏同图形；建议键（蓝底）配白图标，其余配正文色。
    auto *bBackup = Ui::button(tr("立即创建备份"), true);
    setSvgButtonIcon(bBackup, QStringLiteral("layers-snapshot"),
                     [] { return QColor(Qt::white); });   // DSuggestButton 前景恒白
    connect(bBackup, &DPushButton::clicked, this, [this] {
        // 落到「系统快照」子页后直接打开创建对话框（合成一次动作，用户无需再点）。
        // 三个动作都走 QMetaObject::invokeMethod：总览页不持有目标页头文件，
        // 且这些入口是各页的私有实现，转发槽是它们对外唯一的动作契约。
        m_win->gotoPage(PageId::BackupVersion, SubPageId::Snapshots, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::BackupVersion),
                                      "requestCreateSnapshot", Qt::DirectConnection);
        });
    });
    auto *bUpdate = Ui::button(tr("检查系统更新"));
    setSvgButtonIcon(bUpdate, QStringLiteral("cycle-update"),
                     [] { return semanticColor(Semantic::Normal); });
    connect(bUpdate, &DPushButton::clicked, this, [this] {
        m_win->gotoPage(PageId::Update, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::Update), "requestCheckUpdate",
                                      Qt::DirectConnection);
        });
    });
    auto *bProtect = Ui::button(tr("保护设置"));
    setSvgButtonIcon(bProtect, QStringLiteral("shield-check"),
                     [] { return semanticColor(Semantic::Normal); });
    connect(bProtect, &DPushButton::clicked, this, [this] {
        // 跳保护页并定位高亮「开启保护（恢复只读）」行：总览的语境是
        // "保护可能没开，去看看"，直接指出该动哪一行比让用户自己找更省事。
        m_win->gotoPage(PageId::Protection, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::Protection), "highlightEnableProtect",
                                      Qt::DirectConnection);
        });
    });
    btns->addWidget(bBackup);
    btns->addWidget(bUpdate);
    btns->addWidget(bProtect);
    btns->addStretch();
    secQuick->content()->addLayout(btns);
    lay->addWidget(secQuick);

    lay->addStretch();
}

PanshiRow *OverviewPage::makeInfoRow(const QString &label, DLabel **valueOut)
{
    auto *row = new PanshiRow;
    row->setTitle(label);
    // 值标签用 ElidedLabel（中部省略，永不撑破行）+ 显式 minW=0：
    // 纯文本值的退化是优雅的（省略 + 悬停看全文），不需要交互控件那 200 的地板。
    //
    // 为何关键：本页「系统信息」是 2 列 × 2 行的 PanshiRow 网格（存储区
    // 已于 2026-10-06 重设计改为图例行，见 makeLegendRow —— 图例行更紧凑，
    // 不经过本行论证，但同源结论仍成立）。
    // 若值标签带 200 地板且不折行，单行最小宽约 346px、两列合计 806px——
    // 会把 scrollContent 的最小宽顶到 806，窄窗下被外层 makeScrollArea
    // （ScrollBarAlwaysOff）静默裁剪，连累第 3/4 张状态卡与「磐石功能」开关
    // （阶段 3-8 实测：viewport=720 / scrollContent=806 / 裁掉 86px）。
    // 去掉地板 + 可省略后，整页最小宽回落到卡片行的 528px。
    auto *v = new Ui::ElidedLabel(QStringLiteral("-"));
    v->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(v, DFontSizeManager::T5);
    row->addControl(v, 0);
    if (valueOut)
        *valueOut = v;
    return row;
}

QWidget *OverviewPage::makeLegendRow(const QString &name, BarRole role,
                                     DLabel **valueOut)
{
    // 存储分布图例项：色点 + 名称 + 值。与 makeInfoRow 同构但更紧凑——
    // 不套 PanshiRow（48px 行高对图例太奢侈），且图例需要色点表达系列身份。
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(Space::S);
    h->addWidget(new RoleDot(role, w), 0, Qt::AlignVCenter);
    // 名称弱化（Muted）、值突出（TextTitle）：扫视路径 = 先认色点所代表的
    // 仓库，再读数值 —— 与状态卡"标签弱化、值强化"同一层级纪律。
    h->addWidget(makeLabel(name, TextLevel::Body, Semantic::Muted), 1);
    auto *v = new Ui::ElidedLabel(QStringLiteral("-"));
    v->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(v, DFontSizeManager::T5);
    h->addWidget(v, 0, Qt::AlignVCenter);
    if (valueOut)
        *valueOut = v;
    return w;
}

void OverviewPage::refresh()
{
    // 本页分两步取数（快速状态 + 慢速存储统计）。旧实现对每一步各盖一层全页遮罩，
    // 于是切到总览页会"连闪两次灰屏"（问题清单：加载态 I-01）。
    // 新策略（重构方案 §4.5 三档加载）：
    //   首次进入 → L1 骨架屏占位；之后 → 一律 L2 局部加载，内容始终可见可交互。
    setMaskLoadingEnabled(false);

    if (m_firstLoad) {
        m_statRow->hide();
        m_skeleton->show();
        m_skeleton->startPulse();
    }

    m_busy->setText(tr("正在读取磐石系统状态…"));
    m_busy->start();

    runAsync([]() -> QVariant {
        QVariantMap d;
        d.insert(QStringLiteral("immutable"), isImmutable());
        d.insert(QStringLiteral("os"), getOsVersion());
        d.insert(QStringLiteral("deploy"), getDeployVersion());
        d.insert(QStringLiteral("kernel"), getKernel());
        d.insert(QStringLiteral("writable"), QVariant::fromValue(getWritableStatus()));
        d.insert(QStringLiteral("snaps"), QVariant::fromValue(getSnapshots()));
        return d;
    }, [this](const QVariant &res) { onLoadedFast(res); });
}

void OverviewPage::onLoadedFast(const QVariant &res)
{
    const QVariantMap d = res.toMap();
    const bool immutable = d.value(QStringLiteral("immutable")).toBool();
    const WritableStatus w = d.value(QStringLiteral("writable")).value<WritableStatus>();
    m_snaps = d.value(QStringLiteral("snaps")).value<QList<SnapshotInfo>>();
    const QJsonObject os = d.value(QStringLiteral("os")).toJsonObject();

    // 首次加载完成：骨架屏退场、状态卡入场（此后刷新不再出现骨架屏，避免闪屏）
    if (m_firstLoad) {
        m_firstLoad = false;
        m_skeleton->stopPulse();
        m_skeleton->hide();
        m_statRow->show();
    }

    // 同步磐石功能开关行状态
    if (m_switchRow)
        m_switchRow->applyStatus(presentStatus(w));

    // 状态卡：颜色只表达状态（正常/注意/危险），不再用 4 种装饰色区分卡片。
    //
    // 卡 1/卡 2 的**文案与语义色**走 protectioncards.h，与系统保护页共用同一份映射
    // （此前两页各内联一份，且已经漂移过：概览页写「开启/关闭」、保护页写「已开启/已关闭」）。
    //
    // ⚠️ 但**取数口径不同且刻意不合并**：本页「只读保护」用 isImmutable()
    //    （ctl -s 的 Immutable mode，**fail-closed**：出错返回 true），系统保护页用
    //    !w.enable（ctl status -j 的 conf.enable，**fail-open**：出错返回 false）。
    //    两者是不同命令、不同维度、失败行为相反 —— 合并会在后端出错时产出
    //    「只读保护=已开启 且 可写层=已解锁」这类自相矛盾的卡片。详见 protectioncards.h 头注释。
    //
    // ⚠️ 本页卡片**标题**沿用概览页自己的措辞（磐石保护 / 系统可写层），
    //    不取 protectioncards.h 的（只读保护 / 可写层）—— 标题属各页文案决策，不共享。
    PanshiProtection::applyCard(m_cardProtect, PanshiProtection::protectCard(immutable));
    PanshiProtection::applyCard(m_cardWritable, PanshiProtection::writableCard(w.enable));

    // 快照卡只报个数：大小移入存储分布图例（重设计去重——旧版同一大小在
    // 卡片与存储区各出现一次，截图可见「680.47KB / 680.77KB」两份且数值还漂移）。
    m_cardSnap->setValue(tr("%1 个").arg(m_snaps.size()), Semantic::Normal);
    m_cardSnap->setStatus(QString());           // 无状态语义时不显示徽章
    m_cardFree->setValue(tr("统计中…"), Semantic::Muted);
    m_cardFree->setStatus(QString());

    // 存储分布图例：第二步统计未完成前统一"统计中…"（与状态卡同策略）
    m_valRepo->setText(tr("统计中…"));
    m_valPrepo->setText(tr("统计中…"));
    m_valSnap->setText(tr("统计中…"));
    m_valFree->setText(tr("统计中…"));

    m_valSys->setText(os.value(QStringLiteral("system")).toString());
    const QString ver = os.value(QStringLiteral("version")).toString() + QLatin1Char(' ')
                        + os.value(QStringLiteral("edition")).toString();
    m_valVer->setText(ver.trimmed().isEmpty() ? QStringLiteral("-") : ver.trimmed());
    m_valKernel->setText(d.value(QStringLiteral("kernel")).toString());
    const QString deploy = d.value(QStringLiteral("deploy")).toString();
    m_valDeploy->setText(deploy.isEmpty() ? QStringLiteral("-") : deploy);

    // 第二步：后台统计存储空间（慢速，有缓存则快）。
    // 沿用 L2 局部加载：只更新顶部那一行 spinner，页面其余部分保持可读。
    // 快照大小不再单独计算：分布条与图例统一用 RepoSizes::snapshotRoot，
    // 消除旧版"卡片 680.47KB / 存储区 680.77KB"两处取数漂移。
    m_busy->setText(tr("正在统计存储空间…"));
    runAsync([]() -> QVariant {
        QVariantMap d;
        d.insert(QStringLiteral("sizes"), QVariant::fromValue(getRepoSizes()));
        return d;
    }, [this](const QVariant &res) { onLoadedSizes(res); });
}

void OverviewPage::onLoadedSizes(const QVariant &res)
{
    m_busy->stop();

    const QVariantMap d = res.toMap();
    const RepoSizes sizes = d.value(QStringLiteral("sizes")).value<RepoSizes>();

    m_cardSnap->setValue(tr("%1 个").arg(m_snaps.size()), Semantic::Normal);
    m_cardFree->setValue(formatSize(sizes.freeRoot), Semantic::Normal);

    m_valRepo->setText(formatSize(sizes.sysRepo));
    m_valPrepo->setText(formatSize(sizes.persistRepo));
    m_valSnap->setText(formatSize(sizes.snapshotRoot));
    m_valFree->setText(formatSize(sizes.freeRoot));

    // 存储分布堆叠条：三段已用 + 余量。分母 = 仓库合计 + 可用（系统盘总容量），
    // 与图例数值同源（snapshotRoot），避免"卡片/存储区各报一份"式的再次漂移。
    m_storageBar->setSegments(
        { sizes.sysRepo, sizes.persistRepo, sizes.snapshotRoot },
        sizes.sysRepo + sizes.persistRepo + sizes.snapshotRoot + sizes.freeRoot);
}
