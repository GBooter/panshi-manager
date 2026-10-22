// SPDX-License-Identifier: GPL-3.0-or-later
#include "versionpage.h"
#include "common.h"
#include "snapshotspage.h"
#include "deployspage.h"
#include "../mainwindow.h"

#include <DTabBar>
#include <DPalette>
#include <DGuiApplicationHelper>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QGridLayout>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

VersionManagementPage::VersionManagementPage(MainWindow *win)
    : BasePage(win)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(10);

    // DTK 标签页 (隐藏默认的"+"添加按钮)
    m_tabBar = new DTabBar(this);
    m_tabBar->setObjectName(QStringLiteral("panshiTabBar"));
    m_tabBar->setVisibleAddButton(false);
    // 命名（2026-10-06）：这两个词指代 ostree 的两个**不同层级**的对象，
    // 早期叫「备份快照 / 系统版本」，与版本页里「系统版本」列头的版本号同词异义，
    // 用户会以为是同一类东西。现统一为：
    //   Tab 0 = 系统快照（snapshot，可还原的备份点）
    //   Tab 1 = 系统部署实例（deployment，一整套可启动的系统）
    m_tabBar->addTab(tr("系统快照"));
    m_tabBar->addTab(tr("系统部署实例"));

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("panshiContent"));
    m_snapPage = new SnapshotsPage(win);
    m_deployPage = new DeploysPage(win);
    m_stack->addWidget(m_snapPage);
    m_stack->addWidget(m_deployPage);

    // 页头（UI 重构方案 §3.2：页头全页唯一，页级操作集中在页头右侧；
    // 子页 SnapshotsPage / DeploysPage 不再各自渲染页头与刷新按钮）
    auto *head = new QHBoxLayout;
    // 副标题说清「两者不是一回事」，是用户理解本页的第一句话（详见下方 hint 区）
    head->addWidget(Ui::pageHeader(tr("备份与版本"),
                                   tr("系统快照是可还原的备份点，系统部署实例是可启动的系统"),
                                   QStringLiteral("document-save")));
    head->addStretch();
    auto *bRefresh = Ui::button(tr("刷新"));
    connect(bRefresh, &DPushButton::clicked, this, [this] {
        if (m_tabBar->currentIndex() == 0)
            m_snapPage->refresh();
        else
            m_deployPage->refresh();
    });
    auto *bCreate = Ui::button(tr("创建备份"), true);
    // onCreate 为 SnapshotsPage 私有槽，经 QMetaObject 调用
    connect(bCreate, &DPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(m_snapPage, "onCreate");
    });
    head->addWidget(bRefresh);
    head->addWidget(bCreate);

    auto *headWrap = new QWidget(this);
    head->setContentsMargins(0, 0, 0, 0);
    headWrap->setLayout(head);
    lay->addWidget(headWrap);
    lay->addWidget(m_tabBar);

    // ---- 两页区别说明区 ----
    // Subtle 面板（item 底，比页面底深一档）承载，不抢表格的视觉重心；
    // 展开的对照表默认收起，需要时点一下即可 —— 消歧不该变成常驻噪音。
    auto *hintBox = new PanshiUi::PanshiPanel(this, PanshiUi::PanshiPanel::Level::Subtle);
    auto *hintLay = hintBox->body();
    hintLay->setSpacing(PanshiUi::Space::S);
    m_hintLabel = PanshiUi::makeLabel(hintText(0), PanshiUi::TextLevel::Desc,
                                     PanshiUi::Semantic::Muted);
    hintLay->addWidget(m_hintLabel);
    auto *cmp = new PanshiUi::PanshiCollapsible(
        tr("「系统快照」和「系统部署实例」有什么区别？"), hintBox);
    cmp->setContent(buildCompareWidget());
    hintLay->addWidget(cmp);
    lay->addWidget(hintBox);

    lay->addWidget(m_stack, 1);

    connect(m_tabBar, &DTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    // 「创建备份」在当前标签页是「系统部署实例」时无意义，隐藏以免误导
    connect(m_tabBar, &DTabBar::currentChanged, bCreate, [bCreate](int idx) {
        bCreate->setVisible(idx == 0);
    });
    // 子页各自带 L2 局部加载（阶段 3-6），切换标签时只刷新当前子页
    connect(m_tabBar, &DTabBar::currentChanged, this, [this](int idx) {
        updateHint(idx);
        if (idx == 0)
            m_snapPage->refresh();
        else
            m_deployPage->refresh();
    });

    // 子页空闲转达本页（阶段 5 跨页跳转的关键一环）。
    //
    // 本页是"复合页"：refresh() 只是转发给当前子页，在途任务计数落在子页身上，
    // 本页 isBusy() 恒为 false。若不转达，MainWindow 会在子页数据尚未落地时
    // 就认为"已到达"并执行动作——「立即创建备份」会踩到 SnapshotsPage 的
    // `if (m_running) return;` 防重入守卫而被静默丢弃。
    // 转达后两个子页的空闲都会反映到本页，到达时序才正确。
    connect(m_snapPage, &BasePage::idleReached, this, [this] {
        if (!m_snapPage->isBusy() && !m_deployPage->isBusy())
            emit idleReached();
    });
    connect(m_deployPage, &BasePage::idleReached, this, [this] {
        if (!m_snapPage->isBusy() && !m_deployPage->isBusy())
            emit idleReached();
    });
}

void VersionManagementPage::refresh()
{
    // 仅刷新当前激活的子页, 避免不必要的重复加载
    if (m_tabBar->currentIndex() == 0)
        m_snapPage->refresh();
    else
        m_deployPage->refresh();
}

void VersionManagementPage::shutdown()
{
    m_snapPage->shutdown();
    m_deployPage->shutdown();
}

bool VersionManagementPage::isBusy() const
{
    return (m_snapPage && m_snapPage->isBusy()) || (m_deployPage && m_deployPage->isBusy());
}

void VersionManagementPage::showSubPage(int index)
{
    if (index >= 0 && index < m_tabBar->count())
        m_tabBar->setCurrentIndex(index);
}

void VersionManagementPage::requestCreateSnapshot()
{
    // 先确保落在「系统快照」子页：从总览页跳来时子页可能仍停在「系统部署实例」
    showSubPage(0);
    if (m_snapPage)
        QMetaObject::invokeMethod(m_snapPage, "onCreate", Qt::DirectConnection);
}

QString VersionManagementPage::hintText(int index) const
{
    // 每页一句话，落在用户该做的动作上，而不是术语定义上。
    return index == 0
        ? tr("这里记录的是「可还原的备份点」：可以创建、删除，也可以把系统还原回它记录的那一刻。")
        : tr("这里列出的是「一整套可启动的系统」：只能删除以释放磁盘空间，删除后该系统无法再启动。");
}

void VersionManagementPage::updateHint(int index)
{
    if (m_hintLabel)
        m_hintLabel->setText(hintText(index));
}

QWidget *VersionManagementPage::buildCompareWidget()
{
    // 三列对照：行标题 + 快照列 + 部署列。
    // 值列一律 wordWrap（makeLabel 对 Desc 层级已开）→ 窄窗下折行而非横向溢出，
    // 这也是本页不放进 QScrollArea 还能不裁切的前提。
    auto *w = new QWidget;
    auto *g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(PanshiUi::Space::XL);
    g->setVerticalSpacing(PanshiUi::Space::S);

    g->addWidget(PanshiUi::makeLabel(tr("对比项"), PanshiUi::TextLevel::Aux,
                                      PanshiUi::Semantic::Muted), 0, 0);
    g->addWidget(PanshiUi::makeLabel(tr("系统快照"), PanshiUi::TextLevel::Body,
                                      PanshiUi::Semantic::Accent), 0, 1);
    g->addWidget(PanshiUi::makeLabel(tr("系统部署实例"), PanshiUi::TextLevel::Body,
                                      PanshiUi::Semantic::Accent), 0, 2);

    const QStringList keys = {
        tr("是什么"), tr("怎么来的"), tr("能做什么"), tr("删除后"),
    };
    const QStringList snapVals = {
        tr("可还原的备份点，只记录系统层的差异"),
        tr("你手动创建，或系统初始化时自动生成"),
        tr("可以创建、删除，也可以还原回它记录的时刻"),
        tr("只是少一个还原点，系统照常运行"),
    };
    const QStringList deployVals = {
        tr("一整套可启动的系统，占用完整磁盘空间"),
        tr("系统升级、回滚时自动生成"),
        tr("只能删除，用来释放磁盘空间"),
        tr("该系统将无法再启动"),
    };
    for (int i = 0; i < keys.size(); ++i) {
        g->addWidget(PanshiUi::makeLabel(keys.at(i), PanshiUi::TextLevel::Aux,
                                          PanshiUi::Semantic::Muted), i + 1, 0);
        g->addWidget(PanshiUi::makeLabel(snapVals.at(i), PanshiUi::TextLevel::Desc,
                                          PanshiUi::Semantic::Normal), i + 1, 1);
        g->addWidget(PanshiUi::makeLabel(deployVals.at(i), PanshiUi::TextLevel::Desc,
                                          PanshiUi::Semantic::Normal), i + 1, 2);
    }
    g->setColumnStretch(1, 1);
    g->setColumnStretch(2, 1);

    // 两页的联动关系：最容易误解的一点单列一行说清。
    // 「回滚前」徽章会同时出现在两个列表里，指的却是不同层级的对象。
    g->addWidget(PanshiUi::makeLabel(
                     tr("两者关系：还原一个快照后，系统会自动生成一个新的部署实例"
                        "（带「回滚前」标记），可在「系统部署实例」页看到它。"),
                     PanshiUi::TextLevel::Desc, PanshiUi::Semantic::Accent),
                 keys.size() + 1, 0, 1, 3);
    return w;
}
