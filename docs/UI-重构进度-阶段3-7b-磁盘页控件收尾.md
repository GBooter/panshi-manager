# UI 重构进度 · 阶段 3-7b 磁盘页 Qt 原生控件收尾

> 前置：阶段 3-7（磁盘空间页重写）+ 阶段 5（打磨与验收）。
> 本阶段 = 清理阶段 3-7 收尾时**有意保留**的最后一批 Qt 原生控件。
> 关联文档：`docs/UI-重构进度-阶段3-7-磁盘空间页.md`、`docs/UI-重构进度-阶段5-打磨与验收.md` §2.2 / §六。
> **状态：代码已完成，待编译 + 实机复测。**

---

## 一、背景：为什么还有 Qt 原生控件

阶段 5 验收清单 §2.2「主题：Qt 原生控件清除情况」的结论是：

> **磁盘空间页是唯一仍有 Qt 原生控件的页面**，全部集中在 `diskusagepage.*`
> 一个文件内。其余 6 页已全部清除。

阶段 3-7 重写该页时，优先级是「结构组件化 + 语义收敛」（卡片 / 徽章 / Banner /
状态卡 / 表格列宽），控件层留到本阶段统一收口 —— 因为控件替换牵动**行为等价性**，
需要逐个核对 API 使用面，单独一轮做更稳妥。

---

## 二、六项收尾与处置

| # | 原控件 | 位置 | 处置 | 风险 |
|---|---|---|---|---|
| O1 | `QCalendarWidget` ×2 | 自定义范围对话框（纵排两个） | **改为单日历两次点击定区间** | 中（有交互语义变化） |
| O2 | `QProgressBar` | 概览容量条 `m_usageBar` | → `DProgressBar` | **极低**（继承关系） |
| O3 | `QTableWidget` ×2 | 增长详情 / 占用排行 | **保留**（设计方案 §5.2 第 6 条允许） | — |
| O4 | `QListWidget` | 排除目录 `m_excludeList` | → `DListView` + `QStringListModel` | 低 |
| O5 | `QPushButton` ×3 | 返回系统盘 / 立即启动 / 返回上级 | → `DPushButton`（**声明纠正**） | **极低** |
| O6 | `QDoubleSpinBox` | 基线倍数 `m_multiplierSpin` | → `DDoubleSpinBox` | **极低**（继承关系） |

---

## 三、逐项说明

### 3.1 O5：`QPushButton*` → `DPushButton*`（声明与实现不一致）

**这不是控件替换，是类型声明纠正。**

三个成员原声明为 `QPushButton*`（或 `class QPushButton*`）：

```cpp
class QPushButton *m_backTimelineBtn = nullptr;  // 返回系统盘
class QPushButton *m_serviceStartBtn = nullptr;  // 立即启动修复
QPushButton       *m_rankBackBtn    = nullptr;   // 返回上级
```

但它们全部由 `Ui::button()` 创建，而该函数（`common.h`）的返回类型是
**`DPushButton*`**：

```cpp
inline DPushButton *button(const QString &text, bool suggest = false, bool warning = false);
```

之所以能编过，是因为 `DPushButton` 公有继承 `QPushButton`，**隐式向上转型**合法。
代价是成员静态类型丢失了 DTK 语义 —— 且所有 `connect(btn, &DPushButton::clicked, …)`
都能工作（`DPushButton` 未新增同名信号），所以此前未暴露问题。

**修法**：把三个声明改成 `DPushButton*`，并从 `.h` 移除 `class QPushButton;`
前置声明、从 `.cpp` 移除 `#include <QPushButton>`。

> 注：`DPushButton` 在 `components.h`（经 `#include <DPushButton>`）已完整可见，
> `.h` 里无需再前置声明。

### 3.2 O2：`QProgressBar` → `DProgressBar`

`DProgressBar` 公有继承 `QProgressBar`（DTK 官方文档确认），仅覆写
`sizeHint()` / `minimumSizeHint()`，**不隐藏任何继承 API**。因此本处用到的
`setRange(0,1000)` / `setValue(int)` / `setTextVisible(bool)` / `setFormat(QString)` /
`setFixedWidth/Height` **全部行为等价** —— 这是本次六项里风险最低的一个。

**同步清理**（`style.h`）：

- 删除 `QProgressBar#panshiUsageBar { … }` 与 `::chunk` 两条规则 ——
  `objectName` 已随之移除，留着就是死规则；
- 连带删掉 `%HOVERBG%` 占位符与 `hoverBg` 变量（失去唯一引用者）；
- **进一步发现**：`accent / accentSoft / danger / dangerSoft / success /
  successSoft / warning / warnBox / warnText / purple` 这 10 个语义色变量
  也**只被那两条规则使用**，同样成为未使用局部变量（会触发 `-Wunused-variable`）
  → 一并删除，并把对应 10 个 `.replace()` 调用删掉。

> **收口意义**：`panshiStyleSheet()` 此后只保留 5 个**结构性**颜色
> （`bg / sidebar / card / border / textSub`），**不再有任何语义色插值**
> —— 这才真正落实设计方案的「QSS 只承担布局职责，颜色职责归 DPalette」。

### 3.3 O6：`QDoubleSpinBox` → `DDoubleSpinBox`

`DDoubleSpinBox` 是 DTK 的双精度版本（官方文档：DTK6 控件表列有该类，页面描述
为「类似 DDoubleSpinBox」，并新增 `setAlert()` / `showAlertMessage()` /
`lineEdit()` / `setEnabledEmbedStyle()` 等 DTK 扩展）。它**继承 `QDoubleSpinBox`**，
故 `setRange(double,double)` / `setDecimals(int)` / `setValue(double)` 与
**信号 `valueChanged(double)`** 全部沿用。

信号连接的唯一改动是把重载解析的目标类名换掉：

```cpp
// 原
connect(m_multiplierSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), …);
// 现
connect(m_multiplierSpin, QOverload<double>::of(&DDoubleSpinBox::valueChanged), …);
```

> **为什么必须换**：`valueChanged` 在 `QDoubleSpinBox` 与 `DDoubleSpinBox` 里是
> **同一个继承来的信号**，直接写 `&QDoubleSpinBox::valueChanged` 其实也能编过。
> 但成员已是 `DDoubleSpinBox*`，用 DTK 类名更一致、也不会在将来 DTK 覆写该
> 信号时静默失配。这是本轮唯一的「信号目标类名」改动。

### 3.4 O4：`QListWidget` → `DListView`

`DListView` 继承 `QListView`（官方文档 + DTK6 源码索引确认），提供
`addItem(QVariant)` / `addItems(QVariantList)` / `insertItem` / `removeItem(int)` /
`clear()` 等便利方法。但**它是标准 model/view 控件，不自带模型**
（这一点与 `QListWidget` 的便利模型不同），因此需要显式配一个 `QStringListModel`。

**改动**：

```cpp
// 新增成员（diskusagepage.h）
class QStringListModel *m_excludeModel = nullptr;

// 构建（buildSettingsDialog）
m_excludeList = new DListView;
m_excludeList->setMaximumHeight(110);
m_excludeList->setEditTriggers(QAbstractItemView::NoEditTriggers);
m_excludeModel = new QStringListModel(m_excludeList);   // 模型以列表为 QObject 父
m_excludeList->setModel(m_excludeModel);
```

**三个使用点**：

| 原（`QListWidget`） | 现（`DListView` + 模型） |
|---|---|
| `m_excludeList->clear(); addItems(excludes);` | `m_excludeModel->setStringList(excludes);` |
| `m_excludeList->addItem(p);` | `m_excludeModel->setStringList(ex);` |
| `m_excludeList->currentRow()` | `m_excludeList->currentIndex().row()` |
| `delete m_excludeList->takeItem(row);` | `m_excludeModel->setStringList(ex);` |

**顺带修掉一处状态分叉**：原 `onRemoveExclude` 删配置后调用 `takeItem(row)`，
「模型（配置）已删 + 视图再删一行」是两条独立路径；若 `row >= ex.size()`
（越界）则配置未删而视图照删 → 二者不一致。现统一**以配置为单一数据源整表重建**，
不存在该分叉。

> `currentIndex().row()`：无选中项时 `currentIndex()` 无效，`row()` 返回 `-1`，
> 与原 `currentRow()` 的「无选中返回 -1」语义一致，`if (row < 0) return;` 保留。

### 3.5 O1：`QCalendarWidget` ×2 → 单日历两次点击定区间

这是六项里**唯一有交互语义变化**的一处，需要展开说明。

**原实现的问题**（两个日历纵排）：

1. 两个整月网格纵向堆叠 ≈ 700px 高，对话框被迫很高，与其它设置页观感不一致；
2. 起止相互约束靠「点击回调里比大小，决定要不要刷新摘要」——
   终点早于起点的**无效点击会让摘要不更新，但日历的选中日已经变了**，
   出现「界面高亮 vs 摘要文字」不一致；
3. 结束时还要 `std::swap(from, to)` 兜底 —— 恰好反证上面的不一致确实会发生。

**现实现**（一个日历 + 两次点击）：

```
初值：complete = true（视为「已选定完整区间」）
第 1 次点击 → from = to = d，complete = false（起点落定，等终点）
第 2 次点击 → 若 d < from：from = d（起点改到更早，仍等终点）
              否则：      to = d，complete = true（区间完成）
确定时：若 from > to 则 swap
```

配合一句行内提示：「第 1 次点击选择开始日期，第 2 次点击选择结束日期
（第二次早于首次时会自动改为新的开始日期）」。

**关键改进**：**摘要文字的唯一数据源是 `from`/`to` 状态本身**，不再读日历的
`selectedDate()` —— 从结构上消除「高亮与摘要不一致」的可能。

**其它保留**：`setMaximumDate(QDate::currentDate())`（未来无数据，禁选）、
`setVerticalHeaderFormat(NoVerticalHeader)`（去周数列，省宽度）、
`DDialog` 取消/确定按钮语义（`ButtonNormal` / `ButtonRecommend`）均不变。
对话框宽度从 520 收窄到 420（单月网格所需）。

**实现细节**：区间状态用局部 `struct RangeState` + `std::shared_ptr` 承载，
lambda **按值捕获该 `shared_ptr`**，并把连接挂到 `&dlg`（不是 `this`）——
避免「lambda 按引用捕获函数局部变量、而连接挂在长生命周期对象上」的隐患。

---

## 四、改动文件清单

| 文件 | 改动 |
|---|---|
| `src/pages/diskusagepage.h` | include 增删；6 个成员类型；新增 `m_excludeModel`；类注释补 3-7b 段 |
| `src/pages/diskusagepage.cpp` | include 增删（+`<memory>`、`<QStringListModel>`、DTK 头）；`m_usageBar` 构造；`m_multiplierSpin` 构造 + 信号；`m_excludeList` 构造 + 3 处用法；`onOpenCustomRange` 重写 |
| `src/style.h` | 删 `#panshiUsageBar` 两条规则 + 11 个失效变量 + 对应 replace |

---

## 五、复测清单

> 环境提醒：`QT_LOGGING_RULES` 屏蔽 qInfo，调试请用 **qWarning**。
> 助手侧 `Bash` / `Grep` 仍为 `exit 251`，以下需在实机执行。

### 5.1 编译

- [ ] `cmake --build build -j$(nproc)` → **0 error / 0 warning**
      （重点看：`-Wunused-variable` 是否已随 `style.h` 清理消失；
       若还有，检查是否有其它文件引用了被删的 `panshiSoftFill` 等 —— 它们是
       `inline` 自由函数，属公开 API，本阶段**未删**）

### 5.2 O2 概览容量条（`DProgressBar`）

- [ ] 概览卡右侧容量条正常显示，百分比文案在条上居中（`setFormat` 生效）
- [ ] 容量条圆角 / 高度 / 颜色与替换前无可见差异（DTK 原生外观）
- [ ] 无盘符场景（`capacity == 0`）显示 `--`，不崩
- [ ] 深浅主题切换后容量条颜色跟随（无 QSS 残留）

### 5.3 O5 三个按钮

- [ ] 「监控设置」打开设置窗口 → 「立即启动」（服务未跑时）点击正常触发
- [ ] 排行下钻后「返回上级」显示并可点击
- [ ] 目录时间线模式「返回系统盘」显示并可点击
- [ ] 三按钮无外观变化（本就是 `DPushButton`，只是声明纠正）

### 5.4 O6 基线倍数

- [ ] 设置为「智能基线」模式时，基线倍数框**可编辑**、显示 `3.0`（1 位小数）
- [ ] 改成 `5.5` → 关闭并重开设置窗口 → 值为 `5.5`（配置已落盘）
- [ ] 切到「固定阈值」模式 → 倍数框**置灰**；阈值框变为可编辑（双向联动正确）
- [ ] 上下微调按钮步进 0.1，范围钳在 1.0~10.0

### 5.5 O4 排除目录

- [ ] 输入目录 → 「添加」→ 列表新增一行，输入框清空
- [ ] 重复输入同一目录 → 不重复添加
- [ ] 选中一行 → 「移除选中」→ 该行消失
- [ ] **未选中**直接点「移除选中」→ 无反应、不崩（`currentIndex().row() == -1`）
- [ ] 关闭重开设置窗口 → 列表内容与配置一致（`setStringList` 生效）
- [ ] 列表可滚动（超过 `maximumHeight 110` 的行数时），选中行高亮正常

### 5.6 O1 自定义日期范围（本轮重点）

- [ ] 点「自定义」→ 对话框只有**一个**日历，高度明显比旧版矮
- [ ] 初值：从当前已选范围带入；无范围时回落到最近 7 天
- [ ] 第 1 次点 10 号 → 摘要显示「10 号 ~ 10 号」；第 2 次点 20 号 → 「10 号 ~ 20 号」
- [ ] 第 1 次点 20 号 → 第 2 次点 10 号（更早）→ 摘要变「10 号 ~ 10 号」，
      再点第 3 次（如 15 号）→ 「10 号 ~ 15 号」
      （验证「第二次早于首次 → 改为新起点」的分支）
- [ ] 未来日期不可点（`setMaximumDate` 生效）
- [ ] 只点 1 次就「确定」→ 按单日区间查询（不崩、不比反）
- [ ] 「取消」→ 不改变当前范围
- [ ] 确定后时间线按新范围加载，状态标签从「加载中 N/N…」走到终态
- [ ] **摘要与日历高亮始终一致**（本轮的修复目标；任意点击序列下都不该分叉）

### 5.7 保留项 + 回归

- [ ] 增长详情 / 占用排行两表正常（`QTableWidget` **有意保留**，本阶段未动）
- [ ] 立即扫描 / pkexec 提权 / 实时进度正常
- [ ] 排行下钻 + 帧栈返回 + 打开位置正常
- [ ] 时间线懒加载（切范围 / 点数据点）正常
- [ ] 窄窗（WM 最小宽 920）下各控件完整可达

---

## 六、遗留

| 项 | 说明 |
|---|---|
| `QTableWidget` ×2 | **有意保留** —— DTK 未提供等价表格控件；设计方案 §5.2 第 6 条已允许。表格的颜色职责已交还 `DPalette`（阶段 3-6） |
| `QCalendarWidget` | 仍是 Qt 原生（DTK 无日历控件）。本轮改的是**数量与交互范式**（2 → 1），未改变控件本身。若将来 DTK 提供日历控件可再迁 |
| `panshiSoftFill()` / `panshiBlend()` / `panshiColor(ColorType)` | 保留 —— `style.h` 的 `inline` 公开 API，删了可能波及其它 TU；本阶段只删「确定无引用」的局部变量 |

---

## 七、下一步

阶段 5 遗留清单（§六）中，O1/O2/O4/O5/O6 在本阶段勾销，本页 Qt 原生控件清理
**到此收口**（除有意保留的 `QTableWidget`）。

后续候选（均需特定环境，非本阶段范围）：

- **i18n 接入**：`CMakeLists.txt` 加 `qt_add_translations()` + 建 `translations/`
  + 确认 `.qm` 安装路径与 `main.cpp` 的 `loadTranslator` 匹配；
- 阶段 4 设置搜索索引的编译复测（12 点清单见阶段 4 文档 §八）；
- 跨页跳转 C4（开可写层+重启，破坏性）、C7（需 sudo 阻塞）；
- 深色主题选中行/焦点框、服务徽章 Warning/Danger 态、使用率 75~90/≥90 两档、
  `TextLively` 正常主题取值等实机项。
