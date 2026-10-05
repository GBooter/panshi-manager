# UI 重构 · 阶段 5 附：跨页跳转携带动作（到达后动作）

> 对应设计方案 §5.2 第 10 条。本文件记录实现、时序设计与验收要点。
> 完成日期：2026-10-02　·　**状态：已复测通过**（C1/C2/C3/C5/C8 + 窄窗 922 全过；
> C4/C7 环境不可达；C6 UI 层不可构造，机制经代码审查确认）。

---

## §一、问题

总览页「快速操作」原有三个入口只做**跳页**：

```cpp
connect(bBackup, &DPushButton::clicked, this,
        [this] { m_win->gotoPage(PageId::BackupVersion); });
connect(bUpdate, &DPushButton::clicked, this,
        [this] { m_win->gotoPage(PageId::Update); });
connect(bProtect, &DPushButton::clicked, this,
        [this] { m_win->gotoPage(PageId::Protection); });
```

问题有两层：

1. **语义断裂**。按钮文案是「立即创建备份」，但点了只跳页 —— 用户到了备份页还得再找一次「创建备份」按钮。一次意图被拆成两次点击，且第二次点击的位置需要用户自己寻找。
2. **备份页还有一个隐性断点**。备份页「还原」在检测到**可写层冲突**时弹确认框 →「前往设置」→ 只跳保护页，不指出该动哪一行。用户到了保护页面对 3 张状态卡 + 开关行 + 折叠区，仍需自行推断"要关可写层"。

---

## §二、时序设计（本功能的真正难点）

**不能**在 `connect` 里写成"跳页 → 立即执行动作"两连击，原因有两处：

| | 现象 |
|---|---|
| **数据未就绪** | `switchTo()` 里的 `refresh()` 是**异步**的（`runAsync` / `runAsyncQuiet`）。跳页后立刻弹创建对话框，用户看到的是上一轮的旧快照列表。 |
| **动作被静默丢弃** | 各页动作入口普遍带防重入守卫 `if (m_running) return;`。refresh 在途时 `m_running == true`，动作被直接 return 掉 —— 用户看到的表现是**"点了没反应"**（备份页还会留一条"上一项操作尚未完成"的提示，其余页连提示都没有）。 |

因此需要一个「**到达后动作**」机制：动作挂到目标页"空闲"事件上，等 `refresh()` 真正结束再执行。

### 2.1 空闲信号从哪来 —— `BasePage::idleReached()`

由基类在 `runAsync` / `runAsyncQuiet` 层统一计数（`m_inFlight`），归零时发信号。页面无需各自上报，与本项目 `registerResponsiveCardRow` / `setFeedbackBanner` 同一「注册点」范式。

两个关键实现细节：

- **`onDone(res)` 必须排在 `endTask()` 之前**。总览页是"快速状态 → 慢速统计"两段式：`onLoadedFast` 里会再起一段 `runAsync`。若先 `endTask()` 再 `onDone`，第一段结束瞬间计数归零 → 误发 `idleReached()` → 到达动作在第二段数据尚未落地时就跑了。现在的顺序保证计数先抬到 2 再落回 1，全程不归零。
- **`isBusy()` 必须是 `virtual`**。见下节。

### 2.2 复合页要聚合子页忙碌状态 —— `VersionManagementPage`

`VersionManagementPage` 是个**复合页**：它的 `refresh()` 只是转发给当前子页，在途任务计数落在 `SnapshotsPage` / `DeploysPage` 身上，本页 `m_inFlight` 恒为 0。

若不处理，`MainWindow` 会认为"已到达"并立即执行动作 —— 恰好踩中上面第 2 条陷阱。

处理有两步：

```cpp
// versionpage.h
bool isBusy() const override;        // 聚合两个子页

// versionpage.cpp
bool VersionManagementPage::isBusy() const {
    return (m_snapPage && m_snapPage->isBusy()) || (m_deployPage && m_deployPage->isBusy());
}
```

以及把子页的 `idleReached` 转达到本页：

```cpp
connect(m_snapPage, &BasePage::idleReached, this, [this] {
    if (!m_snapPage->isBusy() && !m_deployPage->isBusy())
        emit idleReached();
});
```

### 2.3 MainWindow 侧：挂起 → 等待 → 执行

```cpp
void gotoPage(PageId id, std::function<void()> afterArrive);              // 新增
void gotoPage(PageId id, SubPageId sub, std::function<void()> afterArrive); // 新增
```

`scheduleArrivalAction()` 的三条路径：

| 路径 | 条件 | 行为 |
|---|---|---|
| **立即执行** | `!target->isBusy()` | 该页 `refresh()` 同步完成（`UpdatePage::refresh` 是空实现）或已有缓存瞬间返回 —— 等一个永不到来的信号会卡死，直接执行。 |
| **等信号** | `target->isBusy()` | 连 `idleReached`，同时启动 `kArrivalTimeoutMs = 4000` 兜底定时器。 |
| **超时** | 4s 内没等到 | 强制执行。后端卡住时最坏是动作被防重入守卫弹回提示，也远好过"点了毫无反馈"。 |

**后到覆盖先到**：同一时刻只保留一个待执行动作。用户连点两个快速操作时，最后一次点击最符合其当前意图；覆盖时先 `disconnect` 旧宿主的连接，避免旧页后台空闲时触发已作废的动作。

`flushArrivalAction()` 里**先清状态再执行**：动作内部可能再次 `gotoPage`（备份页"可写层冲突 → 去保护页"就是这种情况），那时 `scheduleArrivalAction` 看到的必须是干净状态。

`closeEvent()` 里调 `flushArrivalAction(false)`：退出时挂起动作作废（不执行），只解绑连接与定时器，避免关闭过程中回调到正在析构的页面。

---

## §三、动作契约

三个到达动作都通过 `QMetaObject::invokeMethod` 调用，原因：总览页**不持有**目标页头文件（避免页面层互相包含），且这些入口是各页的私有实现。

为此给三个页各加一个 `Q_INVOKABLE` 的"对外动作契约"：

| 页面 | 对外方法 | 内部动作 |
|---|---|---|
| `VersionManagementPage` | `requestCreateSnapshot()` | `showSubPage(0)` → 转发 `SnapshotsPage::onCreate` |
| `UpdatePage` | `requestCheckUpdate()` | `onCheck()` |
| `ProtectionPage` | `highlightEnableProtect()` | 滚入视野 + `m_rowEnable->flashHighlight()` |

「创建备份」需要先落到「备份快照」子页（从总览跳来时子页可能仍停在「系统版本」），因此 `requestCreateSnapshot()` 内部先 `showSubPage(0)`。

### 3.1 保护页的高亮而非自动执行

备份页可写层冲突时跳保护页，只做**定位高亮**，**不自动执行**关闭可写层。理由：关闭可写层是危险操作（卸载只读保护），必须由用户在知情前提下自己确认。跨页跳转负责"把用户带到正确的地方并指出那一行"，不代替用户做决定。

`m_rowEnable` 因此从局部变量提升为成员。它属于"常规操作平铺"区，**不在折叠区内**，所以不需要先展开折叠区。

---

## §四、改动的文件

| 文件 | 改动 |
|---|---|
| `src/pages/basepage.h` | 新增 `signals: void idleReached()`、`virtual bool isBusy()`、`beginTask()`/`endTask()`、成员 `m_inFlight`；`runAsync`/`runAsyncQuiet` 接入计数 |
| `src/pages/basepage.cpp` | 实现 `beginTask` / `endTask` |
| `src/mainwindow.h` | 新增两个 `gotoPage` 重载、`scheduleArrivalAction`、`flushArrivalAction`、成员 `m_arrivalAction`/`m_arrivalTimer`/`m_arrivalPage`、常量 `kArrivalTimeoutMs`；`page()` 由 private 提为 public |
| `src/mainwindow.cpp` | 实现上述；`closeEvent` 加 `flushArrivalAction(false)`；补 `#include <QTimer>` |
| `src/pages/versionpage.h/.cpp` | 新增 `requestCreateSnapshot()`、覆写 `isBusy()`、转达子页 `idleReached` |
| `src/pages/updatepage.h` | 新增 `requestCheckUpdate()` |
| `src/pages/protectionpage.h/.cpp` | 新增 `highlightEnableProtect()`；`m_rowEnable`/`m_scroll` 提升为成员 |
| `src/pages/overviewpage.cpp` | 三个快速操作改为"跳页 + 到达动作" |
| `src/pages/snapshotspage.cpp` | 可写层冲突跳转改为"跳页 + 高亮开启保护行" |

**未改**：`updatepage.cpp`、`CMakeLists.txt`（无新文件）。

---

## §五、验收要点（复测结果）

| # | 场景 | 结果 |
|---|---|---|
| C1 | 总览点「立即创建备份」→ 跳「备份快照」子页 + 创建对话框自动弹出（名称预填时间戳每次更新，证明是新弹非残留） | ✅ 通过 |
| C2 | 总览点「检查系统更新」→ 跳更新页 + 自动执行（失败 Banner「sudo: 需要密码」即自动执行的证据） | ✅ 通过 |
| C3 | 总览点「保护设置」→「开启保护 (恢复只读)」行闪一下高亮，峰值→淡出→全褪周期正常 | ✅ 通过（依赖代修 4） |
| C4 | 备份页「还原」遇可写层冲突 → 跳保护页 + 同一行高亮 | ⛔ 环境不可达（需开可写层 + 重启，破坏性） |
| C5 | 慢页跳转：动作等 `idleReached` 而非立即执行，未出现"点了没反应" | ✅ 通过 |
| C6 | 连续点两个快速操作，只有最后一次生效 | ⚠️ UI 层不可构造（动作 <150ms 完成、首击即切页、模态框吞掉后续点击），机制以代码审查确认 |
| C7 | sudo 阻塞下 4s 超时兜底 | ⛔ 环境不可达（无免密 sudo，无法构造阻塞） |
| C8 | 动作执行前关闭窗口 | ✅ 通过（详见 §5.1） |
| — | 窄窗 922 回归 | ✅ 通过（总览 3+1 折行、开关可见；保护页直接 3 列） |

### 5.1 C8：之前"进程残留"是测试工具误差，已撤回

先前报告的「模态框开着关窗 → 进程残留 7:42」**结论撤回**。真相：

- `xdotool windowclose` 执行的是 **XDestroyWindow 强拆**，不走 `WM_DELETE_WINDOW` 关闭协议
  → Qt 的 `quitOnLastWindowClosed` 逻辑根本不触发，事件循环继续跑。
  进程只有 4 线程、主线程停在事件循环（非析构阻塞、无 QtConcurrent 残留）；
  窗口从 X 会话消失只是被强拆。
- 本轮早先用同样手法测正常路径也得到同样的假"挂起"，才暴露该手法本身不可靠。
- 改用 ctypes 手发 `WM_DELETE_WINDOW` 协议消息重测：

| 场景 | 结果 |
|---|---|
| 正常路径协议关闭 | 进程 2s 内干净退出、窗口消失 ✅ |
| 模态框开着协议关闭 | 窗口/对话框/进程均无变化——**Qt 标准模态语义**（先关对话框才能关主窗），安全行为非挂起 ✅ |
| 取消对话框后再协议关闭 | 干净退出 ✅ |

即：应用自身的关闭行为（`closeEvent` → `flushArrivalAction(false)` + 各页 `shutdown()`）**完全正确**。

> **方法论**：验证"关窗行为"必须走协议关闭（`WM_DELETE_WINDOW` / `QCloseEvent`），
> 不能只看"窗口从桌面上消失了"。`xdotool windowclose` 是强拆，会给出假阴性
> （甚至假阳性——正常路径也被判成挂起）。这条同样适用于日后的退出流程回归。

### 5.2 复测期代修 4 处（均为编译/逻辑缺陷）

| # | 文件 | 问题 | 修法 |
|---|---|---|---|
| 1 | `basepage.cpp` | `Space::M` 未加限定（该文件无 `using namespace PanshiUi`） | → `PanshiUi::Space::M` |
| 2 | `components.h` | `QPropertyAnimation` 前置声明写在 `namespace PanshiUi` 内 → 声明出 `PanshiUi::QPropertyAnimation` 这个不存在的嵌套类型，cpp 按完整类型用时报 **incomplete type** | 前置声明移出 namespace |
| 3 | `sidebar.h` | `QLineEdit *m_search` 与 `sidebar.cpp` 用法不一致 | → `DLineEdit *` |
| 4 | `components.cpp` | `flashHighlight` 的 `connect(..., Qt::UniqueConnection)` —— **`UniqueConnection` 不支持 lambda**，运行时被拒、connect 静默失败 → peak 后无人 `start()` 动画 → **高亮卡在满亮永不淡出** | 改为「仅当未创建时 connect 一次」，去掉 `UniqueConnection` |

**代修 4 的连带修复（本轮补充）**：同一缺陷**在 `masterswitch.cpp` 的
`PanshiSwitchRow::flashHighlight` 里也存在**——上一轮只修了 `components.cpp`。
该路径由设置搜索落到「磐石功能」开关行时触发（`highlightSetting` → 元对象调用），
症状同样是高亮不淡出。本轮已一并修正，两处实现现已一致。

> **教训**：`Qt::UniqueConnection` 与 lambda 不兼容。这类缺陷**不报编译错**，
> 只在运行期静默失败，且症状会被误判为"动画参数不对"。
> 同一模式的复制粘贴处必须同步修（本处两处实现是刻意分写以隔离 moc 依赖，
> 但**分写意味着改一处必须找另一处**）。

### 5.3 顺带清理（本轮）

- `PanshiSkeleton` 的两处 `Motion::PulsePeriod / Motion::PulseStep` 整数除法
  收敛为具名 `constexpr int pulseSteps()`，消掉 `-Warithmetic` 诊断并表达意图。
- `PanshiRow` / `PanshiSwitchRow` 的 `peakMs = 1200` / `fadeMs = 320` 原为局部
  字面量（违反"度量取自 tokens.h"的既定原则）→ 新增
  `Motion::HighlightHold = 1200` / `Motion::HighlightFade = 320`。
  **与 `PulsePeriod` 同值但语义不同**（骨架屏明暗周期 vs 高亮停留时长），
  故各自具名、不互相引用，便于将来独立调整。


---

## §六、与既有机制的边界

- **不与「设置搜索」冲突**：搜索落地走 `gotoSetting()` → `switchTo()` + `highlightSetting()`，**不经**到达动作队列。两条路径互不干扰（搜索的定位是同步的、行已存在）。
- **不与 `showDiskDate()` 冲突**：它走 `gotoPage()` + `focusDate()`，同样是"跳页 + 立即动作"。未改造成到达动作是因为磁盘页 `focusDate` 只操作已有控件、不依赖 refresh 完成。若后续发现时序问题，可平移过来。
