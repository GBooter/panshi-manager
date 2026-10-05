# UI 重构进度 — 阶段 4：交互流程统一（第一批）

> **复测轮已结束**：用户侧代修 2 处编译错误（均为我方实现缺陷，见 §八），
> 修后 **0 error / 0 warning**；可验证项**全部通过**。
> 成功路径因本机后端走 `sudo` 且无免密而无法端到端验证（根因已定位，见 §九）。
> 阶段 4 剩余项仅剩**设置搜索索引**（见 §六）。

---

## 一、本阶段目标

阶段 0–3 把各页的**视觉与结构**收敛到了组件层（PanshiPanel / Section / Row / StateCard /
Banner / Collapsible / 三档加载）。阶段 4 处理剩下的**交互流程**层面不统一：

| 问题 | 现状（阶段 3 结束时） |
|---|---|
| 决策对话框 | `ConfirmRequest`（阶段 0 建好、3-6 首次真实使用）与**裸 `DDialog`** 并存 |
| 操作结果反馈 | 各页 `PanshiInlineBanner` 与 `Ui::infoDialog`/`errorDialog`（**模态**）并存 |
| 执行期加载 | 各页 L2 局部加载 与 `masterswitch` 的**整页遮罩**并存 |
| 颜色职责 | 组件层 `DPalette` 与 `style.h` 的 **QSS 属性选择器**（`[stat=...]`）并存 |

本批聚焦**「磐石功能开关」这条贯穿总览页 / 保护页的链路**——它是上述四条唯一的
"全都没收敛"的路径，且是用户最高频的开关操作。

---

## 二、改动清单

| 文件 | 改动 |
|---|---|
| `src/ui/components.h` | `PanshiInlineBanner` 私有区加 `class QTimer *m_dismissTimer = nullptr;` |
| `src/ui/components.cpp` | `autoDismiss()` 改可取消定时器；`setText()` 换内容时取消待定隐藏 |
| `src/pages/masterswitch.h` | `Presentation::stat`(`const char *`) → `PanshiUi::Semantic semantic`；加 `m_detail` |
| `src/pages/masterswitch.cpp` | 全量重写（详见 §三） |
| `src/pages/basepage.h/.cpp` | 新增 `setFeedbackBanner()` / `showFeedback()` / `m_feedback` |
| `src/pages/common.h` | 删 `Ui::infoDialog` / `Ui::errorDialog` + `#include <DDialog>` |
| `src/pages/overviewpage.h/.cpp` | 新增页级反馈 Banner + `setFeedbackBanner()` |
| `src/pages/protectionpage.cpp` | `setFeedbackBanner(m_banner)`；`setProtect()` 反馈改走 `showFeedback()` |
| `src/style.h` | 删 `#panshiSwitchStatus`(4) + `#panshiLinkBtn`(2) |

---

## 三、四处关键改造

### 3.1 反馈宿主：`BasePage::setFeedbackBanner()`

**问题**：`PanshiSwitch::requestSwitch(BasePage *page, ...)` 是总览页与保护页**共用**的逻辑，
它只拿得到 `BasePage *`，不认识任何页面的内部结构。执行结果该写进哪条 Banner？

**被否方案**：让 `PanshiSwitchRow` 自己内建一条 Banner。
→ 组件层将被迫假设"自己上方有一个可放 Banner 的位置"，而它在两页分别位于
`PanshiSection` 内部；且保护页已有一条 `m_banner`，会出现两条反馈条。

**采用方案**：基类加注册点。

```cpp
// basepage.h
void setFeedbackBanner(PanshiUi::PanshiInlineBanner *banner) { m_feedback = banner; }

protected:
    /** 成功：Success + 可关闭 + 自动消失；失败：Danger + 可关闭 + 常驻 */
    void showFeedback(PanshiUi::Semantic s, const QString &text, bool autoHide = false);

private:
    PanshiUi::PanshiInlineBanner *m_feedback = nullptr;   // 未注册时为 nullptr
```

页面构造时认领：

```cpp
// protectionpage.cpp —— 复用已有的 m_banner
m_banner = new PanshiInlineBanner(scrollContent);
setFeedbackBanner(m_banner);

// overviewpage.cpp —— 本页原先没有反馈条，新增一条（紧跟 L2 加载控件）
m_feedback = new PanshiInlineBanner(scrollContent);
m_feedback->hide();
setFeedbackBanner(m_feedback);
```

这与 `registerResponsiveCardRow`（宽度喂入）、`runAsyncQuiet`（静默刷新）是同一收敛思路：
**页面把自身结构告诉基类，共享逻辑只与基类对话**。未注册的页面不因反馈而中断流程。

### 3.2 关闭开关：3 个等权按钮 → 单选

**原实现**

```cpp
dlg.addButton(tr("取消"), Normal);
dlg.addButton(tr("临时关闭 (重启后自动恢复)"), Recommend);
dlg.addButton(tr("完全关闭 (重启后也关闭)"), Warning);
const int ret = dlg.exec();
```

问题有三：
1. **关键差别藏在括号里** —— 三个按钮都叫"关闭"，"临时/完全"只在副文案；
2. **用按钮次序表达风险等级** —— 用户读不出"第三个更危险"，只看到"更靠右"；
3. **取消/临时/完全 是等权选项**，但语义上它是"一个决策 + 一个程度选择"。

**新实现**（`PanshiUi::ConfirmRequest`）

```cpp
req.level         = DialogLevel::Warning;
req.title         = tr("关闭磐石保护");
req.message       = tr("关闭后核心目录将解除只读保护，恶意软件与误操作可能直接破坏系统。");
req.consequences  = { tr("核心系统目录将变为可写"),
                      tr("恶意软件或误操作可直接破坏系统文件"),
                      tr("重启后按所选方式决定是否恢复保护") };
req.options       = { tr("临时关闭（重启后自动恢复）"),
                      tr("完全关闭（重启后仍保持关闭）") };
req.optionDefault = 0;        // 默认落在低风险的「临时关闭」
req.optionResult  = &choice;  // 回写用户选择
req.confirmText   = tr("关闭保护");
```

- 后果清单（`consequences`）由组件统一加 `"· "` 前缀 + `Semantic::Warning`；
- 选项用 `DRadioButton` 呈现，**默认选中低风险项**（原实现 Recommend 按钮的语义被正确继承）；
- 选「完全关闭」再进 `PanshiCountdownDialog`（30s 倒计时 + 风险清单 + 「我已知晓风险」），
  与既有危险操作路径一致；
- 「临时关闭」低风险，**不再二次确认**（与原实现相同）。

**开启分支**（拨到 ON）只做普通确认：

```cpp
req.level = DialogLevel::Normal;
req.title = tr("开启磐石保护");
req.message = tr("确定要恢复核心目录只读保护吗？此操作需要重启后生效。");
req.confirmText = tr("恢复保护");
```

### 3.3 反馈：模态对话框 → 页内 Banner

**原实现**：执行完成后弹 `Ui::infoDialog` / `Ui::errorDialog`（**模态**，`exec()`）。
用户必须再点一次「确定」才能继续操作，且对话框会盖住刚被改动的开关行。

**新实现**：

```cpp
if (r.ok)
    page->showFeedback(Semantic::Success, tr("设置成功，请重启系统使设置生效。"), true);  // 5s 自动消失
else
    page->showFeedback(Semantic::Danger,  tr("设置失败：%1").arg(r.errorMessage));        // 常驻，需用户关闭
```

反馈与操作对象同屏可见，失败信息留在页面上而不是"点掉就没了"。

**顺带清理**：`Ui::infoDialog` / `Ui::errorDialog` 的调用点经全量核查**只剩
`masterswitch.cpp` 两处**（`deployspage.cpp` / `snapshotspage.cpp` 引用 `DDialog` 是用于
其他用途），迁移后即删除这两个内联函数，并从 `common.h` 移除 `#include <DDialog>`。

### 3.4 颜色：QSS 属性选择器 → `DPalette`

**原实现**（`masterswitch.cpp`）

```cpp
m_subtitle->setProperty("stat", p.known ? QLatin1String(p.stat) : QString());
m_subtitle->style()->unpolish(m_subtitle);
m_subtitle->style()->polish(m_subtitle);
```

配套 `style.h`：

```css
#panshiSwitchStatus[stat="green"]  { color: %SUCCESS%; }
#panshiSwitchStatus[stat="orange"] { color: %WARNING%; }
#panshiSwitchStatus[stat="red"]    { color: %DANGER%; }
```

**问题**：
1. 颜色键是 `const char *` 字符串，**编译器无法校验拼写**（写错就静默变灰）；
2. `unpolish/polish` 是重样式的高开销路径；
3. 颜色意图分散在 C++ 与 QSS 两处，违反"颜色职责归 DPalette"的既定原则。

**新实现**：

```cpp
// masterswitch.h
struct Presentation {
    bool known = false;
    bool protectedOn = false;
    QString subtitle;
    PanshiUi::Semantic semantic = PanshiUi::Semantic::Muted;   // 类型安全
};

// masterswitch.cpp
PanshiUi::applySemantic(m_subtitle, p.known ? p.semantic : PanshiUi::Semantic::Muted);
```

映射（沿用组件层既有约定）：保护中 → `Success`、临时关闭 → `Warning`、
完全关闭 → `Danger`、未知 → `Muted`。

「详情」按钮同理去掉 `#panshiLinkBtn`。**注意**：这里不能用 `applySemantic` ——
`setForegroundRole(DPalette::ColorType)` 重载**仅 `DLabel`/`DTipLabel` 提供**
（本轮编译错误实证，详见 §八），按钮必须改设 palette：

```cpp
m_detail = new DPushButton(tr("详情"), this);
m_detail->setFlat(true);
m_detail->setCursor(Qt::PointingHandCursor);
// DPushButton 没有 setForegroundRole(DPalette::ColorType) 重载，
// 故直接设 palette 的 ButtonText —— 与被删掉的 #panshiLinkBtn QSS 等效
QPalette pa = m_detail->palette();
pa.setColor(QPalette::ButtonText, PanshiUi::semanticColor(PanshiUi::Semantic::Accent));
m_detail->setPalette(pa);
```

**颜色不是唯一载体**（WCAG 1.4.1）：副标题文案本身（保护中 / 已临时关闭 / 已完全关闭）
已完整表达状态，颜色只是强化。

---

## 四、组件层缺陷修复：`PanshiInlineBanner::autoDismiss` 竞态

**原实现**

```cpp
void PanshiInlineBanner::autoDismiss(int ms)
{
    QTimer::singleShot(ms, this, [this] { hide(); });
}
```

`singleShot` **不可撤销**。同一页面的同一 `m_banner` 被反复复用时（3-2 / 3-4 / 3-6 各页
都是这个用法），若"成功提示 5s 内又来一条新反馈"：

```
t=0   反馈 A → setText(A) + autoDismiss(5000)   ← 定时器 T1 排队
t=2   反馈 B → setText(B) + autoDismiss(5000)   ← 定时器 T2 排队
t=5   T1 触发 → hide()                          ← 刚显示 3 秒的 B 被隐藏
```

症状是"提示刚出现就消失"，极易被误判为布局问题。

**新实现**

```cpp
// components.h 私有区
class QTimer *m_dismissTimer = nullptr;

// components.cpp
void PanshiInlineBanner::autoDismiss(int ms)
{
    if (!m_dismissTimer) {
        m_dismissTimer = new QTimer(this);
        m_dismissTimer->setSingleShot(true);
        QObject::connect(m_dismissTimer, &QTimer::timeout, this, [this] { hide(); });
    }
    m_dismissTimer->start(ms);   // 重复调用 = 重置倒计时
}

void PanshiInlineBanner::setText(const QString &text)
{
    // 换内容 = 换一次反馈：取消上一条待定的自动隐藏
    if (!text.isEmpty() && m_dismissTimer)
        m_dismissTimer->stop();
    m_text->setText(text);
    setVisible(!text.isEmpty());
}
```

隐藏而非清空文本：内容保留，调用方再次 `setText` 仍可复用同一条 Banner。

---

## 五、复测结果（用户侧，含 2 处代修后）

### 通过项

| # | 验证点 | 结果 |
|---|---|---|
| 1 | 编译 | ✅ **0 error / 0 warning**（代修 2 处后） |
| 2 | 副标题语义色 | ✅ 两页「保护中 · 核心目录只读」均呈**主题色**（Success→Highlight，非灰） |
| 3 | 详情按钮跳转 | ✅ 总览 → 保护页正常；主题色 + 平铺样式 |
| 4 | 决策框：3 条后果 + 单选 | ✅ 红点清单 3 条齐全；◉ 临时关闭（**默认选中**）/ ○ 完全关闭 |
| 5 | 完全关闭 → 30s 倒计时 | ✅ 29s 环 + 3 条风险清单 + 确认按钮倒计时期间灰置；**中途取消无损**（开关回 ON、状态未变） |
| 6 | 失败分支不弹模态框 | ✅ **真实验证**：页内 Banner「设置失败：sudo: 需要密码」，红图标 + 常驻（6.5s 后仍在）+ × 可关 + 关后无残留；失败后开关/状态卡正确回显未变 |
| 7 | 窄窗 920 无回归 | ✅ 总览开关/详情可见、卡片 3+1；保护页直接 3 列 |

### 无法验证（环境限制，非遗漏）— 根因见 §九

临时 / 完全两条**真执行路径**、成功 Banner 5s、Warning / Danger 态副标题、
`autoDismiss` 定时器重置 —— 本机后端走 `sudo` 且无免密，任何开关操作必然失败，
**成功分支到达不了**。

用户明确：定时器修复（成员 QTimer + `start` 重置）**属代码层正确**，
建议在有免密 sudo 的环境补验一次；Danger 态真执行破坏性太大，**即使有免密也不建议自动测**。

### 用户备注

- 失败信息透传清晰（`errorMessage` 原样带出「sudo: 需要密码」），运维可诊断。
- 系统状态无变更：所有真执行均失败或取消，保护保持开启；窗口已恢复默认 1854x1225。

### 挂起待验（累积）

深色主题选中行/焦点框（dconf 沙箱限制）、`reboot3` 策略落盘（破坏性）、
服务徽章 Warning/Danger 态、使用率 75~90 / ≥90 两档、`TextLively` 正常主题取值、
图例「异常增长」色点（本机 `TextLively` 降级环境已知）——**均与本批改动无关**。

---

## 六、未做（阶段 4 剩余项）

| 项 | 说明 |
|---|---|
| **设置搜索索引** | `PanshiRow::title()` / `description()` 已就位但**仍零调用者**；需建立"页面 → 设置行"的索引并接搜索框（**阶段 4 唯一剩余功能项**） |
| `style.h` 结构性 QSS | `#panshiContent` / `#panshiSidebar` / `#panshiCard` 等保留（承担布局职责，符合"QSS 只管布局，颜色归 DPalette"原则） |
| `QProgressBar#panshiUsageBar` | 基础 2 条保留（`objectName` 仍在使用） |
| 表单型对话框 | `snapshotspage::onCreate` 的 `setFixedWidth(480)` + `DLineEdit`/`DTextEdit` 属独立场景（输入表单而非决策确认），不纳入本次收敛 |

---

## 七、本轮教训

**「反馈条复用」必须自带取消——`QTimer::singleShot` 是不可撤销的。**

任何"同一条 Banner 承接多次反馈"的设计，都要把定时器做成**成员**并在换内容时重置，
否则旧定时器会误杀新内容。症状具有误导性：表现为"提示刚出现就消失"，
看起来像布局/可见性问题，实际是定时器生命周期问题。

**推论**：凡是"组件会被复用"的地方（Banner、状态标签、进度控件），
任何与时间相关的副作用都必须显式管理生命周期，不能用 `singleShot` 这类一次性 API。

---

## 八、复测轮：2 处编译错误（均为我方实现缺陷）

### 8.1 `applySemantic` 不接受 `DPushButton`（`masterswitch.cpp:46`）

我写了 `PanshiUi::applySemantic(m_detail, PanshiUi::Semantic::Accent);`，
但 `m_detail` 是 `DPushButton*`。

**用户查 DTK6 头文件确认**：`setForegroundRole(DPalette::ColorType)` 重载
**仅 `DLabel` / `DTipLabel` 提供，`DPushButton` 没有**。
→ `PanshiUi::applySemantic(DLabel*, ...)` 的**适用面仅限 DLabel 系**，不是通用工具。

**修法**（与已删的 `#panshiLinkBtn` QSS 等效，视觉已验证正常）：见 §三 的代码块。

**根因**：阶段 3-7 已沉淀「`DPalette::TextTitle` 属 `DPalette::ColorType` 而非
`QPalette::ColorRole`」这条教训，但**没有记录其适用面仅限 DLabel/DTipLabel** ——
知道"用哪个枚举"，却没确认"哪些控件接受这个重载"。

### 8.2 `showFeedback` 声明为 protected（`masterswitch.cpp:153/156`）

我把 `showFeedback` 放在 `basepage.h` 的 `protected:` 区（与 `setLoadingText`、
`registerResponsiveCardRow` 并列），但 **`PanshiSwitch::requestSwitch` 是自由函数、
只拿 `BasePage *`**，无法访问 protected 成员。

**修法**：移到 public 区，并在文档注释里写明原因。

**根因**：设计"基类加注册点"时我已明确"共享逻辑只拿 `BasePage *`"，
但决定可见性时**没有沿同一条推理链检查调用方身份**。
→ **沉淀规则**：基类中「**由共享逻辑回调**的方法」必须 `public`；
「**由页面自己调用**的注册类方法」（如 `setFeedbackBanner`）可留 `protected`。
**定可见性前先问：谁会调用它？**

---

## 九、成功路径受阻的根因（了结一条长期挂起项）

**本机后端命令走 `sudo` 且无免密**（`sudo -n true` 失败）
→ **任何开关操作必然失败** → 成功分支到达不了。

这解释了此前长期挂起项「Success 横幅自动消失（需免密 sudo）」的**真正原因**：
不是代码问题，也不是遗漏，而是**该环境的操作路径天然会失败**。
失败分支反而因此被充分验证（真实 `sudo: 需要密码` 错误透传到 Banner）。

**建议**：在具备免密 sudo 的环境中补验一次成功路径（成功 Banner 5s 自动消失 +
`autoDismiss` 重置逻辑）；Danger 态真执行不建议自动测。
