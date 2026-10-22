// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"

#include "../backend.h"
DWIDGET_USE_NAMESPACE

class QTableWidget;
class MainWindow;

/**
 * 系统版本（部署）子页（阶段 3-6 重写 + 2026-10-06 两轮重设计）：
 *  - 反馈走 PanshiInlineBanner，不再弹模态窗；
 *  - **顶部 3 张状态卡**（部署总数 / 当前启动 / 可删除部署），与快照页策略卡
 *    同构 —— 两个子页视觉结构一致，页面上半不再是光秃秃的提示 + 表格；
 *  - **版本列去内联徽章**：原「当前启动/待生效/回滚前」徽章与「状态」列取值重复
 *    （同一行同时出现两个「当前启动」），且挤占版本号宽度导致 28 字符的版本串
 *    被中省略成「deep….100」。徽章职责并入状态列（语义着色），版本号列加宽到
 *    210 完整显示；
 *  - **状态列语义着色**：当前启动=Accent / 待生效·回滚备份=Warning / 历史=Muted，
 *    扫视即可区分「正在运行 / 待启用 / 可清理」；
 *  - 空状态改用 PanshiEmptyState；
 *  - 列宽全部 Interactive（可拖拽调整）+ 操作列下限钳制（原「版本号 Stretch」
 *    列手柄无效，用户反馈列宽不可调）；
 *  - 删除按钮改为按 commitId 定位，不再捕获行下标（刷新后下标会错位）；
 *  - L2 局部加载 + m_running 防重入。
 */
class DeploysPage : public BasePage
{
    Q_OBJECT
public:
    explicit DeploysPage(MainWindow *win);

    void refresh() override;

private:
    void onLoaded(const QVariant &res);
    void onUndeploy(const QString &commitId);
    static QString humanizeUndeployError(const QString &raw);
    void applyColumnWidths();
    /** 按 commitId 唯一定位部署；commitId 为空时返回 nullptr */
    const Panshi::DeployInfo *findDeploy(const QString &commitId) const;

    MainWindow *m_win = nullptr;
    QTableWidget *m_table = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    /** 部署状态三卡（部署总数 / 当前启动 / 可删除部署） */
    PanshiUi::PanshiResponsiveCardRow *m_statRow = nullptr;
    PanshiUi::PanshiStateCard *m_statTotal = nullptr;
    PanshiUi::PanshiStateCard *m_statBooted = nullptr;
    PanshiUi::PanshiStateCard *m_statDeletable = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiEmptyState *m_empty = nullptr;
    QList<Panshi::DeployInfo> m_deploys;
    bool m_running = false;
    bool m_colsSized = false;
};
