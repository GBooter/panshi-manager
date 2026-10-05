// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"
DWIDGET_USE_NAMESPACE

class QScrollArea;
class MainWindow;
namespace PanshiSwitch { class PanshiSwitchRow; }

/**
 * 系统保护页（阶段 3-4 重写）：
 *  - 状态区改 PanshiStateCard（数值 + 状态徽章），替代自绘 statCard + QSS stat 属性；
 *    容器用 PanshiResponsiveCardRow，窄窗自动折行（阶段 3-8 统一）；
 *  - 常规操作平铺，「永久/临时关闭保护」收进 PanshiCollapsible 危险操作区（方案 §3.3 P4）；
 *  - 反馈走 PanshiInlineBanner（成功自动消失 / 失败常驻可关），不再弹模态窗；
 *  - 读取与设置走 L2 局部加载，不再盖整页。
 *  - 卡片行宽度由 BasePage 统一喂入（viewport Resize + showEvent 双路径）。
 */
class ProtectionPage : public BasePage
{
    Q_OBJECT
public:
    explicit ProtectionPage(MainWindow *win);

    void refresh() override;

    /** 自报可搜索设置项（阶段 4 设置搜索） */
    QList<PanshiUi::SettingEntry> searchableSettings() const override;

    /**
     * 对外动作契约（阶段 5 跨页跳转）：高亮「开启保护（恢复只读）」行。
     *
     * 用途：备份页遇到「可写层冲突 → 无法还原」时跳来本页，用户需要立刻
     * 知道该点哪个按钮。不做"自动执行"，只做定位——关闭可写层是危险操作，
     * 必须由用户在知情前提下自己确认。
     */
    Q_INVOKABLE void highlightEnableProtect();

private:
    void setProtect(bool protect, bool clearAfterReboot);
    void onLoaded(const QVariant &res);

    MainWindow *m_win = nullptr;
    PanshiSwitch::PanshiSwitchRow *m_switchRow = nullptr;
    PanshiUi::PanshiResponsiveCardRow *m_statRow = nullptr;
    PanshiUi::PanshiStateCard *m_cardProtect = nullptr;
    PanshiUi::PanshiStateCard *m_cardWritable = nullptr;
    PanshiUi::PanshiStateCard *m_cardReboot = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    /** 「开启保护（恢复只读）」行：供跨页跳转定位高亮（阶段 5） */
    PanshiUi::PanshiRow *m_rowEnable = nullptr;
    /** 本页内容滚动区：定位高亮时把目标行滚入视野 */
    QScrollArea *m_scroll = nullptr;
    bool m_running = false;
};
