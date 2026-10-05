# 磐石设置 UI 重构 · 落地进度（阶段 0 + 阶段 1-a）

> 对应方案：`docs/UI-重构设计方案.md`
> 本轮范围：脚手架 + 跳转缺陷修复。**未改动任何现有视觉样式**（`style.h` / `common.h` 原样保留），因此在有构建环境的一侧应能直接编译通过。

## 1. 本轮改动清单

| 文件 | 类型 | 说明 |
|---|---|---|
| `src/ui/tokens.h` | 新增 | 度量 token（间距 / 圆角 / 尺寸 / 动效 / 透明度）。**不含任何颜色与 hex** |
| `src/ui/components.h` | 新增 | UI 组件层声明：`PanshiPanel / PanshiSection / PanshiRow / PanshiBadge / PanshiInlineBanner / PanshiStateCard / PanshiEmptyState / PanshiLocalLoading / PanshiFlatProgress / PanshiSkeleton / PanshiSeparator` |
| `src/ui/components.cpp` | 新增 | 上述组件实现 + 颜色查询 `semanticColor()`、`softFill()`、`themedIcon()`、`reduceMotionRequested()` |
| `src/ui/confirmdialog.h` | 新增 | 统一确认组件 `ConfirmRequest` + `askConfirm()` / `askDanger()` / `createDetailsBox()` |
| `src/ui/confirmdialog.cpp` | 新增 | 实现。倒计时场景复用既有 `PanshiCountdownDialog`（保留不可逆操作的安全等待） |
| `CMakeLists.txt` | 修改 | 新增 `src/ui/components.cpp`、`src/ui/confirmdialog.cpp` |
| `src/mainwindow.h` | 修改 | 引入 `enum class PageId` / `SubPageId`；`QList<BasePage*>` → `QMap<PageId, BasePage*>` |
| `src/mainwindow.cpp` | 修改 | `gotoPage(int)` → `gotoPage(PageId[, SubPageId])`；删除全部位置假设 |
| `src/pages/overviewpage.cpp` | 修改 | 修复 3 处跳转错位 |
| `src/pages/snapshotspage.cpp` | 修改 | 修复 1 处跳转错位 |

## 2. 跳转缺陷修复对照

| 触发入口 | 修复前 | 修复后 |
|---|---|---|
| 总览 ·「立即创建备份」 | `gotoPage(1)` → 磁盘占用 | `gotoPage(PageId::BackupVersion)` |
| 总览 ·「检查系统更新」 | `gotoPage(4)` → 备份策略 | `gotoPage(PageId::Update)` |
| 总览 ·「保护设置」 | `gotoPage(2)` → 备份管理 | `gotoPage(PageId::Protection)` |
| 总览开关 ·「详情」 | `gotoPage(3)` → 系统保护（原本正确，改为强类型） | `gotoPage(PageId::Protection)` |
| 快照还原 · 可写层冲突「前往设置」 | `gotoPage(2)` → 跳到自身（等于无响应） | `gotoPage(PageId::Protection)` |
| `refreshPanshiPages()` | `m_pages.at(3)` / `m_pages.at(0)` | `page(PageId::Protection)` / `page(PageId::Overview)` |

**结构性收益**：`gotoPage(int)` 已不存在，任何新的裸下标跳转都无法通过编译；页面容器改用 `QMap<PageId, BasePage*>`，页面顺序变化不再产生错位。

## 3. 新增组件层用法速查

```cpp
using namespace PanshiUi;

// 分组容器（底色差 + 8px 圆角，无描边）
auto *sec = new PanshiSection(tr("备份保留策略"), tr("无需保存，即时生效"));
sec->addHeaderWidget(new DSwitchButton);          // 组级操作
sec->content()->addWidget(row);

// 行式设置项
auto *row = new PanshiRow;
row->setTitle(tr("自动清理超额备份"));
row->setDescription(tr("超过数量上限时，自动删除最旧的普通备份"));
row->addControl(new DSwitchButton);

// 内联反馈（替代 infoDialog / errorDialog）
banner->setSemantic(Semantic::Danger);
banner->setText(tr("无法关闭保护，可能因为当前有还原任务在进行。"));
banner->addAction(tr("重试"));
banner->autoDismiss(3000);

// 统一确认（确认文案必须是动词）
ConfirmRequest req;
req.title = tr("删除备份");
req.facts = {tr("目标备份: %1").arg(name)};
req.consequences = {tr("该备份将被永久删除，无法恢复")};
req.confirmText = tr("删除");
req.level = DialogLevel::Warning;
if (PanshiUi::askConfirm(this, req)) { /* ... */ }

// 加载三档
auto *sk = new PanshiSkeleton;  sk->setRows(4);  sk->startPulse();   // L1 首次
auto *ld = new PanshiLocalLoading(tr("正在读取存储统计…"));  ld->start();  // L2 局部
auto *pb = new PanshiFlatProgress;  pb->setBusy(true);                 // L3 长任务
```

语义只允许 6 档：`Muted / Normal / Accent / Success / Warning / Danger`，全部由 `semanticColor()` 从 `DPalette` 取值，组件自身不出现颜色常量。

## 4. 静态自检（已在无构建环境下逐条核对）

- [x] `src/` 下已无 `gotoPage(<数字>)` 与 `m_pages.at(<数字>)`（逐个文件通读确认，调用点仅 `mainwindow.cpp` / `overviewpage.cpp` / `snapshotspage.cpp`）
- [x] 新增文件不含任何 hex 色值（`#RRGGBB` 命中数 = 0）
- [x] 新增文件不含 `setStyleSheet()`
- [x] 新增类全部**不使用 Q_OBJECT**，无需 AUTOMOC 改动依赖
- [x] `CMakeLists.txt` 已登记两个新源文件
- [x] 现有页面未做任何改动，`style.h` / `common.h` 保持原样 → 现有行为零回归

## 5. 需在有 DTK 构建环境一侧验证的项（重点）

本机未安装 DTK6 头文件，以下 3 类 API 无法离线核对，若编译报错请优先检查：

1. **`DPalette` 颜色的取值方式**
   `appPalette().color(DPalette::TextTips / TextTitle / TextWarning / TextLively / Base / Window / ItemBackground / FrameBorder)`
   —— 若 `DPalette::color(ColorType)` 不可用，改为 `DGuiApplicationHelper::instance()->applicationPalette().color(...)` 或回落到 `QPalette` 标准角色。
2. **`DFontSizeManager::SizeType` 名称**
   —— `sizeTypeFor()` 的返回类型；若名称不同，直接改为 `auto` 返回值。
3. **`DProgressBar` 构造与 `setBusy`**
   —— `PanshiFlatProgress` 依赖 `setRange(0,0)` 的不确定进度；若 DProgressBar 不支持，回落到 `QProgressBar`（临时），后续再替换。

## 6. 下一批（阶段 1 剩余，未在本轮执行）

按方案 §5.1，剩余部分必须**一次性做完**（QSS 优先级高于 DPalette，分批会产生"一半跟主题一半不跟"的中间态）：

1. 删空 `style.h` 的 hex 色板 → 全部改由 `DPalette` + 新组件承担（同步改 `main.cpp` 的样式装载）
2. 删除 `common.h` 中 15 个内嵌 SVG 与 `IconBadge`（含 `s_instances` 静态表），全量改 `DIconTheme::findQIcon()`
3. 品牌标识：渐变方块 → `data/icons/panshi.svg`
4. 合并 5 份重复的 `confirmDialog / infoDialog / errorDialog` → `PanshiUi::askConfirm` + `PanshiInlineBanner`
5. `UsageChart` / `ScanOverlay` / `RadarWidget` / `CheckOverlay` / `CountdownRing` 的自绘配色改从 palette 取，并同时监听 `applicationPaletteChanged`

**验收硬指标**：`grep -rE '#[0-9A-Fa-f]{6}' src/` 命中数 = 0。
