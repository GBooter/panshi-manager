// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file sidebar.h
 * 分组导航侧边栏（UI 重构方案 §3.1）
 *
 * 用 QListView + QStandardItemModel + 自绘 delegate 实现：
 *  - 分组标题行（概览 / 系统维护 / 设置）：TextTips 小字，不可选中；
 *  - 导航项：主题图标 + 文字，选中态 = Highlight 柔和底 + Highlight 文字
 *    （跟随用户主题色），hover = ItemBackground；
 *  - 键盘可达：↑/↓ 移动（自动跳过分组行）、Enter 激活、Tab 可进入焦点，
 *    焦点框由 QStyle::PE_FrameFocusRect 绘制（修复原 NoFocus + outline:none）。
 *
 * 不使用 Q_OBJECT：激活回调经 std::function 暴露，避免 AUTOMOC 依赖。
 */

#include "components.h"
#include "settingindex.h"

#include <DLineEdit>
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

    /**
     * 绑定设置搜索索引（阶段 4）。
     *
     * 传入 nullptr 或从不调用时，搜索框隐藏——侧栏退化为纯导航，
     * 便于在未提供索引的场景（测试、裁剪构建）保持可用。
     */
    void setSettingIndex(const SettingIndex *index);

    /**
     * 搜索命中并落地时回调，参数为该条目的 pageId 与 targetKey。
     * 由 MainWindow 实现"切页 + 定位 + 高亮"。
     */
    std::function<void(int pageId, const QString &targetKey)> settingActivated;

    /** 导航项被点击/回车激活时回调，参数为 addItem 传入的 pageData */
    std::function<void(int)> activated;

protected:
    /**
     * 搜索态键盘导航：↑↓ 移动结果高亮、Esc 清空关键词。
     *
     * 装在**输入框**与结果列表两处（原因见 .cpp 构造函数注释）：
     * 搜索态下焦点始终在 lineEdit，↑↓ 会被 QLineEdit 消费为输入历史导航，
     * 而结果列表永远拿不到焦点（点它会立即落地切页）。
     * 装在持有焦点的控件上，是这类「列表+输入框」组合的唯一可靠做法。
     */
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    void onSearchTextChanged(const QString &text);
    void refreshResults();
    void activateResult(int row);
    /** 结果为空且有关键词时，显示"无匹配"提示行 */
    void showNoResultHint();

    DLineEdit *m_search = nullptr;
    QListView *m_results = nullptr;
    QStandardItemModel *m_resultModel = nullptr;
    /** 结果列表行的 UserRole 存 targetKey，UserRole+1 存 pageId */
    QWidget *m_resultsHost = nullptr;

    QListView *m_view;
    QStandardItemModel *m_model;

    const SettingIndex *m_index = nullptr;
};

} // namespace PanshiUi
