# 磐石设置 UI 重构设计方案

> 版本：v1.0 · 目标工程：`panshi-manager`（C++17 / Qt6 / DTK6）· 适用 deepin/UOS v25
> 本文只做设计与实施方案，不含代码实现。落地时按第 5 章分阶段执行。

---

## 0. 现状基线（分析结论）

### 0.1 项目结构与功能模块

```
panshi-manager (可执行文件)
├── src/main.cpp              DApplication 入口；--monitor 无界面监控模式；--show-disk D-Bus 定位转发
├── src/mainwindow.{h,cpp}    DMainWindow + DTitlebar + 自绘 QFrame 侧边栏 + QStackedWidget 页面栈
├── src/style.h               全局 QSS（亮/暗两套硬编码 hex 调色板）+ 自绘 SVG 线性图标库(15 个)
├── src/backend.{h,cpp}       磐石命令封装（ostree / deepin-immutable-ctl / -writable / systemctl），全同步
├── src/pages/
│   ├── common.h              Ui 辅助：IconBadge 自绘徽章 / makeScrollArea / pageHeader / card / button / rowLabel
│   ├── basepage.{h,cpp}      页面基类：runAsync(QtConcurrent) + 全页加载遮罩(DSpinner)
│   ├── overviewpage.*        总览
│   ├── diskusagepage.*       磁盘占用（本工程最复杂的页面，约 1450 行）
│   ├── versionpage.*         备份管理外壳（DTabBar 容器）
│   ├── snapshotspage.*       备份快照（QTableWidget 7 列）
│   ├── deployspage.*         系统版本/部署（QTableWidget 6 列）
│   ├── protectionpage.*      系统保护
│   ├── policypage.*          备份策略（应用级 + 系统级参数混排）
│   ├── updatepage.*          系统更新
│   └── aboutpage.*           关于
├── src/widgets/
│   ├── usagechart.*          自绘时间线（柱状/折线自适应、生长动画、扫描/检查动画覆盖层）
│   ├── scanoverlay.*         扫描动画面板     ├── radarwidget.*   雷达动画
│   ├── checkoverlay.*        检查步骤面板     └── countdowndialog.*  30 秒倒计时危险确认
└── src/disk/                 磁盘监控子系统（GUI + --monitor 进程共用）
```

**页面与索引映射（关键）**：`0 总览 / 1 磁盘占用 / 2 备份管理 / 3 系统保护 / 4 备份策略 / 5 系统更新 / 6 关于`

**技术特征**：DTK6 控件已基本到位（DPushButton / DSuggestButton / DWarningButton / DSwitchButton / DDialog / DLineEdit / DTextEdit / DSpinBox / DComboBox / DTabBar），主题跟随系统，字体已用 DFontSizeManager 分级。**但仍有一层全局 QSS 在覆盖 DTK 主题系统**——这是所有视觉问题的根因。

### 0.2 现有 UI 实现方式总结

| 层面 | 现状实现 |
|---|---|
| 窗口外壳 | `DMainWindow` + `DTitlebar`(46px 固定高) + 自绘 `QFrame#panshiSidebar`(210px 固定宽) |
| 导航 | 7 个 `DPushButton` 竖排，`setCheckable` + `setAutoExclusive` 模拟单选，`setFocusPolicy(NoFocus)` |
| 页面容器 | `QStackedWidget` + 每页 `QScrollArea` 包裹（横向滚动条强制关闭） |
| 配色 | `style.h::panshiStyleSheet()` 返回 ~170 行 QSS，亮/暗各一套 hex 常量，`themeTypeChanged` 时整体重刷 |
| 图标 | `common.h` 内嵌 15 个手写 SVG 字符串 + `IconBadge` 自绘控件（圆角底 + `QSvgRenderer` 渲染到 `QPixmap`），配一张静态指针表做主题刷新 |
| 卡片 | `DFrame` + QSS（`background/border/border-radius:10px`） |
| 列表 | 原生 `QTableWidget` + QSS（斑马纹、`gridline-color:transparent`、圆角） |
| 加载态 | `BasePage` 全页半透明遮罩 + 居中 `DSpinner` |
| 对话框 | `DDialog`（确认/信息/错误），**每个页面各写一份匿名命名空间函数，重复约 5 份** |
| 图表 | `UsageChart` 自绘（`QPainter`，颜色硬编码 `#0081FF/#E64545/#F5A623`） |

### 0.3 已确认的功能性缺陷（UI 重构必须一并修复）

> 以下不是审美问题，而是**当前会导致用户操作到错误页面**的缺陷，优先级最高。

`gotoPage()` 使用裸下标，且下标在功能演进中已错位：

| 位置 | 文案 | 实际跳转 | 应为 |
|---|---|---|---|
| `overviewpage.cpp:121` | 「立即创建备份」 | `gotoPage(1)` → 磁盘占用 | `gotoPage(2)` 备份管理 |
| `overviewpage.cpp:123` | 「检查系统更新」 | `gotoPage(4)` → 备份策略 | `gotoPage(5)` 系统更新 |
| `overviewpage.cpp:125` | 「保护设置」 | `gotoPage(2)` → 备份管理 | `gotoPage(3)` 系统保护 |
| `snapshotspage.cpp:375` | 可写层冲突「前往设置」 | `gotoPage(2)` → 备份管理（即当前页，等于无响应） | `gotoPage(3)` 系统保护 |

另：`mainwindow.cpp` 的 `m_pages.at(3)` / `m_pages.at(0)` 同样依赖位置假设；README 与开发文档中的页面顺序说明也与代码不一致。

---

## 1. 现有 UI 存在的问题清单

### 1.1 视觉风格

| # | 问题 | 具体表现 | 影响 |
|---|---|---|---|
| V1 | **全局 QSS 覆盖 DTK 主题系统** | `style.h` 硬编码 `#F7F8FA/#FFFFFF/#1E1E1E/#0081FF/#E64545/#00A870/#F5A623/#7B61FF` 等 | 违反 DTK 规范（应使用 `DPalette`）。QSS 优先级高于 DPalette，导致部分控件跟主题、部分不跟；用户改**主题色**时全应用不变；QSS 未覆盖到的控件（`QCalendarWidget`、`QGroupBox`、`QScrollBar`）会掉回系统默认样式，形成"两种风格拼在一起" |
| V2 | **强调色写死为 `#0081FF`** | 徽章、进度条 chunk、链接按钮、选中态、图表主色 | 不跟随「控制中心 → 个性化 → 主题色」。应为 `DPalette::Highlight` / `DPalette::LightLively` |
| V3 | **自绘 15 个内嵌 SVG 图标 + IconBadge** | `common.h` 里 `ICON_SHIELD`…`ICON_HELP` 手写路径，`IconBadge` 用 `QSvgRenderer` 渲染 | 绕过 `DIconTheme`，不随系统图标主题（DCI/bloom）变化；每个徽章一个独立 QWidget + 一次位图渲染，且用全局静态指针表维护生命周期，属不必要的自造轮子 |
| V4 | **品牌渐变 logo 是 Web 风格** | 侧边栏 36px 方块 + `qlineargradient(#0081FF→#00A870)` + 文字「磐」；关于页 56px 同款 | deepin 系统工具不使用彩色渐变方块做应用标识；且与标题栏的 `panshi.svg` 图标重复表达 |
| V5 | **圆角与间距无统一体系** | 卡片 10px、徽章 12px、导航项 8px、标签 4px、按钮交给 QSS；卡片内边距有 (22,20) (20,18) (18,16) (16,14) 四种 | 同类容器视觉节奏不一致；未使用 `DStyle::PM_FrameRadius` 等 DTK 度量 |
| V6 | **层次靠描边而非背景** | 卡片 `border:1px solid #E4E7ED`，无阴影 | deepin 的分组容器靠"页面底色 vs 卡片底色"的色差 + 圆角表达，1px 描边叠加在浅灰底上显得"表格化、生硬" |
| V7 | **图标权重过大** | 状态卡 46px / 页面标题 48px / 卡片标题 40px / 徽章底色 `rgba(0,129,255,26)` | 图标比文字更抢眼，与 deepin 克制的图标层级不符；状态卡中徽章独占一行进一步挤压数值文本 |
| V8 | **状态色退化为装饰色** | 总览 4 张卡分别 `blue/orange/green/purple`；「系统盘剩余」用紫色语义不明 | 颜色不再承载"正常/警告/危险"，用户无法靠颜色做判断 |
| V9 | **异常柱大面积标红** | `UsageChart` 中所有超过 `minAlertMB` 的日子画成 `#E64545` | 截图里 7 根柱子 6 根红——用户读到的信息是"系统到处告警"，与事实不符，是严重的传达错误 |
| V10 | **同一语义三种实现** | 提示条：`diskusage` 用 `DLabel` 加背景 QSS，`countdowndialog` 用 `QFrame`，`updatepage` 用 `DLabel` | 间距、圆角、图标、可关闭性各不相同 |
| V11 | **表格样式自定制** | `QTableWidget` + `alternate-background-color` 斑马纹 + 圆角 QSS | 与 DTK 列表规范不一致；斑马纹在 deepin 中一般不使用 |

### 1.2 布局结构

| # | 问题 | 具体表现 | 影响 |
|---|---|---|---|
| L1 | **页头重复渲染** | 外层 `VersionManagementPage` 有 `pageHeader("备份管理")`，内层 `SnapshotsPage` 又有一个 `pageHeader` 并带「刷新/创建备份」按钮；「系统版本」tab 下 `DeploysPage` 同样再渲染一次大标题 | 截图中「备份管理」出现两次，页面层级感崩塌，用户不知道自己在第几层 |
| L2 | **侧边栏无分组、无图标、混入技术信息** | 7 项平铺：「关于」与功能项同级；底部写「Deepin 25 · 基于 OSTree」 | 7 项已接近平铺导航的可读上限，且把实现细节暴露给普通用户；导航项无图标，纯文字定位效率低 |
| L3 | **信息架构重叠** | 总览有"系统信息/存储空间/快速操作"；磁盘占用页另有一套 hero 概览；系统保护页的"说明"卡与总览状态卡重复 | 同一事实在 2~3 处出现，维护与一致性成本高 |
| L4 | **磁盘占用页纵向过长、单卡过载** | "时间线"卡内塞入：图例 4 项 + 预设按钮 5 个 + 自定义按钮 + 提示行 + 图表 + 加载状态行 | 首屏信息密度不均；相关控件（范围选择）属于不同层级却挤在同一卡 |
| L5 | **栅格不响应** | 状态卡 `grid->addWidget(card, 0, i)` 恒为 4 列；磁盘汇总恒 4 列；无列伸缩/换行策略 | 最小宽 920px 时每张卡约 190px，「已用 8.60 GB」「已开启(保护关闭)」被迫换行或被压扁 |
| L6 | **卡片内边距/间距四种规格** | `cardLayout`(22,20,22,20)/12、统计卡(18,16,18,16)/8、miniStat(16,14,16,14)/2、hero(20,18,20,18)/10 | 见 V5 |
| L7 | **所有内容强制套 QScrollArea** | 每页 `makeScrollArea()` + `ScrollBarAlwaysOff` | 短页面也带滚动容器（多余嵌套）；长页面滚动条样式未定制，与背景割裂 |
| L8 | **表格列宽策略不良** | 快照表 7 列全 `ResizeToContents` + 描述列 `Stretch`；部署表同 | 截图中「系统初始备份」被折成「Sy… ini…」，「操作」列被挤出可视区，"描述"列却占了最宽 |
| L9 | **对话框尺寸写死** | 磁盘监控设置 `QScrollArea` 固定 540×560；自定义范围对话框 `setFixedWidth(520)` 内塞两个 `QCalendarWidget` | 最小窗口高 620px 时设置弹窗超出屏幕；范围选择需要滚动很长的对话框 |
| L10 | **最小尺寸与内容不匹配** | 窗口默认 1060×700、最小 920×620；`DTitlebar` 固定 46px | 在 1366×768 上默认尺寸偏大，最小尺寸下内容仍溢出（依赖滚动兜底，而非自适应） |
| L11 | **空状态用表格里的文字行充当** | 排行空态 `setItem(0,0,"暂无数据：请开启监控…")`；备份空态 `setSpan` 合并格写一行字 | 无图形、无主操作入口，用户不知道下一步该点什么 |

### 1.3 交互体验

| # | 问题 | 具体表现 | 影响 |
|---|---|---|---|
| I1 | **全页加载遮罩** | `BasePage::showLoading` 用覆盖整页的 `rgba(0,0,0,0.05)` 遮罩 + 居中 DSpinner；`runAsync` 每次调用都会出现 | 总览页 `refresh()` 分两步（快速状态 → 存储统计）会**连闪两次全屏遮罩**；切页刷新整页变灰，已读内容被"擦掉" |
| I2 | **开关操作触发整页重刷** | 拨动「磐石功能」→ `refreshPanshiPages()` → `m_pages.at(3)->refresh()` + 若可见则 `m_pages.at(0)->refresh()` | 一次开关动作引发两次全页遮罩 + 重算存储，反馈延迟明显 |
| I3 | **同一开关在两个页面各持一个实例** | 总览页 `PanshiSwitchRow(page, true)` 与系统保护页 `PanshiSwitchRow(page, false)` 各自持有 `DSwitchButton`，靠 `refreshPanshiPages()` 互相同步 | 无单一数据源；同步逻辑脆弱；两处间距/`withDetail` 行为不同，用户会疑惑"这是同一个开关吗" |
| I4 | **危险确认逻辑重复 5 份** | `snapshots/deploys/policypage/protectionpage/masterswitch/diskusage` 各自有 `confirmDialog/infoDialog/errorDialog` | 按钮类型（`ButtonWarning`/`ButtonRecommend`）、文案、图标、默认焦点各不相同；如「删除部署」的确认按钮文案写死为"删除"，而其它页是"确定" |
| I5 | **确认按钮不用动词** | `confirmDialog(..., tr("确定"))` 反复出现 | 违背 deepin「按钮文案用动作动词」的惯例，用户需回头读标题才知道会发生什么 |
| I6 | **还原流程需要 4 次交互** | 可写层检查对话框 → 还原对话框（含 `QGroupBox` + 3 个单选）→ 二次确认对话框 → 执行后"是否立即重启"对话框 | 流程过长；中间对话框信息过载；`QGroupBox`/`QRadioButton` 是 Qt 原生样式，视觉上"跳出"DTK |
| I7 | **成功反馈一律模态弹窗** | `infoDialog(tr("设置成功! 请重启系统使设置生效。"))` | 轻量结果也要用户点"确定"，打断节奏。deepin 惯例是内联提示/Toast |
| I8 | **错误直接暴露后端 stderr** | `errorDialog(tr("设置失败: %1").arg(r.errorMessage))` | 用户看到的是命令输出而非可行动的说明 |
| I9 | **表格行内塞按钮** | 操作列固定 150px（快照表）/120px（部署表），内放 `DPushButton`「还原」「删除」 | 见 L8，操作列被挤出屏幕；且每行按钮都是独立 widget，无 hover 语义、无右键菜单 |
| I10 | **无键盘可达性** | 导航项 `setFocusPolicy(Qt::NoFocus)` + QSS `#panshiNavItem:focus { outline: none }`；表格无键盘操作约定 | Tab 无法遍历主功能，键盘用户无法使用；无障碍不合格 |
| I11 | **无搜索** | 7 页 + 磁盘监控 10+ 设置项 + 策略 6 项，无任何搜索入口 | 找设置靠翻页；deepin 控制中心标配搜索 |
| I12 | **范围选择无选中态** | 7/30/90/180/365 天是 5 个普通 `DPushButton` | 点过之后不知道当前生效的是哪个范围；「自定义」与预设之间也无互斥关系 |
| I13 | **图表语义不完整** | Y 轴只有 `0.0 GB / 11.6 GB …` 刻度，未标注这是"总占用"；hover tooltip 有日期/已用/增长但图上无提示；无坐标轴标题 | 用户需要猜；柱状图红蓝含义只能从图例反推 |
| I14 | **监控设置是模态弹窗内滚动** | `DDialog` 内嵌 `QScrollArea(540×560)`，"关闭"是唯一按钮，控件即时生效但无保存反馈 | 用户无法确认设置是否生效；"关闭"语义模糊（关闭窗口还是放弃修改） |
| I15 | **策略页两种保存模型混排** | 应用级（`auto_prune`/`max_snapshots`）即时写 `~/.config/panshi-manager/config.json`；系统级（回滚/部署数/阈值）要点「保存系统参数」写 `/etc/...conf` | 同一页面两类行为，用户无法预期哪个是即时生效 |
| I16 | **关闭窗口直接中断任务** | `closeEvent` 遍历 `shutdown()` | 扫描/还原进行中时无提示静默取消 |
| I17 | **技术内容占用主界面** | 关于页"常用命令参考"占据半页长文本 | 高级用户内容未折叠，普通用户被迫观看命令 |
| I18 | **自绘动画未覆盖"主题色变更"** | `DiskUsagePage::onThemeChanged` 只监听 `themeTypeChanged` | 用户只改主题色（不改亮暗）时，图表与徽章不重绘 |

---

## 2. 整体设计思路与重构目标

### 2.1 核心思路：**"退掉自绘，交还 DTK"**

现状的根本问题是**在 DTK 之上又叠了一层自研的 Web 化设计系统**（QSS 色板 + 手写 SVG 图标 + 自绘徽章 + 自绘图表配色）。这条路会持续产生"跟主题不彻底、风格不统一、维护成本高"三类问题。

重构方向是**把视觉控制权交还 DTK**：

```
现在： 应用自研 QSS 色板 + 手写图标 ──覆盖──> DTK 控件
重构： 应用只表达"结构与语义" ──查询──> DPalette / DStyle / DIconTheme / DFontSizeManager
```

应用层只保留三件事：
1. **结构与层级**（哪些内容属于同一组、层级多深）
2. **语义**（这是"危险"、"提示"、"成功"、"主操作"）
3. **设计 token 常量**（间距/圆角/尺寸），且 token 的**取值来源必须是 DStyle 与 DPalette**，而不是 hex

### 2.2 对齐 deepin 设计语言的具体规则

#### 配色（必须全部来自 DPalette）

| 用途 | 现状 | 重构后 |
|---|---|---|
| 窗口/页面底 | QSS `#F7F8FA` | `DPalette::Window`（由 DTK 提供） |
| 卡片/分组底 | QSS `#FFFFFF` / `#2A2A2A` | `DPalette::Base`（浅色为白、暗色为深，DTK 自动切换） |
| 分隔线/描边 | QSS `#E4E7ED` | `DPalette::FrameBorder` |
| 标题文字 | `DPalette::TextTitle` ✅ 已对 | 保持；正文用 `TextTitle`，说明用 `TextTips` |
| 次要/说明文字 | `DPalette::TextTips` ✅ 已对 | 保持 |
| **强调色 / 选中态** | ❌ 写死 `#0081FF` | **`DPalette::Highlight`**（跟随系统主题色） |
| 主按钮 | `DSuggestButton` ✅ | 保持（DTK 内部使用 `LightLively/DarkLively`，本身不受主题色影响，这是 deepin 规范） |
| 危险/警告文字 | 写死 `#E64545` / `#F5A623` | `DPalette::TextWarning`；危险操作容器用 `DPalette::ObviousBackground` 加深 |
| 成功状态 | 写死 `#00A870` | 用 `DPalette::TextLively` 或回落到 `Highlight`；**不再新增自定义绿色** |
| 列表项 hover/选中 | QSS `#F2F4F7` / `#E8F3FF` | `DPalette::ItemBackground`（配合 `DStyleHelper::generatedBrush` 自动处理 hover/press） |

**硬性规则**：
- 新增视觉必须能回答"这个颜色来自哪个 `DPalette::ColorType`"，回答不了就不准写。
- `style.h` 中 17 个 hex 语义色常量全部删除；`style.h` 最终只保留**非颜色的少量度量 QSS**（若确实需要），或整体删除。
- 语义色克制到 4 类：正常（TextTitle）、提示（TextTips）、警告（TextWarning）、强调（Highlight）。禁止再用紫色等"装饰性语义色"。

#### 字体（DFontSizeManager，已在用，需统一映射）

| 层级 | 组件 | 现状 | 规范 |
|---|---|---|---|
| 页面标题 | `pageHeader` 主标题 | T3 | **T2**（与 DTitlebar 拉开层次），全部页面统一 |
| 卡片标题 / 分组标题 | `cardTitle` | T5 | T5 |
| 正文 / 行标签 | `rowLabel` | T5 | T5 |
| 值（大数字） | 统计卡 | T2 | T2（与页面标题同级是刻意的：数值是主角） |
| 值（小数字） | miniStat | T4 | T4 |
| 说明 / 描述 | `rowDesc` | T6 | T6 |
| 辅助/标签/徽章 | 徽章、图例 | T7 | T7 |
| 等宽（命令、ID） | `panshiCmd` | T8 | T8 + 等宽字体（经 `DFontManager` 获取，不写 `font-family: monospace`） |

**清除**：所有 QSS 中的 `font-weight: 600/700`。字重用 `DLabel::setWeight`（DTK6 提供）或 `DStyle` 的字体，不用 QSS。

#### 圆角与阴影（层次）

- 圆角取值统一两档：**分组容器 = `DStyle::pixelMetric(DStyle::PM_FrameRadius)`**（DTK 当前为 8）；**小元素（徽章/标签/进度条）= 4**。不再出现 10/12/16 等自定义值。
- **不使用 box-shadow**。层次表达 = 页面底色（`Window`）与卡片底色（`Base`）的差异 + 8px 圆角 + 16~24px 间距。这是 deepin 控制中心的实际做法。
- 需要"浮起"时（如固定在顶部的操作条）使用 DTK 的 `DFrame` + `DPalette::FrameShadowBorder`，而非自绘阴影。

#### 控件替换清单

| 现用 | 替换为 | 理由 |
|---|---|---|
| `QTableWidget` + QSS | **`DListView` + `DStyledItemDelegate`** | 获得圆角选中背景、hover、`setActionList(Qt::Edge, ...)` 行尾操作按钮、`setBackgroundType(RoundedBackground)` |
| `QListWidget`（排除目录） | `DListView` + `DStyledItemDelegate` | 同上 |
| `QProgressBar` + QSS | **`DProgressBar`** | DTK 原生、自动跟主题；容量展示可用 `waterProgressBar` |
| `QGroupBox`（还原对话框） | `DFrame` + 行式 `DRadioButton` 组 | Qt 原生样式与 DTK 割裂 |
| `QCalendarWidget`（自定义范围） | DTK 日期控件（`DDateEdit` / `DCalendarWidget` 若可用，否则包装为符合 DTK 尺寸的单日历） | 见 L9 |
| `DPushButton`（导航项） | `DListView` + delegate，或 DTK 侧栏控件 | 获得分组、图标、键盘方向键、正确焦点框 |
| 自绘 `IconBadge` | `DIconTheme::findQIcon("<基础名称>")` + `DLabel`/`DIconButton` | 见 V3 |
| `QScrollBar`（默认） | 交给 DTK；如需定制，统一在一处 | 见 L7 |

**图标命名映射（用主题图标名，不自绘）**：

| 语义 | 建议图标名 |
|---|---|
| 磐石保护 / 系统保护 | `security-high` 或 `changes-prevent` |
| 可写层 / 修改 | `document-edit` / `edit-rename` |
| 快照 / 备份 | `document-save` / `document-save-as` |
| 时间 / 计划 | `appointment-soon` / `preferences-system-time` |
| 磁盘 / 存储 | `drive-harddisk` / `drive-harddisk-symbolic` |
| 系统监视 | `utilities-system-monitor` |
| 策略 / 设置 | `preferences-system` / `preferences-other` |
| 更新 | `software-update-available` / `system-software-update` |
| 关于 | `help-about` / `dialog-information` |
| 终端 / 命令 | `utilities-terminal` |
| 打开位置 | `folder-open` |
| 刷新 | `view-refresh` |
| 搜索 | `edit-find` |
| 更多 | `view-more` / `open-menu-symbolic` |

落地时逐个用 `DIconTheme::findQIcon()` 验证非空（不可用则回退到 DTK builtin 名称）。

#### 动效

- 保留并扩展到所有新动效：读取 `org.deepin.dde.appearance enable-animations`（复用现有 `UsageChart::reduceMotionRequested()`），关闭时所有过渡时长归零。
- 动效时长统一：微交互 150ms，状态切换 250ms，不使用 >400ms 的动画。
- **不再为"加载"做大面积动画**：全页雷达/扫描动画只保留在 `UsageChart` 内部的局部区域（扫描是长任务，局部动画合理），其余场景用骨架屏或行内进度。

### 2.3 可验收的重构目标

| 维度 | 验收标准 |
|---|---|
| 配色 | `grep -rE '#[0-9A-Fa-f]{6}' src/` 在 UI 目录下命中数 = 0（`data/icons/panshi.svg` 除外） |
| 主题 | 在控制中心切换 **亮/暗 × 主题色（≥3 种）**，全应用无残留不一致；`QCalendarWidget`/`QGroupBox` 不再出现 |
| 导航正确性 | 所有跨页跳转有 e2e 校验，`gotoPage(数字)` 命中数 = 0，改为 `gotoPage(PageId)` |
| 加载态 | 首屏以外的刷新不出现全页遮罩；连续两次数据加载不再闪两次 |
| 可读性 | 920×620 与 1366×768 下：无横向滚动、无文本截断（elide + tooltip 兜底）、主操作可见 |
| 键盘 | Tab 可遍历「侧边栏 → 页头操作 → 内容 → 主操作」；焦点框可见（非自绘 outline）；方向键可在导航与列表间移动 |
| 危险操作 | 全应用只有 1 个确认组件；按钮文案为动作动词；破坏性操作默认焦点在"取消" |
| 反馈 | 成功类操作 0 次模态弹窗（除需用户决策的"立即重启"） |
| 无障碍 | 所有图标按钮有 `toolTip`；关键控件有 `accessibleName` |

---

## 3. 界面布局与信息架构调整方案

### 3.1 导航结构重构

**现在**（7 项平铺，无分组）：

```
总览 / 磁盘占用 / 备份管理 / 系统保护 / 备份策略 / 系统更新 / 关于
```

**重构后**（分组 + 图标，用 `DListView` + 分组标题）：

```
┌──────────────────────────────┐
│  ▣  总览                     │   ← 分组「概览」
├──────────────────────────────┤
│  ▣  磁盘空间                 │   ← 分组「系统维护」
│  ▣  系统保护                 │
│  ▣  备份与版本   ▸           │
│  ▣  系统更新                 │
├──────────────────────────────┤
│  ▣  策略与设置               │   ← 分组「设置」
│  ▣  关于                     │
└──────────────────────────────┘
```

改动说明：
1. **「磁盘占用」→「磁盘空间」**：用户语言更自然（"占用"偏技术）。
2. **「备份管理」→「备份与版本」**：名称直接说明它同时管理两类对象。
3. **「备份策略」→「策略与设置」**：它实际包含应用级策略与系统级参数，且未来还会长。
4. 加分组标题（概览 / 系统维护 / 设置），7 项分成 3 组后单组最多 4 项，扫描成本显著下降。
5. 每项加 16~20px 主题图标（见 2.2 映射表）。
6. **删除**侧边栏里重复的「磐石设置 / 磐石系统管理」标题块与底部「Deepin 25 · 基于 OSTree」（标题栏已有应用名；技术信息移到关于页）。
7. 侧边栏顶部（或标题栏右侧）加入 **`DSearchEdit` 搜索框**。

### 3.2 页面外壳（统一）

所有页面使用同一外壳，**页头只由框架渲染一次**：

```
┌──────────────────────────────────────────────────────────────┐
│ DTitlebar  [图标] 磐石设置            [搜索] [菜单] [窗口按钮] │
├────────────┬─────────────────────────────────────────────────┤
│            │  页头区（框架渲染，全应用唯一）                   │
│  侧边栏     │  ┌──────┐                                        │
│  (分组+图标) │  │ 图标 │  页面标题 (T2)      [页级操作按钮区]    │
│            │  └──────┘  一句话副标题 (T6 · TextTips)           │
│            ├─────────────────────────────────────────────────┤
│            │  内容区（页面自行组织）                            │
│            │  ┌─ 分组容器 (Base 底 + 8px 圆角) ─────────────┐  │
│            │  │ 分组标题 (T5)                   [组级操作]  │  │
│            │  │ ─────────────────────────────────────────  │  │
│            │  │ 行：标签(T5) + 描述(T6/Tips)     控件  │  │
│            │  └───────────────────────────────────────────┘  │
│            ├─────────────────────────────────────────────────┤
│            │  反馈区：内联提示条 / 顶部进度条（按需，常驻高位）  │
└────────────┴─────────────────────────────────────────────────┘
```

关键约束：
- **页头唯一性**：`VersionManagementPage` 渲染页头，其子页 `SnapshotsPage` / `DeploysPage` **不得**再渲染页头；它们只渲染 tab 栏 + 工具条 + 内容。
- **页级操作**统一放页头右侧（如备份页的「刷新」「创建备份」；磁盘页的「立即扫描」「监控设置」）。
- **组级操作**放分组容器右上（如"排除目录"分组的「添加」）。
- **反馈区位**固定在内容区顶部，不覆盖内容（区别于现在的全页遮罩）。

### 3.3 逐页组成与层级

#### P0 总览

```
页头：总览 · 磐石系统运行状态一览        [刷新]
─────────────────────────────────────────────
① 状态 hero（唯一开关实例所在）
   图标 + 「磐石保护已开启」+ 一句结论 + 生效说明
   [恢复只读保护]（主操作）        [详情 → 系统保护页]
   —— 右侧放 DSwitchButton（唯一实例，与系统保护页共享同一模型）
─────────────────────────────────────────────
② 概览卡组（响应式：≥1100px 四列 / 768~1100 两列 / <768 单列）
   保护状态｜可写层｜备份快照数｜系统盘可用
   统一三行结构：大值(T2) + 标签(T6) + 状态徽章(T7)
   颜色只用 正常/警告/危险 三档
─────────────────────────────────────────────
③ 存储分布（新）
   一条横向堆叠占比条：/ostree ｜ /persistent ｜ 快照 ｜ 可用
   下方 4 项图例（色点 + 名称 + 数值）
   —— 比现在 4 个孤立数字更能回答"空间去哪了"
─────────────────────────────────────────────
④ 系统信息（定义列表，非网格）
   系统名称 / 系统版本 / 内核版本 / 磐石部署
   两列 label-value，值用 TextTitle，label 用 TextTips，长值 elide + tooltip
─────────────────────────────────────────────
⑤ 快速操作
   [创建备份]  [检查更新]  [保护设置]   ← 图标 + 文字按钮，跳转必须正确
```

层级：`总览` → 5 个分组容器，**扁平无嵌套**。移除现在把"系统信息/存储空间"拆成两卡的重复表述。

#### P1 磁盘空间

```
页头：磁盘空间 · 持续监控系统盘，定位容量增长来源     [立即扫描] [监控设置]
─────────────────────────────────────────────
① 容量概览卡
   环形/水位进度（DProgressBar） + 「已用 8.60 GB / 39.04 GB (22.0%)」
   + 监控服务状态徽章（运行中 / 未启动 / 未开启，带一键启动）
   + 最近告警：可关闭的内联条（危险色），点击跳到对应日期
─────────────────────────────────────────────
② 时间线卡
   卡头：卡片标题 + [图例折叠] + Y 轴单位说明「总占用 (GB)」
   范围选择：分段控件（互斥选中态）  [7天|30天|90天|180天|365天] [自定义]
   提示行：「当前显示：系统盘占用变化」  [返回系统盘]（仅目录模式显示）
   图表区（固定高度，最小 220px）
   加载状态：图表内底部一行细进度（不再全页遮罩）
─────────────────────────────────────────────
③ 分析区（DTabBar：区间汇总 / 增长详情 / 占用排行）
   ③a 区间汇总：4 个指标（2×2 网格，窄屏 1 列）
                + 容量预测提示条（可关闭）
                + 「区间增长 Top 项」列表（DListView，非长文本堆）
   ③b 增长详情：说明折叠为 tooltip/问号图标
                + DListView（名称 / 大小 / 占比 / 行尾「打开位置」图标按钮）
                + 空状态：图形 + 「该日无显著增长」
   ③c 占用排行：面包屑路径（可点击回退）+ 返回上级
                + DListView（单击选中→时间线联动；双击下钻）
                + 空状态：图形 + 「开启监控」主 CTA
─────────────────────────────────────────────
④ 监控设置（从模态弹窗改为页内分组，或独立子页「磁盘监控设置」）
   分组「监控」：启用开关 + 采样间隔 + 说明
   分组「告警规则」：告警模式 + 阈值/倍数 + 冷却（控件随模式启用/禁用）
   分组「排除目录」：输入框+添加按钮 + DListView（行尾删除）
   分组「数据存储」：保留天数 + 存储预算 + 当前占用进度 + 清除数据（危险）
   所有改动即时保存 + 行内「已保存」微反馈
```

层级：`磁盘空间` → 4 个分组 → 分析区含 3 个 tab。**关键改动是把"监控设置"从弹窗降为页内分组**，消除 L9/I14。

#### P2 系统保护

```
页头：系统保护 · 核心目录只读保护（可写层）管理       [刷新]
─────────────────────────────────────────────
① 状态 hero
   图标 + 「只读保护：开启」+ 「可写层：关闭」+ 重启后行为说明
─────────────────────────────────────────────
② 状态卡组（3 列，响应式）
   只读保护 ｜ 可写层 ｜ 重启后处理
─────────────────────────────────────────────
③ 保护开关（唯一实例区）
   DSwitchButton + 左右状态文案（开启 / 关闭）
   开关下方的解释文字随状态变化（关→"当前可写，系统面临风险"）
─────────────────────────────────────────────
④ 操作分组
   主操作：[恢复只读保护]（DSuggestButton）
   次要：  [临时关闭（重启后自动恢复）]
   高级：  ▸ 「危险操作」折叠区 → [永久关闭保护]（DWarningButton）
   折叠区展开时显示后果清单（复用统一确认组件的能力）
─────────────────────────────────────────────
⑤ 说明（折叠）
   ▸ 「了解磐石只读保护」→ 展开显示现在"说明"卡的内容
```

层级：`系统保护` → 5 个分组。**把"永久关闭"收进折叠的高级区**，避免三个同级危险按钮平铺（现在的做法让用户每次都要在三个危险按钮里挑）。

#### P3 备份与版本

```
页头：备份与版本 · 系统备份快照与已装系统版本管理     [刷新] [创建备份]
─────────────────────────────────────────────
tab：备份快照 ｜ 系统版本        ← 由外壳渲染，子页不再有页头
─────────────────────────────────────────────
【备份快照】
  工具条：搜索/筛选（全部｜初始｜回滚前） + 排序 + 数量上限说明
  DListView 列：名称(+标签) ｜ 创建时间 ｜ 占用空间 ｜ 描述 ｜ 操作
    操作 = 行尾「更多」图标按钮 → 菜单（还原 / 重命名 / 删除）
    初始备份的「删除」在菜单中禁用并说明原因
  空状态：图形 + 「还没有备份」+ [创建第一个备份]
  底部说明：改为 tooltip 或折叠"了解更多"
【系统版本】
  DListView 列：系统版本(+当前/回滚前标签) ｜ 状态 ｜ 部署时间 ｜ 提交ID ｜ 操作
    提交 ID：等宽字体 + 截断 + hover tooltip 全文 + 复制按钮
    操作 = 行尾「更多」→ 删除部署（当前启动项禁用）
  空状态 + 说明折叠
```

层级：`备份与版本` → 2 个 tab → 每个 tab 各自 1 个列表。**删除内层页头**（修 L1）。

#### P4 系统更新

```
页头：系统更新 · 通过原子更新机制升级系统            [检查更新]
─────────────────────────────────────────────
① 版本信息卡
   当前版本 ｜ 可用更新（有则显示版本号与大小）｜ 上次检查时间
─────────────────────────────────────────────
② 操作卡
   [下载并升级]（有更新时为主按钮，否则禁用并说明）
   [清理下载缓存]
─────────────────────────────────────────────
③ 升级阶段进度（仅升级中显示）
   阶段列表：准备 → 下载 → 校验 → 应用 → 完成
   当前阶段高亮 + 细进度条
   危险提示条：「升级期间请勿关机或断电」
─────────────────────────────────────────────
④ 结果区
   默认：一行结论（成功/失败 + 摘要）+ [显示详情 ▸]
   展开：等宽日志（可选中可复制）
─────────────────────────────────────────────
升级完成 → 内联结果条 + [立即重启]（唯一保留的模态询问）
```

层级：`系统更新` → 4 个分组（阶段进度按需显隐）。移除 `panshiResultBox` 那种"把命令输出直接贴在卡里"的做法。

#### P5 策略与设置

```
页头：策略与设置 · 备份保留策略与磐石系统参数         [刷新]
─────────────────────────────────────────────
① 分组「备份保留」  （应用级 · 即时保存）
   行：自动清理超额备份        开关
   行：备份数量上限            数字输入（0 = 不限）
   行：回滚前备份自动清理      下拉
   组尾：内联"已保存"微反馈
─────────────────────────────────────────────
② 分组「磐石系统参数」 （系统级 · 需管理员 · 显式保存）
   顶部说明条：「修改需管理员权限，点击保存后生效」
   行：升级失败自动回滚        开关
   行：最大备份部署数          数字输入
   行：自动刷新阈值            数字输入
   底部：[保存系统参数]（DSuggestButton，仅在脏状态启用）
   离开页面时若有未保存改动 → 内联提示条（不弹窗）
```

层级：`策略与设置` → 2 个分组。**两组用不同的保存模型，但通过视觉分隔 + 顶部说明 + 按钮状态把差异讲清楚**（修 I15）。

#### P6 关于

```
页头：关于
─────────────────────────────────────────────
① 应用信息卡（居中）
   应用图标（panshi.svg，非渐变方块）+ 名称 + 版本 + 许可 + 一句话描述
   [复制版本信息]
─────────────────────────────────────────────
② ▸ 「技术参考」折叠   → 现有"磐石系统技术参考"内容
③ ▸ 「常用命令」折叠   → 现有内容，每行加「复制」图标按钮
```

层级：`关于` → 1 卡 + 2 个折叠区。把 I17 的技术内容默认收起。

### 3.4 响应式断点

| 断点 | 宽度 | 侧边栏 | 卡片网格 |
|---|---|---|---|
| 紧凑 | < 1000px | 折叠为图标条（56px）或保持 200px | 概览卡 2 列；表单行标签换行到控件上方 |
| 标准 | 1000 ~ 1360px | 200px | 概览卡 2 列，分析指标 2×2 |
| 宽 | > 1360px | 220px | 概览卡 4 列，分析指标 1×4 |

窗口最小尺寸建议 **900×640**（并把最小尺寸下的布局作为回归基线，而非依赖滚动条）。

---

## 4. 关键交互流程优化方案

### 4.1 加载态分级（修 I1 / I2）

定义三档，按"是否已有可读内容"选择：

| 档位 | 场景 | 表现 |
|---|---|---|
| **L1 骨架屏** | 页面首次进入、无任何数据 | 内容区显示与最终结构同形的灰色占位块（卡片轮廓 + 若干行占位条），不遮罩全页、不禁用交互 |
| **L2 局部加载** | 已有内容，触发刷新/切换 tab | 目标容器内显示小 `DSpinner`（16~20px）+ 文字；**其余内容保持可读可交互** |
| **L3 顶部细进度条** | 长任务（扫描、还原、升级） | 内容区顶部 2px 不确定/确定进度条 + 操作按钮进入 loading 态；页面不灰 |

删除 `BasePage` 的全页遮罩机制，替换为这三个可复用的状态组件。

**总览页两次加载的处理**：第一次（基本状态，毫秒级）→ 直接填充数值，不显示任何 loading；第二次（存储统计）→ 在"备份快照""系统盘可用"两张卡内显示 L2 局部 loading，其余卡片已可读。

### 4.2 单一数据源：磐石保护状态（修 I2 / I3）

```
现在：OverviewPage.m_switchRow  ←互相同步→  ProtectionPage.m_switchRow   （两个 DSwitchButton）
重构：PanshiProtectionModel (单例)
        ├── state: {known, protectedOn, clearAfterReboot}
        ├── setProtection(bool on, Mode mode) → 异步执行 + 广播
        └── changed 信号
              ├── OverviewPage 订阅 → 只更新 hero 区域的文字与开关状态
              └── ProtectionPage 订阅 → 只更新状态卡与开关状态
```

要点：
- 全应用**只有一个 `DSwitchButton` 实例在任一时刻可见**；总览页展示状态 + 快捷开关，系统保护页展示完整操作。两者绑定同一模型。
- 状态变更后**只更新受影响的控件**，不调用 `refresh()`，杜绝整页重算。

### 4.3 危险操作统一组件（修 I4 / I5 / I6）

统一为一个 `PanshiConfirmDialog`，参数化：

```
PanshiConfirmDialog {
    标题           // "删除备份"
    图标语义       // 信息 / 警告 / 危险
    摘要块         // 受操作对象的键值摘要（如：目标备份 / 创建时间 / 描述）
    后果清单       // 逐条列出（危险级别 ≥ 警告时必填，带图标）
    确认按钮文案   // 必须是动词："删除" "还原" "完全关闭"
    危险级别       // 普通(Recommend) / 警告(Warning)
    倒计时秒数     // 仅"永久关闭保护"等不可逆高危操作使用，默认 0
    可展开的高级区 // 如"回滚前备份策略"三个单选，默认收起并预选推荐项
}
```

规则：
- 全应用**只有这一个确认组件的实现**，删除 5 份重复的 `confirmDialog/infoDialog/errorDialog`。
- 信息类、错误类不再用模态弹窗，改用**内联提示条**（见 4.4）。
- 默认焦点在"取消"；Esc 取消；危险按钮不设 `default`。
- 破坏性操作最多 **2 次交互**：选对象 → 确认对话框（内含高级选项）。

### 4.4 反馈策略（修 I7 / I8）

| 场景 | 现在 | 重构后 |
|---|---|---|
| 操作成功、无需后续动作 | 模态 `infoDialog("备份已删除")` | 内容区顶部内联成功条，3s 自动消失，可手动关闭 |
| 操作成功、需用户后续动作 | 模态 `infoDialog("请重启生效")` | 内联结果条 + 明确按钮（「立即重启」），条常驻直到用户处理 |
| 操作失败 | 模态 + 后端 stderr 原文 | 内联错误条 + **可行动文案**：「无法关闭保护，可能因为当前有还原任务在进行。请稍后重试。」+ [显示详情 ▸] 展开原始输出 |
| 设置已保存 | 无 | 组尾内联「已保存 ✓」，2s 淡出 |
| 需要用户决策 | 模态 | 保持模态（统一用 `PanshiConfirmDialog`） |

**文案规范**：错误信息 = "发生了什么" + "可能原因" + "可以怎么做"。原始命令行输出一律折叠在"详情"里。

### 4.5 导航与跨页跳转（修 0.3 缺陷）

1. 定义 `enum class PageId { Overview, DiskUsage, BackupVersion, Protection, Policy, Update, About }` 与 `enum class SubPageId { Snapshots, Deploys }`。
2. `MainWindow::gotoPage(PageId)` / `gotoPage(PageId, SubPageId)`；删除全部 `gotoPage(int)` 与 `m_pages.at(n)`。
3. 跨页跳转必须携带上下文：
   - 总览「创建备份」→ `gotoPage(BackupVersion, Snapshots)` 并**直接打开创建对话框**（现在只是跳页，用户还得再点一次）
   - 总览「检查更新」→ `gotoPage(Update)` 并**自动触发检查**
   - 备份页可写层冲突 → `gotoPage(Protection)` 并**高亮"恢复只读保护"按钮**（现在跳到自身，等于无响应）
4. 增加一个 e2e 校验：遍历所有跳转入口，断言落地页正确。

### 4.6 范围选择与图表（修 I12 / I13 / V9）

**范围选择**：改为分段控件（`DTabBar` 或自绘 segmented），5 个选项互斥选中，选中态用 `DPalette::Highlight` 背景 + `HighlightedText`。
「自定义」打开一个**单日历范围选择**（点击起始日、再点击结束日，中间高亮），替代现在两个纵排 `QCalendarWidget`。

**图表配色重做**：

| 元素 | 现在 | 重构后 |
|---|---|---|
| 正常柱 | `#0081FF` | `DPalette::Highlight` |
| 异常柱 | 整柱 `#E64545` | 主体仍用 `Highlight`，顶部加 `TextWarning` 三角形标记 + 描边；**不再整柱变红** |
| 选中日 | 橙色点 | `DPalette::Highlight` 描边 + 顶部指示三角 |
| 趋势线 | 蓝色点线 | `TextTips` 虚线 |
| 坐标轴 | 无标注 | Y 轴加单位与含义「总占用 (GB)」；X 轴按范围自动稀疏日期标签 |

**新增**：Y 轴含义说明（放卡头右侧）、「异常标记」显隐开关、hover tooltip 增加"较前一日 +X"、鼠标离开后 tooltip 淡出 150ms。
**空状态**：图表区居中显示图形 + 「暂无监控数据」+ [开启监控] / [立即扫描]。
**目录模式**：面包屑（`/` › `var` › `lib`）可点击逐级回退，替代现在单行的「返回上级」按钮。

### 4.7 键盘可达与无障碍（修 I10）

1. 删除 `setFocusPolicy(Qt::NoFocus)`；导航列表按 Tab 可达。
2. 删除 `#panshiNavItem:focus { outline: none }`；焦点框由 DStyle 绘制（`PM_FocusBorderWidth`）。
3. 导航列表支持 ↑/↓ 移动、Enter 激活、Home/End 跳首尾。
4. 内容区列表支持 ↑/↓ 选中、Enter 执行主操作、Delete/菜单键打开操作菜单。
5. 所有图标按钮必须 `setToolTip()`；关键控件 `setAccessibleName()`。
6. 对话框 Esc = 取消；主操作支持其对应快捷键。
7. 焦点顺序：侧边栏 → 页头操作 → 内容区 → 页脚/主操作，检查无"焦点黑洞"。

### 4.8 搜索（修 I11）

- 位置：标题栏右侧 `DSearchEdit`（`DTitlebar::setSearchWidget` 若可用）或侧边栏顶部。
- 索引范围：所有设置项的 `标签 + 描述关键词 + 所属页面`（需在每个设置项注册时提供 `QStringList keywords`）。
- 行为：输入即时展示下拉结果（分组显示"页面 / 设置项"），Enter 或点击 → 跳到对应页面并**高亮 + 滚动到该控件**（1.5s 高亮描边后淡出）。
- 磁盘监控的 10+ 设置项、策略的 6 项是主要受益者。

### 4.9 关闭窗口保护（修 I16）

`closeEvent` 中若存在进行中任务（扫描 / 还原 / 升级）：
- 显示统一确认对话框：「仍有任务进行中」+ 列出任务 + 「任务中断可能导致数据不完整」。
- 提供 [继续等待]（默认）/ [仍然退出]。
- 仅 `--monitor` 无界面模式不受影响。

---

## 5. 实施步骤与注意事项

### 5.1 阶段划分

> 每个阶段结束都应可构建、可运行、可截屏对比，避免"大爆炸式"改造。

#### 阶段 0：基线与脚手架（0.5~1 天）

1. 建立 `src/ui/` 目录：
   - `tokens.h` — 度量 token（间距 4/8/12/16/24/32、圆角取自 `DStyle::PM_FrameRadius`、图标尺寸 16/20/24/32），**不含颜色**。
   - `components/` — `PanshiSection`（分组容器）、`PanshiRow`（标签+描述+控件行）、`PanshiStateCard`（状态卡）、`PanshiBadge`（标签徽章）、`PanshiInlineBanner`（内联提示条）、`PanshiLoadingState`（三档加载）、`PanshiEmptyState`（空状态）、`PanshiConfirmDialog`、`PanshiListDelegate`（继承 `DStyledItemDelegate`）。
2. 截屏回归基线：为 7 个页面在 **亮/暗 × 1060×700 / 920×620** 下截图，存档为 before。现有 `docs/*.png` 可作为补充。
3. 建立检查脚本：`grep` 统计 `src/` 下 hex 色值、`gotoPage(` 数字、`setStyleSheet(` 的出现次数，作为阶段验收指标。

#### 阶段 1：打地基（2~3 天）★ 优先级最高

不改变任何布局，只做"去自研"，且**必须一次做干净**（QSS 与 DPalette 混用会产生难以定位的中间态）：

1. **修导航跳转缺陷**（0.3 表格中 4 处 + `m_pages.at(n)`）。这一项独立于视觉改造，可最先提交。
2. 删除 `style.h` 的 hex 色板；把 QSS 中仍承担视觉职责的部分逐个替换：
   - `#panshiCard` → `DFrame` + `DPalette::Base` 背景 + 圆角
   - `#panshiStatValue[stat=...]` → 由代码直接 `setPalette`（Highlight / TextWarning / TextTips）
   - `#panshiTag*` → `PanshiBadge` 组件（背景 = 对应语义色 15% alpha）
   - `#panshiTipBar` / `#panshiWarnBox` → `PanshiInlineBanner`（统一实现）
   - `QProgressBar#panshiUsageBar` → `DProgressBar`
   - `QTableWidget` / `QHeaderView` → `PanshiListDelegate`（或至少先移除 QSS，交给 DTK）
   - 色点 `legendDot` 的内联 `setStyleSheet` → 自绘小 widget + palette
3. 删除 `common.h` 中 15 个内嵌 SVG 与 `IconBadge`，全量替换为 `DIconTheme::findQIcon()`；徽章尺寸从 46/48/40 降到 24/32。
4. 品牌标识：侧边栏/关于页的渐变方块 → `panshi.svg`（`DIconTheme::findQIcon("panshi")`）。
5. 合并 5 份 `confirmDialog/infoDialog/errorDialog` 到 `PanshiConfirmDialog` + `PanshiInlineBanner`。
6. 把 `DiskUsagePage::onThemeChanged` 从 `themeTypeChanged` 扩展到同时监听 `applicationPaletteChanged`；`UsageChart` / `ScanOverlay` / `RadarWidget` / `CheckOverlay` / `CountdownRing` 内部颜色全部改为从 palette 取。

**阶段 1 验收**：hex 计数 = 0；`setStyleSheet` 只剩 ≤2 处（且不涉及颜色）；亮/暗 + 3 种主题色下无残留不一致。

#### 阶段 2：外壳与导航（2 天）

1. `MainWindow::initSidebar()` 重写：`QFrame` + 7 个 `DPushButton` → `DListView` + `PanshiListDelegate`（分组标题、图标、hover/选中/焦点态）。
2. 侧边栏去掉重复标题与 OSTree 脚注。
3. 引入 `PageId` 枚举与 `gotoPage(PageId, SubPageId?)`。
4. 抽出统一的页头渲染（仅框架渲染一次），各页删除自己的 `pageHeader` 调用。
5. 标题栏接入 `DSearchEdit` + 搜索索引框架。

**阶段 2 验收**：侧边栏键盘可达；页头不再重复；搜索可跳到设置项。

#### 阶段 3：逐页重构（每页 0.5~1.5 天，独立提交）

推荐顺序（由简到难，先建立范式再攻坚）：

| 顺序 | 页面 | 主要工作 | 依赖 |
|---|---|---|---|
| 1 | 关于 | 图标、折叠区、复制按钮 | 阶段 1 |
| 2 | 系统更新 | 阶段进度、结果折叠、内联反馈 | 阶段 1 |
| 3 | 策略与设置 | 两类保存模型视觉分离、行式设置项、脏状态按钮 | 阶段 1 |
| 4 | 系统保护 | hero、唯一开关、危险操作折叠 | 阶段 1 + 4.2 模型 |
| 5 | 总览 | hero、响应式卡组、存储分布条、快速操作跳转 | 阶段 1 + 4.2 模型 |
| 6 | 备份与版本 | 删内层页头、DListView、行尾菜单、空状态、还原流程精简 | 阶段 2（页头） |
| 7 | 磁盘空间 | tab/空状态、监控设置内联化、图表配色与坐标轴、面包屑 | 阶段 4 全部 |

#### 阶段 4：交互与流程（2~3 天，可与阶段 3 交错）

1. 实现 `PanshiLoadingState` 三档，替换 `BasePage` 遮罩；改造总览页两步加载。
2. 实现 `PanshiProtectionModel` 单一数据源，移除 `refreshPanshiPages()` 的整页刷新。
3. 统一反馈：成功/错误改内联条；错误文案可行动化 + 详情折叠。
4. 范围选择改分段控件；自定义范围改单日历；图表配色与坐标轴重做。
5. 空状态组件接入 4 处（排行、详情、备份、部署）。
6. 键盘可达与无障碍全量走查（4.7）。
7. `closeEvent` 任务保护。

#### 阶段 5：打磨与验收（1~2 天）

1. 三态截屏对比：亮/暗 × 主题色 × 3 档窗口尺寸（920×620 / 1366×768 / 1920×1080）。
2. 执行 2.3 的验收清单，逐项打勾。
3. 更新 `README.md` 与 `docs/开发文档.md`：页面/导航顺序、目录结构（新增 `src/ui/`）、截屏。
4. 翻译：`lupdate` 重新生成 `.ts`（本次会引入大量新文案与删除旧文案）。
5. 在 1366×768 真机（X11 + Wayland 各一轮）验证：主题切换、图标查找、滚动条、对话框尺寸。

### 5.2 注意事项

#### 关于 QSS 与 DTK 的边界

1. **QSS 优先级高于 DPalette**。只要 QSS 里还留着一条颜色规则，该控件就不会跟主题。因此阶段 1 的 QSS 清理**必须一次性完成**，不能"分批替换"。
2. 确实需要 QSS 的场景只剩两类，且都不涉及颜色：布局微调（padding/margin）、DTK 未覆盖的控件尺寸约束。若发现自己在写 `background`/`color`/`border`，立即停下来找对应的 DPalette 角色。
3. 自绘控件（`UsageChart`/`ScanOverlay`/`RadarWidget`/`CheckOverlay`/`CountdownRing`）不能用 QSS，必须在 `paintEvent` 里从 `palette()` 或 `DGuiApplicationHelper::instance()->applicationPalette()` 取色，并监听 `applicationPaletteChanged`。

#### 关于 DTK 控件替换

4. `DStyledItemDelegate` 的 `setBackgroundType(RoundedBackground)` 才能得到 deepin 的圆角选中效果；行尾操作按钮用 `item->setActionList(Qt::RightEdge, {actions})`，不要再用 `setCellWidget` 塞 `DPushButton`。
5. `DFrame` 默认会画边框，需要 `setLineWidth(0)`（现在的 `Ui::card()` 已做），但改造后应改用"无边框 + Base 底色"，不要依赖 `FrameBorder` 描边来表达分组。
6. 从 `QTableWidget` 迁到 `DListView` 需要引入 `QAbstractTableModel`；这是阶段 3 中备份/部署/排行三处的主要工作量，建议先抽出通用的 `PanshiTableModel`。
7. `QCalendarWidget` / `QGroupBox` / `QProgressBar` / `QScrollBar` 这几个 Qt 原生控件是"风格漏点"，必须全部替换，否则主题一致性验收通不过。

#### 关于行为与数据

8. **列表迁移必须保行为**：备份表的"初始备份不可删除"、部署表的"当前启动项不可删除"、排行表的"单击选中 / 双击下钻"、详情表的"点击目录行切换时间线"——这些逻辑在换控件时最容易丢，需逐条写进改造 checklist。
9. **DConfig 键不能动**：`monitor.*` 16 个键（见开发文档 §7.1）是持久化契约。UI 重排后要跑一遍键表回归，确认读写仍对应。
10. **跨页跳转带动作**（4.5 第 3 点）会改变时序：例如"跳到备份页并打开创建对话框"要求目标页 `refresh()` 完成后才能弹窗。需在 `gotoPage` 中引入"到达后动作"队列，而不是在 `connect` 里直接调用。
11. **还原流程简化不能降低安全性**：永久关闭保护的 30 秒倒计时、还原的二次确认必须保留，只能收敛到统一组件里，不能删除。
12. **`--show-disk` 的 D-Bus 转发路径**（`main.cpp`）依赖 `MainWindow::showDiskDate`，重构跳转时不要破坏这条链路；改造后需重跑开发文档 §10.1 的 4 条回归命令。

#### 关于工程与质量

13. **性能**：`IconBadge` 的静态实例表（`QList<IconBadge*> s_instances`）必须随该类一起删除，否则会遗留失效指针风险与主题刷新开销。新组件不要引入每帧绘制。
14. **动效合规**：新增动效全部读取 `enable-animations`，"减少动态效果"开启时归零。
15. **翻译**：所有新文案必须 `tr()`；本次改造会删除大量旧字符串，`.ts` 需要清理陈旧条目。
16. **构建**：保持 0 警告（现状 README 声明 0 警告 0 错误，不要破坏）。
17. **不要顺带改功能**：本次只做视觉与交互重构。例如"跨分区统计口径"（开发文档 §11 已知限制）不在范围内，不要顺手改数据逻辑，否则回归面失控。
18. **`data/icons/panshi.svg` 需要单独评估**：如果它本身也是 Web 风格的彩色图标，建议一并按 deepin 图标规范（简洁线性、单色 + 主题色，支持 symbolic 变体）重绘 —— 这是本次唯一需要动设计资产的项。

### 5.3 风险与回退

| 风险 | 应对 |
|---|---|
| QSS 清理过程中出现"一半跟主题一半不跟"的中间态 | 阶段 1 单独立分支、一次性清理完成后再合并；期间不做其它 UI 改动 |
| `DListView` 迁移导致表格行为丢失 | 每张表迁移前先写行为清单（含禁用条件、双击/单击语义），迁移后逐条验证 |
| 侧边栏换 `DListView` 影响启动路径 | 保留 `gotoPage(PageId)` 的对外接口不变，只改内部实现 |
| 截屏对比发现主题色未跟随 | 优先检查是否仍有 QSS 颜色残留 + 是否监听了 `applicationPaletteChanged` |
| 工作量超预期 | 阶段 3 逐页独立提交，任意页面可单独回退；阶段 4 可按需延后（先保阶段 1~3） |

---

## 附录 A：视觉改造前后对照速查

| 元素 | 现在 | 重构后 |
|---|---|---|
| 页面底色 | QSS `#F7F8FA` / `#1E1E1E` | `DPalette::Window` |
| 卡片底色 | QSS `#FFFFFF` / `#2A2A2A` | `DPalette::Base` |
| 卡片描边 | `1px solid #E4E7ED` | 无描边，靠底色差 + 8px 圆角 |
| 卡片圆角 | `10px`（写死） | `DStyle::PM_FrameRadius` |
| 强调色 | `#0081FF` | `DPalette::Highlight` |
| 危险色 | `#E64545` | `DPalette::TextWarning` |
| 成功色 | `#00A870` | `Highlight` / `TextLively`（不再自定义绿） |
| 装饰紫 | `#7B61FF` | 删除 |
| 图标 | 15 个手写 SVG + IconBadge | `DIconTheme::findQIcon()` |
| 应用标识 | 渐变方块 + "磐" | `data/icons/panshi.svg` |
| 导航 | 7 个 DPushButton | 分组 `DListView` + delegate |
| 列表 | QTableWidget + QSS | `DListView` + `DStyledItemDelegate` |
| 进度条 | QProgressBar + QSS | `DProgressBar` |
| 加载 | 全页遮罩 + DSpinner | 骨架屏 / 局部 spinner / 顶部细进度（三档） |
| 确认框 | 5 份重复实现 | 1 个 `PanshiConfirmDialog` |
| 成功反馈 | 模态弹窗 | 内联提示条 |
| 跳转 | `gotoPage(0..6)` 裸下标（4 处错位） | `gotoPage(PageId, SubPageId?)` |
| 键盘 | 导航 `NoFocus` + outline:none | 全流程可达、焦点框可见 |
| 搜索 | 无 | 标题栏 DSearchEdit + 设置项索引 |

## 附录 B：改造检查清单（每页完成后逐项打勾）

- [ ] 页面无 hex 颜色；所有颜色可从 DPalette 角色追溯
- [ ] 图标全部来自 `DIconTheme::findQIcon()`，且非空
- [ ] 字体全部经 `DFontSizeManager::bind()`，无 QSS `font-size/font-weight` 残留
- [ ] 圆角取自 `DStyle::PM_FrameRadius` 或 token
- [ ] 无 Qt 原生控件残留（`QTableWidget` / `QListWidget` / `QProgressBar` / `QCalendarWidget` / `QGroupBox`）
- [ ] 页头只渲染一次（子页不再渲染）
- [ ] 响应式：920×620 / 1366×768 下无横向滚动、无文本截断
- [ ] 加载态符合三档定义，未使用全页遮罩
- [ ] 空状态有图形 + 明确 CTA
- [ ] 破坏性操作使用统一确认组件，按钮为动词，默认焦点在取消
- [ ] 成功反馈为内联条，非模态弹窗
- [ ] 错误文案可行动，原始输出折叠在"详情"
- [ ] 键盘可遍历，焦点框可见（非自绘 outline）
- [ ] 所有图标按钮有 tooltip，关键控件有 accessibleName
- [ ] 亮/暗 × 主题色切换后该页无残留不一致
- [ ] 动效遵循 `enable-animations`
- [ ] 新文案全部 `tr()` 包裹
- [ ] 构建 0 警告 0 错误
