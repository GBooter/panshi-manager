# UI 重构进度 — 阶段 5：打磨与验收

> 阶段 5 任务来源：`docs/UI-重构设计方案.md` §5.1。
> 本文件是**验收走查记录 + 待办清单**，不是完成证明 —— 标注「需实机」的项必须由用户在
> 真实 deepin 25 桌面（X11 + Wayland）上执行。

---

## 一、阶段 5 的五项任务与当前状态

| # | 任务（按设计方案 §5.1） | 状态 |
|---|---|---|
| 1 | 三态截屏对比（亮/暗 × 主题色 × 3 档窗口尺寸） | ⏳ 需实机（截屏与视觉判定） |
| 2 | 执行 §2.3 验收清单，逐项打勾 | ✅ **本轮代码侧走查完成**（见 §二），含「需实机」标注 |
| 3 | 更新 `README.md` 与 `docs/开发文档.md` | ✅ **本轮完成**（见 §三） |
| 4 | `lupdate` 重新生成 `.ts`（新增大量文案、删除旧文案） | ⏳ 需实机（`lupdate` 无法在助手侧运行），已给出清单（见 §四） |
| 5 | 1366×768 真机 X11 + Wayland 验证 | ⏳ 需实机，已给出回归清单（见 §五） |
| + | **跨页跳转携带动作**（用户追加决策，非 §5.1 原列） | ✅ **本轮完成**（见 `docs/UI-重构进度-阶段5-跨页跳转动作.md`），待编译复测 |
| + | **磁盘页 Qt 原生控件收尾（阶段 3-7b）**（用户追加决策，非 §5.1 原列） | ✅ **代码完成**（见 `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md`），待编译 + 实机复测；勾销 §六 O1/O2/O4/O5/O6 |

---

## 二、§2.3 验收清单逐项走查（代码侧）

### 2.1 配色：hex 色值命中数

| 检查项 | 结论 | 依据 |
|---|---|---|
| `src/style.h` 无 hex 常量 | ✅ 达成 | `style.h` 全部颜色经 `panshiColor(DPalette::ColorType/QPalette::ColorRole)` 运行时取值；17 个 hex 语义色常量已删除 |
| `src/pages/common.h` 无 hex | ✅ 达成 | 全部走 `DPalette::TextTitle/TextTips` + `DFontSizeManager`；图例色点 `LegendDot` 自绘取 `PanshiUi::semanticColor()` |
| `src/ui/components.h/.cpp` 无 hex | ✅ 达成 | `semanticColor()` / `softFill()` 统一从 DPalette 派生 |
| `src/pages/*.cpp` 无 hex | ✅ 达成 | 阶段 3 各页重写时逐页清理；3-7 清了最后一批（`panshiTagXxx`/`panshiWarnBox`/`panshiTipBar`/`panshiStatValue[stat]`） |
| `src/widgets/*.cpp` 无 hex | ✅ 达成 | `usagechart` / `scanoverlay` / `radarwidget` / `checkoverlay` / `countdowndialog` 全部改为 `paintEvent` 内取 `palette()` / `applicationPalette()` |
| `src/disk/*` 无 hex | ✅ 不适用 | 纯数据/命令层，无绘制代码 |
| `data/icons/panshi.svg` | ⚠️ **保留**（设计方案明确除外） | §5.1 第 18 条单列：应用图标属设计资产，需单独评估是否按 deepin 图标规范重绘 |

> **助手侧限制**：`Bash` / `Grep` 持续 `exit 251` 失效，无法执行
> `grep -rE '#[0-9A-Fa-f]{6}' src/` 做计数断言。上表结论来自**逐文件通读**
> （阶段 1～4 每轮都按此方式核对，且阶段 1 验收时计数为 0）。
> **建议实机复跑一次计数命令**确认无回归（见 §五 回归命令 R1）。

### 2.2 主题：Qt 原生控件清除情况

| 控件 | 位置 | 状态 |
|---|---|---|
| `QCalendarWidget` | `diskusagepage.cpp` 自定义范围对话框 | ✅ **已收口**（阶段 3-7b：两个纵排 → 单日历两次点击定区间） |
| `QGroupBox` | — | ✅ 已清除（阶段 3-6 备份页还原流程重写时删除） |
| `QProgressBar` | `diskusagepage.cpp` `m_usageBar`（概览容量条） | ✅ **已收口**（阶段 3-7b → `DProgressBar`） |
| `QTableWidget` | `diskusagepage.cpp` ×2（增长详情 / 占用排行） | ⚠️ **有意保留**（设计方案 §5.2 第 6 条允许；DTK 无等价表格） |
| `QListWidget` | `diskusagepage.cpp` `m_excludeList`（排除目录） | ✅ **已收口**（阶段 3-7b → `DListView` + `QStringListModel`） |
| `QPushButton` | `diskusagepage.cpp` `m_rankBackBtn` / `m_serviceStartBtn` / `m_backTimelineBtn` | ✅ **已收口**（阶段 3-7b → `DPushButton`，原声明与 `Ui::button()` 返回类型不一致） |
| `QDoubleSpinBox` | `diskusagepage.cpp` `m_multiplierSpin` | ✅ **已收口**（阶段 3-7b → `DDoubleSpinBox`） |
| `QScrollBar` | 交给 DTK（无自定义） | ✅ 达成 |

> **结论（阶段 3-7b 后）**：磁盘空间页的 Qt 原生控件已清理完毕，
> **仅剩 `QTableWidget` ×2 为有意保留**（设计方案明确允许）。
> ⚠️ **全项目口径**：另有快照页 1 个、部署页 1 个 `QTableWidget` 同样是有意保留
> （两页在阶段 3-6 已全量重写，只是表格控件按同一条理由保留）——
> **全项目 `QTableWidget` 实为 ×4**，此前本文档只统计磁盘页，已于 2026-10-03 更正。
> 连带 `style.h` 删除了 `#panshiUsageBar` 两条死规则与其依赖的 11 个局部变量
> —— `panshiStyleSheet()` 此后只保留 5 个结构性颜色，**不再有任何语义色插值**。
> 详见 `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md`。

### 2.3 导航正确性

| 检查项 | 结论 | 依据 |
|---|---|---|
| `gotoPage(数字)` 命中数 = 0 | ✅ 达成 | 阶段 1 引入 `enum class PageId`，全部调用点改强类型；`mainwindow.h` 公开接口只有 `gotoPage(PageId)` 与 `gotoPage(PageId, SubPageId)` |
| `m_pages.at(n)` 命中数 = 0 | ✅ 达成 | 改为 `QMap<PageId, BasePage*>` + `page(PageId)` / `toIndex(PageId)` |
| §0.3 的 4 处错位已修 | ✅ 达成 | 阶段 1 第 1 项独立提交 |
| 跨页跳转携带动作 | ⚠️ 部分 | 「创建备份」直达/「检查更新」自动触发 见 §六 遗留项 O7 |

### 2.4 加载态

| 检查项 | 结论 |
|---|---|
| 首屏以外刷新不出现全页遮罩 | ✅ 达成（三档 L1/L2/L3 已落地；各页 `setMaskLoadingEnabled(false)`） |
| 连续两次加载不闪两次 | ✅ 达成（3-5 总览页 `m_firstLoad` 分流 + L2 局部加载） |
| `masterswitch` 整页遮罩已删 | ✅ 达成（阶段 4 第一批） |

### 2.5 可读性

| 检查项 | 结论 |
|---|---|
| 920×620 无横向滚动 / 无截断 / 主操作可见 | ✅ 达成（3-8 实测通过；卡片行响应式 + `ElidedLabel` + 表格自兜滚动条） |
| 1366×768 | ⏳ 需实机（见 §五） |
| 「显式最小宽度」四处防线就位 | ✅ 达成（表格列 320 / `PanshiRow` 控件区 200 / 数值控件 140 / 状态卡 168） |

### 2.6 键盘可达

| 检查项 | 结论 | 依据 |
|---|---|---|
| 侧栏导航 Tab 可进入 | ✅ 达成 | 阶段 2：`setFocusPolicy(Qt::StrongFocus)`（原 `NoFocus` 已删） |
| 焦点框可见（非自绘 outline） | ✅ 达成 | delegate 用 `QStyle::PE_FrameFocusRect` 绘制；`#panshiNavItem:focus{outline:none}` 已随阶段 1 QSS 清理删除 |
| 侧栏 ↑↓/Home/End 导航 | ✅ 达成（`QListView` 原生） |
| 搜索框键盘导航 | ✅ 已实现（阶段 4 设置搜索索引，待编译复测） |
| 图标按钮 tooltip | ⚠️ **部分** —— 见 §六 遗留项 O8 |
| 关键控件 `accessibleName` | ✅ **全覆盖** —— 侧栏 + `PanshiRow` 输入控件 + **4 张表（表级名 + 单元格级「列标题: 值」）**，见 §六 遗留项 O9 |

### 2.7 危险操作

| 检查项 | 结论 |
|---|---|
| 全应用只有 1 个确认组件 | ✅ 达成（`PanshiUi::ConfirmRequest` + `askConfirm`/`askDanger`；`Ui::confirmDialog` 只是薄封装转发，不含独立 DDialog 构造逻辑） |
| 按钮文案为动作动词 | ⚠️ 部分（`Ui::confirmDialog` 默认值仍是「确定」，各调用点需确认已传 `okText`）见 §六 遗留项 O10 |
| 破坏性操作默认焦点在「取消」 | ✅ 达成（`confirmdialog.cpp` 实现） |
| 30s 倒计时保留 | ✅ 达成（`PanshiCountdownDialog`，完全关闭保护路径） |

### 2.8 反馈

| 检查项 | 结论 |
|---|---|
| 成功类操作 0 次模态弹窗（除「立即重启」） | ✅ 达成（`infoDialog`/`errorDialog` 已删；改 `BasePage::showFeedback` → `PanshiInlineBanner`） |
| 错误文案可行动 + 详情折叠 | ⚠️ 部分 —— 见 §六 遗留项 O11 |
| 原始 stderr 不直接暴露 | ⚠️ 部分（当前是「设置失败：%1」+ 原文；原文本身对运维可诊断，用户已认可） |

### 2.9 无障碍与文案

| 检查项 | 结论 |
|---|---|
| 新文案全部 `tr()` | ✅ 达成（逐页核对；组件层用 `QObject::tr()`） |
| 动效遵循 `enable-animations` | ✅ 达成（`PanshiUi::reduceMotionRequested()` 读 DConfig；阶段 4 的 `flashHighlight` 亦遵守） |
| 构建 0 警告 0 错误 | ✅ 达成（阶段 4 第一批复测：代修 2 处后 0/0；搜索索引待复测） |

---

## 三、文档更新（本轮完成）

### 3.1 `README.md`

**原状问题**（严重过时，描述的是重构前的实现）：
- 「图标」行写「**自绘 SVG 线性图标徽章**（`IconBadge` 类）… 尺寸加大（状态卡 46px / 页面标题 48px / 卡片 40px）」—— 该方案已被推翻
- 页面名仍是旧称：磁盘占用 / 备份管理 / 备份策略
- 目录结构缺 `src/ui/`，且仍写 `style.h # 亮暗主题自适应 QSS`
- 未提三档加载、注册点范式、设置搜索

**本轮改为**：见文件本体 —— 图标改为 `DIconTheme` 主题图标、页面名对齐 `PageId`、
目录结构补 `src/ui/`、新增「UI 设计体系」章节。

### 3.2 `docs/开发文档.md`

**需同步**：
- §1.1 功能一览：页面名旧称
- §3 目录结构：缺 `src/ui/`
- §4.1 分层图：「QSS」表述过时
- §4.4 主题体系：整节描述的是**已废弃的 QSS 主题方案**（「`panshiStyleSheet()` 生成全局 QSS…
  在 `main.cpp` 监听 `themeTypeChanged` 时整体刷新」「`Ui::IconBadge` 自绘线性 SVG 徽章，
  `refreshAll()` 随主题刷新颜色」）—— 需重写为本项目的 DPalette 方案
- §7.1 DConfig 16 键表：**不改**（持久化契约，UI 重排不影响）

---

## 四、`.ts` 翻译清单（需实机执行）

`lupdate` 需在具备 Qt6 工具链的环境执行，助手侧无法运行。**执行步骤**：

```bash
# 1) 若尚无 .ts 文件，先创建（按 CMakeLists 的组织方式二选一）
lupdate -recursive src/ -ts translations/panshi-manager_zh_CN.ts

# 2) 已有文件则更新
lupdate -recursive src/ -ts translations/*.ts

# 3) 编译为 .qm
lrelease translations/*.ts
```

**本轮改造引入的文案变化特征**（供 `.ts` 审核时聚焦）：

| 类别 | 说明 |
|---|---|
| **新增** | 设置搜索：「搜索设置」「未找到匹配的设置项」；7 条设置项标题/说明/分组名；`PanshiRow::flashHighlight` 无文案；阶段 4 决策框：3 条 `consequences` + 2 条 `options` + 各页动词化确认按钮 |
| **新增** | 阶段 3 各页组件化文案：`PanshiStateCard` 标签、`PanshiEmptyState` CTA、`PanshiCollapsible` 标题 |
| **删除** | 原 5 份 `confirmDialog`/`infoDialog`/`errorDialog` 的重复文案（「确定」「设置成功! 请重启系统使设置生效。」等） |
| **删除** | 原 QSS 时代的 objectName 相关无文案；原「磁盘占用」「备份管理」「备份策略」页名 |
| **改写** | 页名：磁盘占用→磁盘空间、备份管理→备份与版本、备份策略→策略与设置 |
| **注意** | `lupdate` 会把「删除」的旧条目变成**標記为 obsolete**（带 `type="obsolete"`），不会自动物理删除 —— 需 `lupdate -no-obsolete` 或在 `.ts` 中人工清理，否则陈旧条目持续累积 |

**风险提示**：本项目**此前没有 `translations/` 目录**（`CMakeLists.txt` 未引用 `.ts`/`.qm`，
应用依赖 Qt 默认行为）。正式引入翻译需同步在 `CMakeLists.txt` 中增加
`qt_add_translations()`，否则生成的 `.qm` 不会被安装。

> ✅ **已落地**（2026-10-01）：i18n 机制已接入 —— `CMakeLists.txt` 加
> `qt_add_translations(panshi-manager TS_FILES …)`，新建 `translations/`
> （`panshi-manager_zh_CN.ts` 恒等 + `panshi-manager_en_US.ts` 译文），
> 并修正 `main.cpp` 的 `tr()` 时序与 `aboutpage.cpp` 的一处裸中文。
> 本节的 lupdate 步骤现由 `qt_add_translations` 的 `update_translations` 目标
> 承接（也可继续手跑 `lupdate`）。完整说明见
> `docs/UI-重构进度-阶段-i18n接入.md` 与 `docs/开发文档.md` §9。
> 注意：`lupdate -no-obsolete` 已写进 CMake 的 `LUPDATE_OPTIONS`，避免陈旧条目累积。

---

## 五、实机回归清单（需你执行）

### 5.1 验收计数命令

```bash
cd /path/to/磐石设置

# R1 — hex 色值计数（要求：src/ 下命中 = 0，data/icons/panshi.svg 除外）
grep -rE '#[0-9A-Fa-f]{6}' src/ | wc -l

# R2 — 裸下标跳转（要求：0）
grep -rnE 'gotoPage\([0-9]' src/ | wc -l
grep -rn 'm_pages\.at(' src/ | wc -l

# R3 — QSS 残留（要求：≤2 处，且不涉及颜色）
grep -rn 'setStyleSheet' src/ | wc -l

# R4 — 阻塞式模态弹窗（要求：仅 PanshiCountdownDialog 与决策框）
grep -rn 'exec()' src/ | grep -v countdowndialog

# R5 — Qt 原生控件清单（期望：只剩 QTableWidget，见 §六 O3）
grep -rnE '\bQ(TableWidget|ListWidget|ProgressBar|CalendarWidget|GroupBox|DoubleSpinBox|PushButton)\b' src/ | grep -v '^src/.*//'
# 预期命中：仅 QTableWidget（磁盘页 2 + 快照页 1 + 部署页 1 = 4 处有意的保留）
# ⚠️ 命中里会混进**注释文本**（如 diskusagepage 记录替换历史的注释），需人工确认不是真控件
# 若命中 ListWidget / ProgressBar / DoubleSpinBox / PushButton / CalendarWidget → 说明收尾回归了
```

### 5.1b 跨页跳转携带动作回归（新，阶段 5 附）

见 `docs/UI-重构进度-阶段5-跨页跳转动作.md` §五 的 C1~C8 八项。其中：

- **C1/C2/C3** 直接点总览三个快速操作即可；
- **C4** 需先开启可写层（保护页「永久关闭保护」）+ 重启，再去备份页点「还原」；
- **C5** 建议在磁盘空间页观察（该页取数最慢）；
- **C7** 需构造 sudo 阻塞场景（如把 `SUDO_ASKPASS` 指向一个等待脚本）。

### 5.2 `--show-disk` D-Bus 转发回归（设计方案 §5.2 第 12 条）

按 `docs/开发文档.md` §10.1 的 4 条命令逐条执行，确认重构未破坏这条链路。

### 5.3 DConfig 16 键表回归（设计方案 §5.2 第 9 条）

按开发文档 §7.1 键表，在磁盘监控设置页逐项改一次，确认读写仍对应（UI 重排不得动键名）。

### 5.4 三态截屏对比

亮/暗 × ≥3 种主题色 × 3 档窗口（920×620 / 1366×768 / 1920×1080）。
重点看：卡片底色差、强调色跟随、`ElidedLabel` 省略、卡片行折列。

### 5.5 真机 X11 + Wayland 各一轮（§5.1 第 5 条）

主题切换、`DIconTheme` 图标查找（确认非空）、滚动条、自定义范围对话框尺寸。

> 阶段 3-7b 后，自定义范围对话框已从「两个纵排日历」改为「单日历两次点击」，
> 尺寸明显变矮（宽 520 → 420）—— 请重点确认在 X11 与 Wayland 下**弹出位置与
> 高度**都正常，且两次点击定区间的交互无歧义。清单见
> `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md` §5.6。

---

## 六、阶段 5 遗留项清单

| # | 项 | 位置 | 优先级 | 说明 |
|---|---|---|---|---|
| **O1** | ~~`QCalendarWidget` ×2（纵排）~~ | `diskusagepage.cpp` 自定义范围对话框 | — | ✅ **已完成**（阶段 3-7b）：改为**单日历 + 两次点击定区间**，摘要以 `from`/`to` 为唯一数据源，消除「日历高亮 vs 摘要不一致」的状态分叉。见 `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md` §3.5 |
| **O2** | ~~`QProgressBar` `m_usageBar`~~ | `diskusagepage.cpp` 概览容量条 | — | ✅ **已完成**（阶段 3-7b）：→ `DProgressBar`（继承 `QProgressBar`，`setFormat`/`setTextVisible` 等全部行为等价）；`style.h` 的 `#panshiUsageBar` 两条规则 + 11 个失效变量一并删除 |
| **O3** | `QTableWidget` ×4 | `diskusagepage.cpp` 增长详情 / 占用排行；`snapshotspage.cpp` 快照列表；`deployspage.cpp` 部署列表 | **低（确认保留）** | 设计方案 §5.2 第 6 条：迁 `DListView` 需引入 `QAbstractTableModel`，是三处列表的主要工作量；§3.6 已评估「列对齐要手写 delegate 重实现，风险大于收益」，**用户已验收当前表格手感**，故保留。⚠️ 2026-10-03 更正：原表述为「×2（仅磁盘页）」，**漏计快照页与部署页各 1 个** |
| **O4** | ~~`QListWidget` `m_excludeList`~~ | `diskusagepage.cpp` 排除目录 | — | ✅ **已完成**（阶段 3-7b）：→ `DListView` + `QStringListModel`；取当前行改 `currentIndex().row()`；顺带修掉「配置已删而视图照删」的越界分叉 |
| **O5** | ~~`QPushButton` ×3~~ | `diskusagepage.cpp` 返回上级 / 立即启动 / 返回系统盘 | — | ✅ **已完成**（阶段 3-7b）：→ `DPushButton`（实为声明纠正 —— `Ui::button()` 本就返回 `DPushButton*`，原声明靠继承关系才编过） |
| **O6** | ~~`QDoubleSpinBox` `m_multiplierSpin`~~ | `diskusagepage.cpp` 告警倍数 | — | ✅ **已完成**（阶段 3-7b）：→ `DDoubleSpinBox`（继承 `QDoubleSpinBox`，`setDecimals`/`setRange(double)`/`valueChanged(double)` 全部等价） |
| **O7** | ~~跨页跳转未携带动作~~ | `mainwindow.cpp` `gotoPage` | — | ✅ **已完成并复测通过**（阶段 5 附）。总览「立即创建备份」直达并开框、「检查更新」自动触发、「保护设置」定位高亮；备份页可写层冲突跳保护页并高亮「开启保护」行。实现见 `docs/UI-重构进度-阶段5-跨页跳转动作.md`，架构说明见 `docs/开发文档.md` §3.2。**C4/C7 环境不可达待补** |
| **O8** | ~~图标按钮 tooltip 未全量~~ | 各页 | — | ✅ **已消解**（2026-10-03 核实）。全项目无 `QToolButton`、无 `Ui::iconButton()`；图标按钮实际只有 2 处 `DIconButton` 工厂（`diskusagepage.cpp` 打开位置、`aboutpage.cpp` 复制命令），**均已带 `setToolTip`**。侧栏导航项与搜索结果走 `QStandardItem::setToolTip(text)`，Banner 关闭按钮在 `components.cpp`。原记录已随页面集合变化而失效 |
| **O9** | ~~`accessibleName` 未设置~~ | 侧栏（搜索框 / 两个列表）+ 全部 4 张表 | — | ✅ **已结项**（2026-10-04）。① 侧栏搜索框 `DLineEdit` + 其内 `QLineEdit`（名称 + 描述）、导航列表「主导航」、搜索结果列表「设置搜索结果」（2026-10-03）；② `PanshiRow` 输入控件：`addControl()` / `setTitle()` 把行标题回填为控件 `accessibleName`（仅回填未显式命名的）；③ **表格单元格级（本轮）** —— ⚠️ **此前的判断「DTK 未提供单元格接口、需自绘 delegate」是错的**：Qt 原生 `QTableWidgetItem::setData(Qt::AccessibleTextRole, …)` 即可，读屏软件直接生效，**不需要 delegate**。已在 `components.cpp` 收敛为两个 helper：`setTableAccessibleName()`（表级名）+ `finalizeTableAccessible()`（遍历所有已填 item 写「列标题: 值」，空 item 写中性占位「列标题: -」），4 张表全覆盖（磁盘增长详情/占用排行、系统备份、系统部署）。⚠️ 两个易漏点已就地处理：**cellWidget 列（路径/名称/操作按钮）遍历不到 item 角色**，必须在各自工厂里单独 `setAccessibleName`，且纯图标按钮与操作按钮都要**带上行标识**（路径/版本号/快照名），否则读屏逐格移动时听不出是哪一行 |
| **O10** | ~~`Ui::confirmDialog` 默认「确定」~~ | `common.h` | — | ✅ **已清零**（2026-10-03）。原先 `protectionpage.cpp:210` 传空 `okText` 依赖默认值 → 危险确认按钮显示「确定」。已改为传动作动词（`action` 变量，本就是「开启只读保护」/「关闭只读保护(开启可写层)」）。其余调用点（`updatepage.cpp` 传「开始升级」、全部 `ConfirmRequest` 路径）本来就显式赋值 |
| **O11** | ~~错误详情未折叠~~ | `masterswitch.cpp` / `protectionpage.cpp` / `snapshotspage.cpp` | — | ✅ **已完成**（2026-10-03）。原状：`createDetailsBox()` 已实现但**零调用**，三处仍 `tr("设置失败：%1").arg(原始输出)` 裸拼接。修法：① `PanshiInlineBanner` 增加 `setDetails(title, rawText)` —— 正文与详情块改为竖排容器，详情用现成 `PanshiCollapsible`，不新造控件；② `BasePage` 增加 `showFeedbackWithDetails()`，内部先清旧详情再复用 `showFeedback`（避免两处逻辑分叉）；③ 三处调用点改为「正文一句可行动说明 + 详情放原始输出」。`updatepage.cpp` 原本就有真折叠，无需改 |
| **O12** | 侧栏折叠（<1000px） | `mainwindow.cpp` | 低 | ❌ **用户决策不实现（2026-10-04）**。设计方案 §3.4 提到紧凑断点可折叠为 56px 图标条；评估后认为在当前主流桌面分辨率下价值有限（3-8 实测 920 宽下 200px 无挤压问题），用户明确放弃，不阻塞验收 |

> **O1/O2/O4/O5/O6 已于阶段 3-7b 勾销** —— 它们原本全部集中在 `diskusagepage.*`
> 一个文件（阶段 3-7 为避免回归面失控而保留），按此前的建议**单独立了一轮**
> 「阶段 3-7b：磁盘页控件收尾」来完成，而非混进阶段 5 验收。
> 见 `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md`。
>
> 磁盘页 Qt 原生控件现状：**仅剩 `QTableWidget` ×2 为有意保留**（O3）。

---

## 七、决策结果（已由用户答复）

| # | 事项 | 决定 | 落地状态 |
|---|---|---|---|
| 1 | 是否引入 i18n 机制（`translations/` + CMake `qt_add_translations`） | **引入** | ✅ **已完成并复测通过**（2026-10-02）。CMake 加 `qt_add_translations` + `LinguistTools` 组件；新建 `translations/`（zh_CN 恒等 + en_US 译文 + README）；修 `main.cpp` 三处 `tr()` 时序与 `aboutpage.cpp` 裸中文；`install.sh` 用户级分支补拷 `.qm`。T1~T7 / I2 / D1 全过（I1 环境不可达；C3/I3 无法构造）。见 `docs/UI-重构进度-阶段-i18n接入.md` |
| 2 | 是否启动「阶段 3-7b：磁盘页控件收尾」（清 O1/O2/O4/O5/O6） | **启动** | ✅ **代码完成**（2026-10-02）。六项中 O2/O4/O5/O6 为低风险（控件替换/声明纠正），O1 为单日历两次点击定区间；O3（`QTableWidget`）确认保留。见 `docs/UI-重构进度-阶段3-7b-磁盘页控件收尾.md`。**待编译 + 实机复测** |
| 3 | O7（跨页跳转携带动作）是否本轮做 | **做** | ✅ **本轮完成并复测通过**（C1/C2/C3/C5/C8 + 窄窗 922 全过，见 `docs/UI-重构进度-阶段5-跨页跳转动作.md` §五） |

> 决策 1 的注意点：项目**此前无 `translations/` 目录、`CMakeLists.txt` 未引用翻译**
> （全局 `set(CMAKE_AUTOMOC ON)`）。正式引入需同步加 `qt_add_translations()`，
> 并确认 `.qm` 安装路径与 `main.cpp` 的 `loadTranslator` 匹配。
> → **已按此落地**，另发现两处真实缺陷并修复（`tr()` 时序倒置、关于页裸中文），
> 原因与修法见 `docs/UI-重构进度-阶段-i18n接入.md` §一。
>
> 决策 2 的注意点：必须**保行为**（不动数据逻辑），逐控件替换 + 每步可回退；
> 与阶段 5 验收分离，避免回归面与验收面交叉。

---

## 八、环境

- 助手侧 **`Bash` / `Grep` 持续 `exit 251` 失效**（本轮已再次确认：`grep`、`python3 -c` 均无输出、无 stderr）。
  §二 的走查结论**全部来自 `Glob` + `Read` 逐文件通读**，非命令断言。
- `QT_LOGGING_RULES` 屏蔽 `qInfo`，调试用 **`qWarning`**。
- 本机后端走 `sudo` 且无免密 → 写操作成功分支无法端到端验证（阶段 4 已定位此根因）。
