// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "../backend.h"
#include "ui/components.h"

#include <DLabel>
DWIDGET_USE_NAMESPACE

class MainWindow;
class StorageBar;   // 本页私有：存储分布堆叠条（overviewpage.cpp 内实现，无 Q_OBJECT）
/**
 * 存储分布条/图例的系列身份色（图表用色，非语义状态色）。
 * 为什么自建枚举而不是 DPalette::ColorType：DTK 的 ColorType 没有 Highlight
 * （强调色在继承的 QPalette::ColorRole 里），TextTips/TextWarning 又只在
 * ColorType 里 —— 两套枚举混用会在调用点产生歧义（编译实测）。
 * 本枚举在 overviewpage.cpp 统一映射到对应角色，条与图例走同一函数保证同色。
 */
enum class BarRole { Accent, Warn, Tips, Track };
namespace PanshiSwitch { class PanshiSwitchRow; }

/**
 * 总览页（UI 重构方案 §3.3 P1 + P0 目标结构）。
 *
 * 结构：页头(带刷新) → ① 状态 hero（开关行，Surface 面板承载）
 *      → ② 响应式状态卡组 → ③ 存储分布条（堆叠条 + 图例）
 *      → ④ 系统信息 → ⑤ 快速操作。
 * 相对旧版的三处修正（2026-10-06 重设计）：
 *  1. hero：原「磐石功能」section 标题与行内标题重复，去掉 section 包装，
 *     开关行直接由 Surface 面板承载，成为首屏视觉锚点；
 *  2. 存储空间：2×2 纯数字网格 → 堆叠占比条 + 图例，回答"空间去哪了"；
 *     卡片里的「系统盘剩余 / 快照大小」不再与存储区重复（快照卡只报个数）。
 *  3. 页头新增刷新按钮（设计文档 P0 [刷新]）。
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
    /** 存储分布图例项：色点 + 名称 + 值；色点颜色取自 BarRole（与堆叠条同源） */
    QWidget *makeLegendRow(const QString &name, BarRole role, DLabel **valueOut = nullptr);

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

    /** 存储分布堆叠条（L2 第二步取数后填充） */
    StorageBar *m_storageBar = nullptr;

    DLabel *m_valSys = nullptr;
    DLabel *m_valVer = nullptr;
    DLabel *m_valKernel = nullptr;
    DLabel *m_valDeploy = nullptr;
    DLabel *m_valRepo = nullptr;
    DLabel *m_valPrepo = nullptr;
    DLabel *m_valSnap = nullptr;
    DLabel *m_valFree = nullptr;
};
