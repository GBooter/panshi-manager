// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file tokens.h
 * 磐石设置 UI 度量 token
 *
 * 约束（UI 重构方案 §2.2）：
 *  1. 本文件只允许描述「尺寸 / 间距 / 时长」，禁止出现任何颜色与 hex 色值。
 *  2. 颜色一律经 components.h 从 DPalette / DGuiApplicationHelper 取得。
 *  3. Radius::Frame 与 DTK 的 DStyle::PM_FrameRadius 当前取值(8)一致；
 *     待构建环境可访问 DStyle 后可改为运行时取值。
 */

namespace PanshiUi {

/** 间距：4px 基准 */
namespace Space {
constexpr int XS = 4;
constexpr int S = 8;
constexpr int M = 12;
constexpr int L = 16;
constexpr int XL = 24;
constexpr int XXL = 32;
} // namespace Space

/** 圆角：只保留两档 */
namespace Radius {
constexpr int Element = 4;   // 徽章 / 标签 / 进度条 / 行内高亮
constexpr int Frame = 8;     // 分组容器 / 卡（对齐 DTK PM_FrameRadius）
} // namespace Radius

/** 尺寸 */
namespace Size {
constexpr int IconSmall = 16;
constexpr int Icon = 20;
constexpr int IconLarge = 24;
constexpr int IconHero = 32;
constexpr int Separator = 1;
constexpr int RowMinHeight = 48;
/**
 * 行式设置项右侧「控件区」的显式最小宽度。
 *
 * 存在的理由：布局空间不足时 Qt 会把子项压到各自 minimumSizeHint，而
 * DSpinBox / DComboBox 的 minimumSizeHint 极小（几十像素），会被压成「…」
 * 甚至不可见；同时左侧说明因 wordWrap=true 反向"抢"宽度。显式最小宽度
 * 是挤压下唯一可靠的防线（与表格列的 setMinimumWidth 同一课）。
 * 取 200：够放下「告警冷却 [数值] 小时」这类带后缀的控件 + 若干间距。
 */
constexpr int RowControlMinWidth = 200;
/**
 * 状态卡（PanshiStateCard）的显式最小宽度。
 *
 * 与 RowControlMinWidth 同一课（显式最小宽度是挤压下唯一可靠的防线）：
 * 卡片内是「大数值 T2 + 标签 T6 + 徽章」，其 minimumSizeHint 由最长的一行
 * 文本决定，宽度需求大且随内容浮动。若不设下限，卡片行在窄窗下会先被压扁
 * 再横向溢出——而外层 makeScrollArea 是 ScrollBarAlwaysOff，溢出部分会被
 * 静默裁掉（总览页实测：第 4 卡 GB 被截、行内开关被推出视口不可见）。
 *
 * 取 168：够放下「46.89 GB」这类 T2 大数值 + 卡片内边距（Space::L × 2）。
 * 配套用法：卡片行容器用 QGridLayout + 按可用宽度切列数（见 overviewpage.cpp
 * 的 relayoutStatCards），下限保证「单卡不会先塌」，切列数保证「不横向溢出」。
 */
constexpr int StateCardMinWidth = 168;
constexpr int Spinner = 20;
constexpr int ThinProgress = 2;
/**
 * 概览「已用比例」条的槽高（磁盘页）。
 *
 * ⚠️ 缺陷 O2-1 二次修正（2026-10-02 复测 V-16 像素级实测）：
 *   我原先在此写的是「带内嵌文字的进度条 = 18px」，并解释为「14px 装不下
 *   文字行高」——**该判断是错的**。提到 18px 后文字仍不显示。
 *   真实原因：**DTK6 的 DProgressBar 样式不绘制内嵌 format 文字**
 *   （`setTextVisible(true)` 在继承链上有效，但样式层未实现
 *   `CE_ProgressBarLabel`）。故百分比已改为**外置标签**（见 UsagePctWidth）。
 *   → 本 token 回归「纯形状条」语义：14px 足够，且更紧凑。
 *
 * 教训：控件「某个 setXxx 调了但没效果」时，先查**样式层是否实现**，
 * 不要先怀疑尺寸/裁剪。文档写有该绘制入口 ≠ 实现存在。
 */
constexpr int UsageBarHeight = 14;
/** 外置百分比标签的固定宽度：容纳「100.0%」且不因位数变化抖动 */
constexpr int UsagePctWidth = 56;
constexpr int MinTouchTarget = 36;
} // namespace Size

/** 动效时长（毫秒）；"减少动态效果"开启时一律归零 */
namespace Motion {
constexpr int Micro = 150;
constexpr int State = 250;
constexpr int PulsePeriod = 1200;
constexpr int PulseStep = 60;
/**
 * 定位高亮的「峰值保持」与「淡出」时长（设置搜索落点 / 跨页跳转定位）。
 *
 * 峰值 1200 与 PulsePeriod 同值但**语义不同**（一个是骨架屏明暗周期，
 * 一个是高亮停留时长），故各自具名，不互相引用——将来调其中之一时
 * 不必担心牵连另一个。
 *
 * 为何需要这个量：设置页不是列表，没有"当前项"概念，只靠一次短促的
 * 视觉提示告诉用户"目标在这一行"——峰值必须足够长，让视线能从搜索框
 * 移到该行；淡出必须够快，避免像是常驻选中态。
 */
constexpr int HighlightHold = 1200;
constexpr int HighlightFade = 320;
} // namespace Motion

/** 透明度（0~255） */
namespace Alpha {
constexpr int SoftFill = 38;   // ≈15% 柔和底（徽章等小面积元素）
constexpr int BannerFill = 51; // ≈20% 提示条底（整条面积大，需比 SoftFill 更实）
constexpr int Outline = 51;    // 20% 描边
} // namespace Alpha

} // namespace PanshiUi
