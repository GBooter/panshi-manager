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
    m_tabBar->addTab(tr("备份快照"));
    m_tabBar->addTab(tr("系统版本"));

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("panshiContent"));
    m_snapPage = new SnapshotsPage(win);
    m_deployPage = new DeploysPage(win);
    m_stack->addWidget(m_snapPage);
    m_stack->addWidget(m_deployPage);

    // 页头（UI 重构方案 §3.2：页头全页唯一，页级操作集中在页头右侧；
    // 子页 SnapshotsPage / DeploysPage 不再各自渲染页头与刷新按钮）
    auto *head = new QHBoxLayout;
    head->addWidget(Ui::pageHeader(tr("备份与版本"),
                                   tr("系统备份快照与已装系统版本管理"),
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
    lay->addWidget(m_stack, 1);

    connect(m_tabBar, &DTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    // 「创建备份」在当前标签页是「系统版本」时无意义，隐藏以免误导
    connect(m_tabBar, &DTabBar::currentChanged, bCreate, [bCreate](int idx) {
        bCreate->setVisible(idx == 0);
    });
    // 子页各自带 L2 局部加载（阶段 3-6），切换标签时只刷新当前子页
    connect(m_tabBar, &DTabBar::currentChanged, this, [this](int idx) {
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
    // 先确保落在「备份快照」子页：从总览页跳来时子页可能仍停在「系统版本」
    showSubPage(0);
    if (m_snapPage)
        QMetaObject::invokeMethod(m_snapPage, "onCreate", Qt::DirectConnection);
}
