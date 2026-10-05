// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"

#include "../backend.h"
DWIDGET_USE_NAMESPACE

class QTableWidget;
class MainWindow;

/**
 * 系统版本（部署）子页（阶段 3-6 重写）：
 *  - 反馈走 PanshiInlineBanner，不再弹模态窗；
 *  - 「当前」「回滚前」标签改用 PanshiBadge；
 *  - 空状态改用 PanshiEmptyState；
 *  - 列宽改 Interactive + 初始宽度，避免窄窗口下操作列被挤出；
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
    void onUndeploy(int index);
    void applyColumnWidths();
    /** 按行下标定位部署；下标越界时返回 nullptr（删除前调用方会复核 commitId） */
    const Panshi::DeployInfo *findDeploy(const QString &commitId, int index) const;

    MainWindow *m_win = nullptr;
    QTableWidget *m_table = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiEmptyState *m_empty = nullptr;
    QList<Panshi::DeployInfo> m_deploys;
    bool m_running = false;
    bool m_colsSized = false;
};
