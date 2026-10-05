// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file settingindex.h
 * 设置搜索索引（UI 重构方案 §4.x / 阶段 4）
 *
 * 背景：设置项散落在多个页面（策略与设置页 6 项、系统保护页的磐石功能开关 1 项），
 * 用户要找某一项时必须先猜它在哪个页面。本索引把"设置项"从页面结构里抽出来，
 * 支持按关键词直接定位。
 *
 * 设计取舍：
 *  - **不是全局单例**：由 MainWindow 持有一个实例，各页在构造时注册。
 *    与 registerResponsiveCardRow / setFeedbackBanner 同一"注册点"范式，
 *    避免组件层反向依赖具体页面。
 *  - **匹配不做分词**：量级只有十余条，标题/说明的子串匹配足够；
 *    加权（标题命中 > 说明命中）保证"备份数量上限"优先于说明里提到"备份"的行。
 *  - **不持有控件指针**：条目只记录 pageId + 标题，落地时由页面按标题自己定位。
 *    这样索引不必关心页面何时重建，也避免悬垂指针。
 *
 * 不使用 Q_OBJECT（纯数据 + 查询，无信号需求）。
 */

#include <QList>
#include <QString>

namespace PanshiUi {

/** 一条可搜索的设置项 */
struct SettingEntry
{
    int pageId = -1;      // MainWindow::PageId 的 int 值（避免 ui 层包含 mainwindow.h）
    QString group;        // 分组名（如「备份保留策略」），用于结果列表的次级说明
    QString title;        // 设置项标题（主要匹配对象）
    QString description;  // 设置项说明（次要匹配对象）
    /** 页面内定位用的落地键：PanshiRow 的标题，页面据此找到对应行并高亮 */
    QString targetKey;
};

/**
 * 设置搜索索引。
 * 查询结果按「匹配强度」降序：标题前缀命中 > 标题子串命中 > 说明子串命中。
 */
class SettingIndex
{
public:
    /** 追加一条设置项（重复 title 会被忽略，保证落地键唯一） */
    void add(const SettingEntry &e);

    /** 按关键词查询；kw 为空返回空列表 */
    QList<SettingEntry> query(const QString &kw) const;

    /** 已注册条目数（调试/自检用） */
    int count() const { return m_entries.size(); }

    /** 清空（页面重建时重新注册） */
    void clear() { m_entries.clear(); }

private:
    QList<SettingEntry> m_entries;
};

} // namespace PanshiUi
