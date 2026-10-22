// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"

#include <DTabBar>

class QStackedWidget;
class QWidget;
class MainWindow;
class SnapshotsPage;
class DeploysPage;
// ⚠️ 不要在这里前置声明 DLabel —— components.h 已带 DWIDGET_USE_NAMESPACE，
// 再声明一个全局 DLabel 会与 Dtk::Widget::DLabel 构成歧义（编译报 reference is ambiguous）。

/**
 * 备份管理页: 合并 系统快照(snapshot) + 系统部署实例(deployment)
 * 通过 DTabBar 标签切换两个子页面
 *
 * ⚠️ 两者是不同层级的对象，别再改回「备份快照 / 系统版本」：
 *   快照 = 可还原的备份点（可创建/删除/还原）；
 *   部署实例 = 一整套可启动的系统（只能删除，删了不能启动）。
 *   早期页签叫「系统版本」，与版本页里「系统版本」列头（版本号字符串）同词异义。
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

    /** 切换到指定子页 (0=系统快照, 1=系统部署实例) */
    void showSubPage(int index);

    /**
     * 对外动作契约（阶段 5 跨页跳转）：切到「系统快照」子页并打开创建对话框。
     * Q_INVOKABLE：总览页的快速操作经元对象调用，不持有本页头文件。
     * 内部再把动作转给 SnapshotsPage::onCreate。
     */
    Q_INVOKABLE void requestCreateSnapshot();

private:
    /**
     * 两页区别说明区（2026-10-06 新增）。
     *
     * 为什么必须有：这两个标签页的对象都是「系统备份/版本相关」，早期还同名，
     * 用户分不清「删一个备份」和「删一个系统」的后果差别。本区把差异摆在
     * 标签页正下方 —— 切换标签时它就在眼前，是消歧最省力的位置。
     * 说明文案与展开的对照表由本页统一维护（两子页各写一份必然分叉）。
     */
    void updateHint(int index);
    /** 随标签切换的一句话定位（写在类里，tr() 上下文才是本页） */
    QString hintText(int index) const;
    /** 对照表内容（是什么 / 怎么来的 / 能做什么 / 删除后 / 两页联动关系） */
    QWidget *buildCompareWidget();

    DTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    SnapshotsPage *m_snapPage = nullptr;
    DeploysPage *m_deployPage = nullptr;
    /** 随标签切换的一句话定位（Subtle 面板内第一行） */
    DLabel *m_hintLabel = nullptr;
};
