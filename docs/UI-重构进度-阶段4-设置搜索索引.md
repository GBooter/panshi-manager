# UI 重构进度 · 阶段 4 设置搜索索引

> 阶段 4「交互流程统一」的最后一个功能项。
> 前序：`docs/UI-重构进度-阶段4-交互流程统一.md`（反馈宿主 / 确认流程 / 去掉模态 / QSS 收敛）

---

## 一、要解决的问题

设置项散落在多个页面，用户要找某一项必须先猜「它在哪个页面」。

当前可搜索的设置项分布：

| 页面 | 设置项 | 分组 |
|---|---|---|
| 策略与设置 | 自动清理超额备份 | 备份保留策略 |
| 策略与设置 | 备份数量上限 | 备份保留策略 |
| 策略与设置 | 回滚前备份自动清理 | 备份保留策略 |
| 策略与设置 | 升级失败自动回滚 | 磐石系统参数 |
| 策略与设置 | 最大备份部署数 | 磐石系统参数 |
| 策略与设置 | 自动刷新阈值 | 磐石系统参数 |
| 系统保护 | 磐石功能 | 保护开关 |

共 7 条。「磐石功能」开关在总览页也出现，但搜索一律落到系统保护页——那里才是完整的管理入口（含临时/永久关闭的折叠区）。

---

## 二、三项设计决策（已与用户确认）

| 决策点 | 选择 | 理由 |
|---|---|---|
| 入口位置 | **侧边栏顶部搜索框** | 侧栏是全局常驻元素，不占页面内容高度；与导航同一视线区域，符合「找不到就去侧栏找」的直觉 |
| 第一期覆盖 | **策略页 6 项 + 保护页开关 1 项** | 磁盘页那批设置牵涉弹窗协同（设置窗口 → 页内控件），落地链路更长，放第二期 |
| 键盘导航 | **支持**（↑↓ 选择 / Enter 落地 / Esc 关闭） | 侧栏导航项本身已支持键盘（方案 §3.1），搜索框是其自然延伸；只有鼠标可用会让键盘用户在搜索态「断链」 |

---

## 三、架构：「注册点」范式的第四次应用

本项目已形成的核心模式是——**页面把自身结构告诉基类/宿主，共享逻辑只与基类对话**：

| 注册点 | 登记者 | 共享逻辑 |
|---|---|---|
| `registerResponsiveCardRow` | 页面 | 卡片行宽度喂入 |
| `setFeedbackBanner` | 页面 | 结果反馈写入 |
| `runAsyncQuiet` | 页面 | 静默刷新 |
| **`registerSearchTarget`（本次）** | **页面** | **设置搜索定位** |

本次新增的两个注册侧：

- **条目注册**：页面重写 `searchableSettings()` 自报条目 → MainWindow 收集进 `SettingIndex`
- **定位注册**：页面在构造时 `registerSearchTarget(key, row, scroll)` → 基类按 key 找行并高亮

**关键取舍：索引不持有控件指针。** 条目只记 `pageId + title`，落地时由页面用 `PanshiRow::title()` 自行比对。这样索引不必关心页面何时重建，也避免悬垂。

---

## 四、新增组件

### `src/ui/settingindex.h` / `.cpp`（新建）

```cpp
struct SettingEntry {
    int pageId = -1;      // MainWindow::PageId 的 int 值（避免 ui 层包含 mainwindow.h）
    QString group;        // 分组名（如「备份保留策略」）
    QString title;        // 设置项标题（主要匹配对象）
    QString description;  // 设置项说明（次要匹配对象）
    QString targetKey;    // 页面内定位用的落地键（= PanshiRow 标题）
};

class SettingIndex {
public:
    void add(const SettingEntry &e);
    QList<SettingEntry> query(const QString &kw) const;
    int count() const;
    void clear();
private:
    QList<SettingEntry> m_entries;
};
```

设计取舍（已写入头文件注释）：

- **不是全局单例**：由 MainWindow 持有一个实例，各页在构造时注册。与既有两个注册点同一范式，避免组件层反向依赖具体页面。
- **匹配不做分词**：量级只有 7~十余条，子串匹配足够。
- **加权排序**：`3 = 标题前缀命中` > `2 = 标题子串命中` > `1 = 说明子串命中`。保证搜「备份」时「备份数量上限」优先于说明里提到「备份」的行。
- **同分稳定**：用 `std::stable_sort`（`QList::sort` 不保证稳定），保持注册顺序 = 页面内从上到下的顺序。
- **去重键是 `(targetKey, pageId)`**：同一行被两页注册时只保留首个。
- **不使用 Q_OBJECT**：纯数据 + 查询，无信号需求。

### `PanshiRow` / `PanshiSwitchRow` 的高亮能力

两者各自新增 `flashHighlight()`：

- 语义：Accent 柔和底「闪一下再淡出」——**设置页不是列表，没有「当前项」概念，因此不做常驻选中态**。
- 时序：峰值保持 1200ms + 淡出 320ms（`QEasingCurve::OutCubic`）。
- 用成员 `QPropertyAnimation` + 成员单发 `QTimer`，`stop()` 后重启 —— 连续搜索多次命中同一行时不会叠加动画（与 `PanshiInlineBanner::autoDismiss` 同一课：**可复用的定时/动画必须是可重置的成员对象**）。
- 尊重系统「减少动态效果」：`reduceMotionRequested()` 为真时不做渐隐，直接显示后定时清除。
- 两者各自持有动画对象而不抽公共基类：`PanshiRow` 在组件层、`PanshiSwitchRow` 在页面层，强行合并会把 moc 依赖引入组件层。

---

## 五、改动清单

| 文件 | 改动 |
|---|---|
| `src/ui/settingindex.h` / `.cpp` | **新建**：`SettingEntry` + `SettingIndex` |
| `CMakeLists.txt` | **`src/ui/settingindex.cpp` 加入 `PANSHI_SOURCES`**（漏加会导致链接期 undefined reference） |
| `src/ui/components.h` / `.cpp` | `PanshiRow` 加 `Q_OBJECT` + `Q_PROPERTY(qreal highlight)` + `flashHighlight()` + `highlight()/setHighlight()` + `paintEvent()`；文件头注释同步说明这一例外 |
| `src/ui/sidebar.h` / `.cpp` | 加搜索框（`DLineEdit`）+ 结果列表（`QListView`）+ `setSettingIndex()` + `settingActivated` 回调 + `eventFilter()`；结果角色改用独立区间 |
| `src/mainwindow.h` / `.cpp` | 持有 `SettingIndex`；`initSidebar()` 接线；构造时在 `initContent()` 后收集各页条目；新增 `gotoSetting()` |
| `src/pages/basepage.h` / `.cpp` | 加 `registerSearchTarget()` / `highlightSetting()` / `virtual searchableSettings()` + `SearchTarget` 表 |
| `src/pages/masterswitch.h` / `.cpp` | `PanshiSwitchRow` 加高亮能力；`flashHighlight()` 标 `Q_INVOKABLE` |
| `src/pages/policypage.h` / `.cpp` | 6 个行引用成员 + 注册 + `searchableSettings()` |
| `src/pages/protectionpage.h` / `.cpp` | 开关行注册 + `searchableSettings()` |

---

## 六、三个实现陷阱（本轮新增的教训）

### 6.1 两个模型的角色号撞车

导航模型与结果模型共用同一个 `SidebarDelegate`，而 delegate 一律按 `kGroupRole` / `kDataRole` 取值。

我最初把结果角色也写成 `Qt::UserRole + 1 / + 2 / + 3`，与导航角色**完全重合**——结果行的 `pageId`（存在 `+2`）会被 delegate 读成 `kGroupRole`，于是**每一行搜索结果都被当成「分组标题行」**渲染（32~44px 高、TextTips 小字、无图标）。

**修法**：结果角色移到互不重叠的区间。

```cpp
constexpr int kDataRole       = Qt::UserRole + 1;   // 导航：PageId
constexpr int kGroupRole      = Qt::UserRole + 2;   // 导航：是否分组标题行
constexpr int kTargetKeyRole  = Qt::UserRole + 11;  // 结果：落地键
constexpr int kResultPageRole = Qt::UserRole + 12;  // 结果：PageId
constexpr int kHintRole       = Qt::UserRole + 13;  // 结果：是否提示行
```

**一般化**：多个模型共用一个 delegate 时，角色号必须落在**互不重叠的区间**；否则某模型的业务数据会被 delegate 当成结构标记读取。这类缺陷不会编译报错，只会在运行时表现为「布局莫名错位」。

### 6.2 循环包含 → 元对象调用

`masterswitch.h` 包含 `basepage.h`（`PanshiSwitchRow` 构造需要 `BasePage *`），因此 `basepage.cpp` **不能**包含 `masterswitch.h`。

但 `highlightSetting()` 要对两种行做同一件事。解法：

```cpp
if (auto *r = qobject_cast<PanshiUi::PanshiRow *>(row))
    r->flashHighlight();                                      // 组件层，可直接 cast
else if (row->inherits("PanshiSwitch::PanshiSwitchRow"))      // 页面层，反向包含
    QMetaObject::invokeMethod(row, "flashHighlight", Qt::DirectConnection);
```

配套：`PanshiSwitchRow::flashHighlight()` 必须标 `Q_INVOKABLE`，否则元对象调用静默失败。

### 6.3 `components.h` 的「无 moc」原则被迫开了一个口子

文件头原本写着「本文件所有类不使用 Q_OBJECT（不需要 moc）」。但 `QPropertyAnimation` 必须有 `Q_PROPERTY`，`Q_PROPERTY` 必须有 `Q_OBJECT` —— `PanshiRow` 因此成为唯一例外。

**注释必须同步更新**，否则下一个维护者会照旧注释写出「为什么这里可以用 Q_PROPERTY」的错误结论。已改为：

> 本文件默认不使用 Q_OBJECT（不需要 moc），避免给组件层引入 moc 依赖。唯一例外是 `PanshiRow`：它的搜索高亮需要一个可动画的 `qreal` 属性，`QPropertyAnimation` 必须走 `Q_PROPERTY`，因此该类带 `Q_OBJECT`。新增带 `Q_OBJECT` 的类前请先确认确有此需要，并确认 AUTOMOC 已覆盖本文件。

（本项目 `CMakeLists.txt` 已全局 `set(CMAKE_AUTOMOC ON)`，覆盖无虞。）

---

## 七、落地链路

```
用户在侧栏搜索框输入关键词
  → PanshiSidebar::onSearchTextChanged → refreshResults()
      · 结果非空：隐藏导航列表，显示结果列表，默认选中第 0 行
      · 结果为空：显示「未找到匹配的设置项」提示行（Qt::NoItemFlags）
  → 用户 ↑↓ 选择 / 点击 / 回车
  → PanshiSidebar::activateResult(row)
      · 清空搜索框（触发 textChanged → 恢复导航视图）
      · 调 settingActivated(pageId, targetKey)
  → MainWindow::gotoSetting(pageId, targetKey)
      · switchTo(id)      ← 必须先切页并 refresh
      · p->highlightSetting(targetKey)
  → BasePage::highlightSetting(key)
      · scroll->ensureWidgetVisible(row)   滚到该行
      · row->flashHighlight()              闪一下再淡出
```

**顺序不能反**：隐藏页在 `QStackedWidget` 里尚未布局，此时 `ensureWidgetVisible` 会算错位置。所以先 `switchTo`（触发 `refresh()` 让布局就绪），再定位。

**Esc 处理**：结果列表内按 Esc → 清空关键词 + 焦点交回输入框（`eventFilter`）。符合「搜索态可一键退出」的直觉。

**无索引时降级**：`setSettingIndex(nullptr)` 或从不调用时搜索框隐藏，侧栏退化为纯导航——便于在测试、裁剪构建中保持可用。

---

## 八、待复测点（本轮未经编译验证）

助手侧 `Bash` / `Grep` 长期 `exit 251` 失效，本轮改动**只做了静态自查**（逐文件通读 + 跨文件类型/包含关系核对），未编译。

复测请重点覆盖：

| # | 验证点 | 预期 |
|---|---|---|
| 1 | 编译 | 0 error / 0 warning（重点：`settingindex.cpp` 已入 CMake、`Q_INVOKABLE` 元对象调用、两个 `Q_PROPERTY`） |
| 2 | 搜索框出现 | 启动后侧栏顶部可见「搜索设置」输入框；若不出现说明 `setSettingIndex` 未接线 |
| 3 | 基本匹配 | 输入「备份」→ 出现多条结果；结果行是**正常导航行样式**（16px 图标 + 常规字号，不是 32px 小字分组标题） |
| 4 | 加权排序 | 输入「备份」时「备份数量上限」应排在说明里含「备份」的行之前 |
| 5 | 落地定位 | 选中「自动刷新阈值」回车 → 切到策略页 + 滚到该行 + Accent 柔和底闪一下 |
| 6 | 保护页落地 | 输入「磐石」→ 选中「磐石功能」→ 切到系统保护页，开关行闪光 |
| 7 | 键盘导航 | 输入后 ↑↓ 移动高亮项，Enter 落地，双击结果也可落地 |
| 8 | Esc | 结果列表内按 Esc → 关键词清空 + 导航列表恢复 + 焦点回输入框 |
| 9 | 无匹配 | 输入乱码 → 显示「未找到匹配的设置项」，不可选中、回车无反应 |
| 10 | 减少动态效果 | 系统开「减少动态效果」→ 落地时高亮**不做渐隐**，直接显示后消失 |
| 11 | 连续命中 | 对同一行连续搜索落地两次 → 高亮不叠加、不会停留在半透明状态 |
| 12 | 窄窗无回归 | 920px 窄窗下侧栏布局正常（搜索框 + 列表不互相挤压） |

---

## 九、第二期（未做）

- 磁盘空间页的设置项（区间汇总阈值、告警冷却等）—— 牵涉「打开设置窗口 → 定位到页内控件」的跨容器链路。
- 结果项显示所属分组与页面名（`SettingEntry::group` 已采集，当前仅用于 tooltip）。
- 模糊匹配 / 拼音首字母匹配。

---

## 十、阶段 4 收尾状态

| 项 | 状态 |
|---|---|
| 反馈宿主注册点 + 去掉模态 | ✅ 已复测 |
| 确认流程统一（3 按钮 → 单选 + 后果清单） | ✅ 已复测 |
| `PanshiInlineBanner` 定时器竞态修复 | ✅ 已复测 |
| QSS 属性选择器 → DPalette（`#panshiSwitchStatus` / `#panshiLinkBtn`） | ✅ 已复测 |
| **设置搜索索引** | **⏳ 待编译复测（本文档）** |
| 结构性 QSS（`#panshiUsageBar` 等） | 按设计保留 |
