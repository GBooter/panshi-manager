// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend.h"
#include "ui/components.h"

#include <QCoreApplication>
#include <QString>

/**
 * 「只读保护 / 可写层 / 重启后状态」三张状态卡的视图映射 —— 页面与探针的**单一定义点**。
 *
 * 为什么单独成文件：`panshi-protectionprobe` 要断言这几张卡的文案与语义色，
 * 而断言必须打在**页面真正使用的那份映射**上。若探针另抄一份，页面改了、
 * 探针不改，就会「探针全绿而故障依旧」——本项目已有同类教训（O9 注册漏调、
 * V-23 颜色角色实测不符）。与 `usageSemanticForPct` / `kUsageWarnPct`
 * 同一范式：判据一处定义，页面与断言同时生效。
 *
 * ---------------------------------------------------------------------------
 * 职责切分（本文件只管「视图」，不管「取数」）
 *
 * 本文件把「一个布尔 → 一张卡的文案与语义色」定义为纯函数。**各页自己决定那个
 * 布尔怎么来**，且**故意不合并**两页的取数口径：
 *
 *   | 页面           | 卡 1「只读保护」取值              | 数据源                          |
 *   |----------------|----------------------------------|---------------------------------|
 *   | ProtectionPage | `!w.enable`                      | `getWritableStatus()`（ctl `status -j` 解析 `conf.enable`，**fail-open**：出错返回 false） |
 *   | OverviewPage   | `isImmutable()`                  | `ctl -s` 解析 `Immutable mode`，**fail-closed**：出错返回 true  |
 *
 * 两者是**不同命令**、报**不同维度**（全局不可变模式 vs 可写层开关）、**失败行为还相反**。
 * 强行并成一个函数会在任一后端出错时产出「只读保护=已开启 且 可写层=已开启」这类
 * 自相矛盾的卡片（失败行为相反 ⇒ 两个信号会朝相反方向同时翻转）。故保持分离，
 * 本文件只保证「同一个布尔在两页渲染出同一张卡」。
 *
 * ---------------------------------------------------------------------------
 * 2026-10-05 变更 ①：卡 3 由「重启后处理」改为「重启后状态」
 *
 * 旧实现常态（enable=false，即绝大多数用户）填 `无(保护已开启)`，四个问题：
 *   ① 与卡 1「只读保护 = 已开启」信息重复；
 *   ② 括号在中文语境里默认被读成「包含关系」，信息量为零；
 *   ③ 更糟：被扫读成 `无保护 · 已开启`，把**安全状态读成危险状态**；
 *   ④ 语义色用 Muted（灰）—— 灰在状态卡里等于「未生效 / 空卡」，
 *      与「系统本就安全」的事实相反。
 *
 * 新定位是「重启后的结果」，三张卡由此构成一条时间轴：
 *   当前保护 → 当前可写层 → 重启后
 * 三个分支恒有实义：主值 = 重启后的结果，徽标 = 当前设置的定性。
 *
 * ---------------------------------------------------------------------------
 * 2026-10-05 变更 ②：卡 1 / 卡 2 迁入本文件（此前两页各抄一份）
 *
 * 此前 `ProtectionPage::onLoaded` 与 `OverviewPage::onLoadedFast` 各自内联
 * 了卡 1/卡 2 的文案与语义色。两份副本已经漂移过：概览页用 `开启/关闭`、
 * 保护页用 `已开启/已关闭`；且 `已开启/已关闭` 同时用于卡 1 与卡 2 而**含义相反**
 * （卡 1 已开启 = 保护生效 = 安全；卡 2 已开启 = 可写层已开 = 危险），
 * 同词同色反义，横排极易误读。现由本文件统一，两页与探针共用同一份。
 */
namespace PanshiProtection {

/**
 * 翻译上下文。
 *
 * ⚠️ 用 `Q_DECLARE_TR_FUNCTIONS` 而不是手写 `QCoreApplication::translate(ctx, s)`：
 *    lupdate 只在「`tr(...)`」或「`translate(字面量, 字面量)`」处提取文本，
 *    先包一层 `Tr::tr(const char *s)` 再转发的写法**提取不到**——实测改完跑 lupdate
 *    报 `0 new`，新文案静默丢失，无任何编译告警，只在英文界面才暴露。
 */
class Tr
{
    Q_DECLARE_TR_FUNCTIONS(PanshiProtection)
};

/** 一张状态卡的视图：主值 + 徽标 + 各自语义色 */
struct CardView
{
    QString value;
    PanshiUi::Semantic valueSem = PanshiUi::Semantic::Normal;
    QString status;
    PanshiUi::Semantic statusSem = PanshiUi::Semantic::Muted;
};

/** 应用到一张卡上：视图 → 控件。页面与探针都走这条，避免两处各写一遍 setter */
inline void applyCard(PanshiUi::PanshiStateCard *card, const CardView &v)
{
    card->setValue(v.value, v.valueSem);
    card->setStatus(v.status, v.statusSem);
}

// ---------------------------------------------------------------- 标题
// 三个标题与映射同源同生：若某个标题只存在于页面里而漏进本文件，探针就无法
// 渲染出真实卡片（探针要靠标题文案定位标签，见 probeRealCard）。

inline QString protectCardTitle()
{
    return Tr::tr("只读保护");
}

inline QString writableCardTitle()
{
    return Tr::tr("可写层");
}

inline QString rebootCardTitle()
{
    return Tr::tr("重启后状态");
}

// ---------------------------------------------------------------- 卡 1
/**
 * 卡 1「只读保护」：核心目录当前是否处于只读保护下。
 *
 * @param readOnlyProtected 只读保护**当前是否生效**。注意两页的取数口径不同，
 *        见本文件头部的职责切分表 —— 本函数只负责「给定这个结论怎么渲染」。
 */
inline CardView protectCard(bool readOnlyProtected)
{
    CardView c;
    c.value     = readOnlyProtected ? Tr::tr("已开启") : Tr::tr("已关闭");
    c.valueSem  = readOnlyProtected ? PanshiUi::Semantic::Success : PanshiUi::Semantic::Danger;
    c.status    = readOnlyProtected ? Tr::tr("核心目录只读") : Tr::tr("核心目录可写");
    // 徽标分工：正常态一律 Muted 中性注释、异常态跟随值语义色复述告警。
    // 正常态曾用 Success 蓝，与卡 2/卡 3 的灰徽标不一致，且让正常态多出一处彩色、
    // 稀释「值 = 全卡唯一状态色」的规矩。
    c.statusSem = readOnlyProtected ? PanshiUi::Semantic::Muted : PanshiUi::Semantic::Danger;
    return c;
}

// ---------------------------------------------------------------- 卡 2
/**
 * 卡 2「可写层」：可写层当前是否已启用。
 *
 * ⚠️ 文案刻意用「解锁 / 锁定」而**不是**「已开启 / 已关闭」：后者原先同时用于
 *    卡 1（已开启 = 保护生效 = 安全）与卡 2（已开启 = 可写层已开 = 危险），
 *    同词同色反义，横排极易误读。「锁定/解锁」贴合「可写层」的锁语义，彻底避开撞车。
 */
inline CardView writableCard(bool writableEnabled)
{
    CardView c;
    c.value     = writableEnabled ? Tr::tr("已解锁") : Tr::tr("已锁定");
    c.valueSem  = writableEnabled ? PanshiUi::Semantic::Warning : PanshiUi::Semantic::Success;
    c.status    = writableEnabled ? Tr::tr("开发调试模式") : Tr::tr("未启用");
    c.statusSem = writableEnabled ? PanshiUi::Semantic::Warning : PanshiUi::Semantic::Muted;
    return c;
}

// ---------------------------------------------------------------- 卡 3
/**
 * 卡 3「重启后状态」：预测「当前设置重启后会变成什么」。
 *
 * @param w 当前可写层设置。**这是本文件唯一直接吃后端结构体的映射** ——
 *        它天然可共享，故不受「两页取数口径不同」的限制。
 */
inline CardView rebootCard(const Panshi::WritableStatus &w)
{
    CardView c;
    if (w.enable) {
        const bool tmp = w.clearAfterReboot;
        c.value     = tmp ? Tr::tr("恢复只读") : Tr::tr("仍保持可写");
        c.valueSem  = tmp ? PanshiUi::Semantic::Warning : PanshiUi::Semantic::Danger;
        c.status    = tmp ? Tr::tr("临时关闭") : Tr::tr("永久关闭");
        c.statusSem = c.valueSem;
    } else {
        // 常态：本来只读，重启后还是只读 —— 正向结果，用 Success 与卡 1/卡 2 的正常态
        // 一致，形成「整体正常」的观感；用 Muted 会被误读成「未生效 / 空卡」。
        // 徽标只作辅助说明，保持 Muted 以区分主次。
        c.value     = Tr::tr("仍为只读");
        c.valueSem  = PanshiUi::Semantic::Success;
        c.status    = Tr::tr("未做变更");
        c.statusSem = PanshiUi::Semantic::Muted;
    }
    return c;
}

} // namespace PanshiProtection
