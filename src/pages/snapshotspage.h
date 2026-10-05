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
 * 备份快照子页（阶段 3-6 重写）：
 *  - 反馈走 PanshiInlineBanner（成功自动消失 / 失败常驻可关闭），不再弹模态窗；
 *  - 「备份数量上限 / 回滚安全备份」策略提示条改为 Banner 样式，不再用自绘 QSS 框；
 *  - 表内「初始」「回滚前」标签改用 PanshiBadge（柔和语义底 + 语义色文字）；
 *  - 空列表改用 PanshiEmptyState（含「创建备份」主操作）；
 *  - 列宽：固定列预算 443px（按默认窗口态视口倒推）+ 名称列弹性；
 *    操作列用 QHeaderView::Fixed + kOperationColWidth(170) 固定宽度，
 *    保证「还原」「删除」两个按钮在极窄窗口下仍完整可读
 *    （Stretch 模式下 setColumnWidth 无效，操作列必须 Fixed，详见 .cpp 注释）；
 *  - 窄窗兜底：表格横向 SizePolicy=Ignored + 自身横向滚动条 AsNeeded，
 *    避免外层 makeScrollArea（ScrollBarAlwaysOff）把右端操作列静默裁掉
 *    （第三轮验证发现的残留问题，详见 .cpp 与进度文档 §十二）；
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
    void updatePolicyText(int count, const Panshi::RollbackState &state);
    /** 只读地定位快照，避免 onDelete/onRollback 里手写查找循环 */
    const Panshi::SnapshotInfo *findSnapshot(const QString &id) const;
    /** 后台线程侧的数据装配（纯函数，不触碰 UI） */
    static QVariant loadPayload();

    MainWindow *m_win = nullptr;
    QTableWidget *m_table = nullptr;
    PanshiUi::PanshiInlineBanner *m_policyBar = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiEmptyState *m_empty = nullptr;
    QList<Panshi::SnapshotInfo> m_snaps;
    QMap<QString, QPair<qint64, qint64>> m_sizes;
    bool m_running = false;
    bool m_colsSized = false;
};
