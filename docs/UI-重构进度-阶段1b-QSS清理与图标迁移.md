# UI 重构进度 — 阶段 1-b（QSS 清理 · 图标迁移 · 对话框合并）

> 前置：阶段 0 + 1-a 已通过编译与 8 秒冒烟测试（见《UI-重构进度-阶段0与阶段1a.md》）。
> 本轮完成**阶段 1 剩余全部内容**。按方案要求，QSS 颜色清理为**一次性完成**，
> 不存在"一半跟主题一半不跟"的中间态。

## 一、本轮改动清单

### 1. `src/style.h` — QSS 颜色全部改为 DPalette 运行时取值（重写）
- **源码中不再存在任何 hex 色值**。所有颜色运行时取自
  `DGuiApplicationHelper::applicationPalette()`：
  | QSS 变量 | 来源 |
  |---|---|
  | bg | `DPalette::Window` |
  | sidebar / card | `DPalette::Base` |
  | border | `DPalette::FrameBorder` |
  | hoverBg | `DPalette::ItemBackground` |
  | textSub | `DPalette::TextTips` |
  | accent / success / purple | `DPalette::Highlight`（语义收敛，跟随用户主题色） |
  | danger | `DPalette::TextLively` |
  | warning / warnText | `DPalette::TextWarning` |
  | 各 Soft 柔和底 | 语义色 15% 透明度叠加 Base（`panshiSoftFill()`） |
- **删除两处 Web 风格渐变 Logo QSS**（`#panshiLogo`、`#panshiAboutLogo` 的
  `qlineargradient #0081FF→#00A870`）。
- QSS 现在只承担布局职责（间距/圆角/字重/选择器结构），颜色职责全部归 DPalette。
- 主题切换时由 `main.cpp` 的 `themeTypeChanged` 回调重新生成整表（已有钩子）。

### 2. `src/pages/common.h` — 图标迁移 + 统一对话框（重写）
- **删除 15 个内嵌 SVG 字符串、`iconMap()`、`badgeIconColor/badgeBackgroundColor()`、
  `IconBadge` 自绘类**（含 `QSvgRenderer` 依赖）。
- `Ui::iconBadge(name, size)` 改为主题图标 `DLabel`：走 `DIconTheme::findQIcon()`，
  缺失时回落 `QStyle` 标准图标；新增 `Ui::themedIcon()` 助手。
- `Ui::legendDot()` 从"setStyleSheet 动态色"改为自绘 `LegendDot` 小部件，
  **存 `DPalette::ColorType`、绘制时实时取调色板**——主题切换后图例色自动跟随。
- **新增全应用唯一的三个对话框助手**（原 6 份重复实现合并于此）：
  - `Ui::infoDialog(parent, title, text)`
  - `Ui::errorDialog(parent, text)`
  - `Ui::confirmDialog(parent, title, text, okText = "", danger = false)`
    （danger=true 时：警告图标 + ButtonWarning 确认键）

### 3. 对话框去重（6 个文件）
| 文件 | 改动 |
|---|---|
| `masterswitch.cpp` | 删本地 info/error，调用改 `Ui::` 前缀 |
| `protectionpage.cpp` | 删本地 3 份，改前缀；确认语义不变 |
| `updatepage.cpp` | 删本地 3 份，改前缀 |
| `policypage.cpp` | 删本地 2 份（`configPath()` 保留），改前缀 |
| `deployspage.cpp` | 删本地 3 份，改前缀；**删除部署确认显式传 `tr("删除"), true`**（保持危险语义） |
| `snapshotspage.cpp` | 删本地 3 份，改前缀（8 处调用） |

### 4. 品牌标识去渐变化
- `mainwindow.cpp` 侧栏 Logo、`aboutpage.cpp` 关于页 Logo：
  「磐」字渐变色块 → `DIconTheme::findQIcon("panshi")` 应用图标（36/56px）。

### 5. 自绘控件取色归 DPalette
| 文件 | 改动 |
|---|---|
| `widgets/usagechart.cpp` | accent→`Highlight`、danger→`TextLively`、warn→`TextWarning`、textSub→`TextTips`、网格→`FrameBorder`(带透明度)、空数据占位→`ItemBackground`、选中点描边→`Base`；主题切换由 `DiskUsagePage::onThemeChanged` 触发重绘（已有） |
| `widgets/radarwidget.cpp` | `accentColor()` → `applicationPalette().color(Highlight)` |
| `widgets/scanoverlay.cpp` | 删未使用的 `accentColor()`；面板底→`ItemBackground`(半透明) |
| `widgets/checkoverlay.cpp` | 面板底→`ItemBackground`(半透明) |
| `widgets/countdowndialog.cpp` | 倒计时危险弧 `#E64545` → `DPalette::TextLively` |
| `pages/diskusagepage.cpp` | 图例三色 → `Highlight / TextLively / TextWarning`（自动跟随主题） |

### 6. `src/ui/components.cpp/.h` — 语义映射修正
对齐方案 §2.3 的 4 类收敛（并修正 1-a 遗留的映射偏差）：
- `Accent/Success` → `DPalette::Highlight`（**原误映射到 TextLively 红色**，本轮纠正）
- `Warning` → `TextWarning`；`Danger` → `TextLively`
- `Muted` → `TextTips`；`Normal` → `TextTitle`

### 7. `src/main.cpp`
- 移除两处 `Ui::IconBadge::refreshAll()`（类已删除）。

## 二、静态自检项（本机 Bash/Grep 仍 251 失效，无法编译/grep 验证）
1. ✅ 全源码通读复查：`style.h / common.h / usagechart / scanoverlay / checkoverlay /
   radarwidget / countdowndialog / diskusagepage` 中已无 `#RRGGBB` 字面量
   （`#panshiMask` 的 `rgba(0,0,0,0.05)` 为 alpha 黑，保留）。
2. ✅ 6 个页面的 `Ui::` 前缀逐一核对，无遗漏的无 qualify 调用。
3. ✅ `policypage.cpp` 的 `configPath()` 在误删后已恢复。
4. ⚠ 建议编译后执行验收硬指标：`grep -rE '#[0-9A-Fa-f]{6}' src/` 应命中 0。

## 三、需要编译侧重点验证
1. `common.h` 的 `Ui::confirmDialog` 默认参数在 6 个调用点均为合法调用（含 3/4/5 参）。
2. `panshiColor(DPalette::ColorType)` 在 style.h 中为 inline 全局函数，确认无 ODR 冲突
   （仅 main.cpp 包含该头，风险低）。
3. `DIconTheme::findQIcon("panshi")` 需确认应用图标在开发环境可见（未 `install.sh` 时
   data/icons 未安装到图标路径，可能回落为空 pixmap——仅影响两处 Logo 显示）。

## 四、遗留视觉语义说明（预期行为，非遗漏）
- 统计卡的 green/purple 现在与 blue 同为 Highlight（语义收敛，方案 §2.3）；
  阶段 3 逐页重构时由 `PanshiStateCard` 取代，将只保留 4 类语义。
- `masks`、表格、进度条等 QSS 保留至阶段 3 被新组件逐页替换。

## 五、下一步
- **阶段 2 外壳导航**：`DListView + DStyledItemDelegate` 侧边栏、导航分组、
  页头唯一化（消除 VersionManagementPage/SnapshotsPage 双页头）、修复导航键盘焦点。
- 随后阶段 3 逐页重构（关于→更新→策略→保护→总览→备份→磁盘）。
