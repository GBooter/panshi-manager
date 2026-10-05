// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "../backend.h"
#include "ui/components.h"

#include <DLabel>
DWIDGET_USE_NAMESPACE

class MainWindow;
namespace PanshiSwitch { class PanshiSwitchRow; }

/**
 * 总览页（UI 重构方案 §3.3 P1）。
 *
 * 结构：功能总开关 → 状态卡 → 系统信息 → 存储空间 → 快速操作。
 * 加载态：首次进入用 L1 骨架屏，其后一律 L2 局部加载——本页永不盖整页遮罩。
 */
class OverviewPage : public BasePage
{
    Q_OBJECT
public:
    explicit OverviewPage(MainWindow *win);

    void refresh() override;

private:
    void onLoadedFast(const QVariant &res);
    void onLoadedSizes(const QVariant &res);

    /** 行式信息项：左侧字段名 + 右侧值；返回值标签以便回填（阶段 4 搜索索引可读） */
    PanshiUi::PanshiRow *makeInfoRow(const QString &label, DLabel **valueOut = nullptr);

    MainWindow *m_win = nullptr;
    bool m_firstLoad = true;

    PanshiSwitch::PanshiSwitchRow *m_switchRow = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiSkeleton *m_skeleton = nullptr;
    /** 页级反馈条：承接磐石开关切换的执行结果（成功/失败），阶段 4 */
    PanshiUi::PanshiInlineBanner *m_feedback = nullptr;
    /**
     * 状态卡容器：列数随可用宽度自适应（组件层实现）。
     * 宽度由 BasePage::registerResponsiveCardRow 统一喂入——
     * 它同时监听 viewport Resize 与 showEvent，覆盖隐藏页首次显示的路径。
     */
    PanshiUi::PanshiResponsiveCardRow *m_statRow = nullptr;

    PanshiUi::PanshiStateCard *m_cardProtect = nullptr;
    PanshiUi::PanshiStateCard *m_cardWritable = nullptr;
    PanshiUi::PanshiStateCard *m_cardSnap = nullptr;
    PanshiUi::PanshiStateCard *m_cardFree = nullptr;

    QList<Panshi::SnapshotInfo> m_snaps;

    DLabel *m_valSys = nullptr;
    DLabel *m_valVer = nullptr;
    DLabel *m_valKernel = nullptr;
    DLabel *m_valDeploy = nullptr;
    DLabel *m_valRepo = nullptr;
    DLabel *m_valPrepo = nullptr;
    DLabel *m_valSnap = nullptr;
    DLabel *m_valFree = nullptr;
};
