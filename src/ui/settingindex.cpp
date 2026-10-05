// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingindex.h"

#include <algorithm>

namespace PanshiUi {

namespace {

/**
 * 匹配强度：0 = 不匹配；分数越高越靠前。
 *   3 = 标题前缀命中（用户大概率在打标题的开头）
 *   2 = 标题子串命中
 *   1 = 说明子串命中
 */
int matchScore(const SettingEntry &e, const QString &kw)
{
    if (kw.isEmpty())
        return 0;
    if (e.title.startsWith(kw, Qt::CaseInsensitive))
        return 3;
    if (e.title.contains(kw, Qt::CaseInsensitive))
        return 2;
    if (e.description.contains(kw, Qt::CaseInsensitive))
        return 1;
    return 0;
}

} // namespace

void SettingIndex::add(const SettingEntry &e)
{
    if (e.title.isEmpty() || e.targetKey.isEmpty())
        return;
    // 落地键唯一：同标题的条目（同一行被两个页面注册）只保留首个
    for (const SettingEntry &old : m_entries) {
        if (old.targetKey == e.targetKey && old.pageId == e.pageId)
            return;
    }
    m_entries.append(e);
}

QList<SettingEntry> SettingIndex::query(const QString &kw) const
{
    QList<SettingEntry> hits;
    if (kw.trimmed().isEmpty())
        return hits;

    const QString needle = kw.trimmed();
    QList<QPair<int, SettingEntry>> scored;
    for (const SettingEntry &e : m_entries) {
        const int s = matchScore(e, needle);
        if (s > 0)
            scored.append({s, e});
    }

    // 分数降序；同分保持注册顺序（QList::sort 不保证稳定，故用 std::stable_sort）
    std::stable_sort(scored.begin(), scored.end(),
                     [](const QPair<int, SettingEntry> &a,
                        const QPair<int, SettingEntry> &b) { return a.first > b.first; });

    hits.reserve(scored.size());
    for (const auto &p : scored)
        hits.append(p.second);
    return hits;
}

} // namespace PanshiUi
