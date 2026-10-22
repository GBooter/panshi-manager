// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"

#include "../backend.h"
#include <DLabel>
DWIDGET_USE_NAMESPACE

class QTableWidget;
class MainWindow;

/**
 * 备份快照子页（阶段 3-6 重写 + 2026-10-06 二轮重设计）：
 *  - 反馈走 PanshiInlineBanner（成功自动消失 / 失败常驻可关闭），不再弹模态窗；
 *  - 「备份数量上限 / 当前数量 / 回滚前安全备份」策略提示改为 **3 张状态卡**
 *    （PanshiStateCard + PanshiResponsiveCardRow），替代原先的整条 Banner ——
 *    状态是「数据」不是「提醒」，卡片化的标签+大值更直观，窄窗自动折行不溢出；
 *  - 表内「初始」「回滚前」标签改用 PanshiBadge（柔和语义底 + 语义色文字）；
 *  - 空列表改用 PanshiEmptyState（含「创建备份」主操作）；
 *  - 列宽：**全部列 Interactive（可拖拽调整）**——原「名称 Stretch / 操作 Fixed」
 *    把最重要的两列拖拽手柄废掉了（Stretch 列手柄无效），用户反馈列宽不可调。
 *    现全列可拖：名称改回固定初始宽度（180），操作列保留下限钳制
 *    （sectionResized 弹回 kOperationColWidth，按钮不被截字）；无 Stretch 列后
 *    宽窗留白由用户拖宽「名称」吸收，窄窗由表格内部横向滚动条兜底；
 *  - 窄窗兜底：表格横向 SizePolicy=Ignored + 自身横向滚动条 AsNeeded，
 *    避免外层 makeScrollArea（ScrollBarAlwaysOff）把右端操作列静默裁掉；
 *  - 还原流程把"还原对话框 + 二次确认"收敛为单次决策（摘要 + 后果 + 策略单选）；
 *  - 任务进行中用 L2 局部加载（setMaskLoadingEnabled(false)），不再盖整页。
 */
class SnapshotsPage : public BasePage
{
    Q_OBJECT
public:
    explicit SnapshotsPage(MainWindow *win);

    void refresh() override;

private slots:
    void onCreate();
    void onDelete(const QString &id);
    void onRollback(const QString &id);
    void onLoaded(const QVariant &res);

private:
    void render(const QList<Panshi::SnapshotInfo> &snaps,
                const QMap<QString, QPair<qint64, qint64>> &sizes,
                const Panshi::RollbackState &state);
    void doRollback(const QString &id, const QString &name, const QString &strategy);
    void applyColumnWidths();
    /** 刷新三张策略状态卡（备份上限 / 当前数量 / 回滚前安全备份） */
    void updatePolicyCards(int count, const Panshi::RollbackState &state);
    /** 只读地定位快照，避免 onDelete/onRollback 里手写查找循环 */
    const Panshi::SnapshotInfo *findSnapshot(const QString &id) const;
    /** 后台线程侧的数据装配（纯函数，不触碰 UI） */
    static QVariant loadPayload();

    MainWindow *m_win = nullptr;
    QTableWidget *m_table = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    /** 策略状态三卡（替代原整条 Banner：状态是数据，不是提醒） */
    PanshiUi::PanshiResponsiveCardRow *m_statRow = nullptr;
    PanshiUi::PanshiStateCard *m_statLimit = nullptr;
    PanshiUi::PanshiStateCard *m_statCount = nullptr;
    PanshiUi::PanshiStateCard *m_statRollback = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiEmptyState *m_empty = nullptr;
    QList<Panshi::SnapshotInfo> m_snaps;
    QMap<QString, QPair<qint64, qint64>> m_sizes;
    bool m_running = false;
    bool m_colsSized = false;
};
