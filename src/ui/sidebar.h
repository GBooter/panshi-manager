// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file sidebar.h
 * 分组导航侧边栏（UI 重构方案 §3.1）
 *
 * 用 QListView + QStandardItemModel + 自绘 delegate 实现：
 *  - 分组标题行（概览 / 系统维护 / 设置）：TextTips 小字，不可选中；
 *  - 导航项：主题图标 + 文字，选选态 = Highlight 柔和底 + Highlight 文字
 *    （跟随用户主题色），hover = ItemBackground；
 *  - 键盘可达：↑/↓ 移动（自动跳过分组行）、Enter 激活、Tab 可进入焦点，
 *    焦点框由 QStyle::PE_FrameFocusRect 绘制（修复原 NoFocus + outline:none）。
 *
 * ⚠️ 2026-10-06：按用户要求**移除了设置搜索框**（原 DLineEdit + 结果列表）。
 * 侧栏退化为纯导航。页面侧的 searchableSettings() / highlightSetting() 能力
 * **保留未动**（其它入口与后续恢复搜索都要用），只是本控件不再提供搜索 UI，
 * 也不再消费 SettingIndex。相关成员与接口见下方「已移除」注释。
 *
 * 不使用 Q_OBJECT：激活回调经 std::function 暴露，避免 AUTOMOC 依赖。
 */

#include "components.h"

#include <QListView>
#include <QStandardItemModel>
#include <functional>

DWIDGET_USE_NAMESPACE

namespace PanshiUi {

class PanshiSidebar : public QWidget
{
public:
    explicit PanshiSidebar(QWidget *parent = nullptr);

    /** 追加一个分组标题行；其后的 addItem 都归属该组 */
    void beginGroup(const QString &title);

    /**
     * 追加导航项。
     * @param pageData     PageId 的 int 值，经 activated 回调原样返回
     * @param iconName     首选主题图标名
     * @param fallbackIcon 图标主题缺失时的备选主题图标名（可空）
     */
    void addItem(const QString &iconName, const QString &fallbackIcon,
                 const QString &text, int pageData);

    /** 按 pageData 选中导航项（只更新选中态，不触发 activated）；无匹配返回 false */
    bool setCurrentData(int pageData);

    // ---- 已移除（2026-10-06，用户要求移除侧栏搜索）----
    //
    //   void setSettingIndex(const SettingIndex *index);
    //   std::function<void(int pageId, const QString &targetKey)> settingActivated;
    //   QLineEdit/DLineEdit *m_search;   QListView *m_results;
    //   QStandardItemModel *m_resultModel;   QWidget *m_resultsHost;
    //   const SettingIndex *m_index;
    //   eventFilter() 覆写（搜索态 ↑↓/Esc 键盘导航专用）
    //
    // 恢复搜索时按此清单回填即可；SettingIndex 与页面侧接口均未删除。

    /** 导航项被点击/回车激活时回调，参数为 addItem 传入的 pageData */
    std::function<void(int)> activated;

private:
    QListView *m_view;
    QStandardItemModel *m_model;
};

} // namespace PanshiUi
