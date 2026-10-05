// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"

#include <DTabBar>

class QStackedWidget;
class MainWindow;
class SnapshotsPage;
class DeploysPage;

/**
 * 备份管理页: 合并 备份快照 + 系统版本(部署)
 * 通过 DTabBar 标签切换两个子页面
 */
class VersionManagementPage : public BasePage
{
    Q_OBJECT
public:
    explicit VersionManagementPage(MainWindow *win);

    void refresh() override;
    void shutdown() override;

    /**
     * 复合页：本页自身的在途计数恒为 0（refresh 转发给子页），
     * 必须把两个子页的忙碌状态聚合上来，「到达后动作」才会等到真正空闲
     * （详见 .cpp 里 idleReached 转达的说明）。
     */
    bool isBusy() const override;

    /** 切换到指定子页 (0=备份快照, 1=系统版本) */
    void showSubPage(int index);

    /**
     * 对外动作契约（阶段 5 跨页跳转）：切到「备份快照」子页并打开创建对话框。
     * Q_INVOKABLE：总览页的快速操作经元对象调用，不持有本页头文件。
     * 内部再把动作转给 SnapshotsPage::onCreate。
     */
    Q_INVOKABLE void requestCreateSnapshot();

private:
    DTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    SnapshotsPage *m_snapPage = nullptr;
    DeploysPage *m_deployPage = nullptr;
};
