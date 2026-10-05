# 侧栏图标重设计 + 亮暗主题切换修复

> 日期：2026-10-03
> 输入：用户反馈「亮暗色切换有问题」「侧栏图标亮暗都不明显，请重设计：彩色 + 大一点」
> 状态：**代码完成，待编译 + 复测**

---

## 一、用户观察到的三个症状

| # | 症状 |
|---|---|
| 1 | 切换主题时**有些文字会无法显示** |
| 2 | **图标也没法看见** |
| 3 | 切暗色时**有时不会变** |

> 关键信息：症状 1、3 都不是「配色难看」，而是**功能性的失效**，
> 根因在主题切换机制本身，不在视觉方案。

---

## 二、三个根因（逐一核实代码后定位）

### 根因 1｜图标是**快照**，主题切换后不重查 ⭐ 主因

`setPixmap(icon.pixmap(...))` / `QStandardItem(icon, ...)` 里的
**pixmap 与 QIcon 都是构造那一刻的快照**。QIcon 后续怎么换主题，
都不会让已经画进 label / 模型的图标变化。

DTK 官方文档「主题切换时的图标更新」明确要求**手动重查**。

本项目有**三处**踩了这个坑（同一个病因，三个位置）：

| 位置 | 原写法 | 症状 |
|---|---|---|
| `Ui::iconBadge()`（`pages/common.h`） | `setPixmap(themedIcon(k).pixmap(isz))` | 页面内所有大图标切暗后仍是亮色版 → **浅色图标在深底上看不见** |
| `PanshiEmptyState`（`components.cpp`） | `refreshIcon()` 里 `setPixmap` | 空状态图标不跟随 |
| 侧栏导航（`sidebar.cpp`） | `QStandardItem(navIcon(...), text)` | **侧栏图标不跟随主题**（用户说的「图标也没法看见」） |

### 根因 2｜自绘组件收不到重绘通知 → 「有时不变」

侧栏与所有自绘卡片（`PanshiPanel` / `PanshiStateCard` / `PanshiBadge` /
`PanshiInlineBanner`）都在 `paintEvent` 里**实时**取 `appPalette()` ——
取色逻辑本身是对的。但：

- `QListView` 在调色板替换后未必主动重绘**全部行** → 部分行变了、部分没变
  → 这正是「**有时**不会变」的形态；
- **隐藏页**（`QStackedWidget` 里未激活的）在 `w->update()` 时不会被标脏
  → 下次切到该页仍是旧主题像素。

### 根因 3｜信号时序不一致 ⭐ 最隐蔽

项目里**两套主题信号都在用，但时序不同**：

| 信号 | 发出时机 | 后果 |
|---|---|---|
| `themeTypeChanged` | DTK 调色板替换**尚未完成** | 此时重绘/重建会得到「半新半旧」（main.cpp 已有详细注释记录这个坑） |
| `applicationPaletteChanged` | 新调色板**就位后** | 此刻重绘才安全 |

`main.cpp` 的重灌挂在后者（正确），但 `DiskUsagePage::onThemeChanged`
挂在**前者** → **两者不一致**，用户看到的「有时不会变」就是这条错配。

---

## 三、改动清单

### 3.1 图标改为彩色 + 20px（侧栏）

| 项 | 之前 | 现在 |
|---|---|---|
| 图标来源 | 只按名字取 → 落到**单色 symbolic** | **彩色优先**（`-colored` / `_colored` / 原名 三级试） |
| 尺寸 | 16px（`Size::IconSmall`） | **20px**（`Size::Icon`） |
| 行高 | 40 | 44（容得下 20px + 内边距） |
| 分组行高 | 32 / 44 | 32 / 46 |
| 文字左缩进 | 按 `IconSmall` 算 | 按 `Icon` 算（否则文字偏左） |
| 选中态图标 | `QIcon::Selected` | **恒用 `QIcon::Normal`** |

**为什么选中态不能用 `QIcon::Selected`**：它会对图标做状态替换，
部分主题下**直接把彩色图标压成单色** —— 恰好毁掉我们要的彩色锚点。
选中态改由「柔和底 + 文字高亮」表达（`softFill(Accent)` + `semanticColor(Accent)`），
**图标本身不动**。这反而让选中态更稳定。

> 用户反馈「图标亮暗都不明显」的根因就是单色 symbolic：
> 浅色底上是深灰、深色底上是浅灰，**两边都缺色彩锚点**。
> 彩色版在亮/暗底上都有稳定色相。

### 3.2 三处图标都挂主题重查

```cpp
// 侧栏：重查并写回模型（需存原名 → 新增两个角色）
constexpr int kNavIconRole     = Qt::UserRole + 21;  // QString：首选图标名
constexpr int kNavFallbackRole = Qt::UserRole + 22;  // QString：备选图标名
```
- 侧栏建图标时把原名存进角色，主题切换时重查 `setIcon()`
- `Ui::iconBadge()` 挂 `applicationPaletteChanged` → 重设 pixmap
- `PanshiEmptyState` 构造函数挂同一信号 → `refreshIcon()`

> ⚠️ 角色号继续遵守**互不重叠**原则：`+21/+22` 与既有 `+1/+2/+11/+12/+13` 不撞车
> （这是阶段 4 踩过的坑，角色撞车会静默渲染错行）。

### 3.3 重绘链路补全

| 位置 | 改动 |
|---|---|
| `DiskUsagePage` | 新增 `onPaletteChanged()` 挂 `applicationPaletteChanged`；`onThemeChanged()` 加 `update()` |
| `main.cpp` | 顶层 `w->update()` → **深度递归遍历所有子控件 `update()`** |

**递归刷新为什么必要**：隐藏页的自绘卡片不会被顶层 `update()` 标脏，
而它们的取色逻辑是对的 —— **问题只是没人叫它们重画**。

### 3.4 补 include

| 文件 | 补 |
|---|---|
| `sidebar.cpp` | `DGuiApplicationHelper`、`QStandardItemModel` |
| `components.cpp` | `DGuiApplicationHelper` + `DGUI_USE_NAMESPACE` |
| `main.cpp` | `QList`、`QWidget` |

---

## 四、待复测清单

### 编译
- [ ] **T-1** 0 error / 0 warning
- [ ] **T-2** 侧栏行高 44 后，7 个导航项 + 3 个分组在 700px 高窗口内**不溢出**（无滚动条）

### 侧栏图标
- [ ] **T-3** 图标**明显变大**（20px），文字未被挤压、左对齐正常
- [ ] **T-4** 图标是**彩色**（若主题无彩色版则回退单色，属预期 —— 需确认回退后的观感是否可接受）
- [ ] **T-5** 选中态：柔和底 + 文字变 Accent 色，**图标仍保持彩色**（不被压成单色）

### 亮暗切换（核心）
- [ ] **T-6** 亮→暗：**所有文字**变浅色，**无任何文字消失**（症状 1）
- [ ] **T-7** 亮→暗：侧栏 7 个图标**全部更新**（症状 2）
- [ ] **T-8** 亮→暗：**逐页切换检查**（总览/磁盘/备份/保护/策略/更新/设置/关于）—— 尤其**从别的页切入**的路径（症状 3 的触发场景）
- [ ] **T-9** 暗→亮：反向同样全项通过
- [ ] **T-10** 连续切换 3 次（亮→暗→亮→暗），确认**不残留上一次的半新半旧状态**
- [ ] **T-11** 页面内大图标（`iconBadge`）与空状态图标跟随切换

### 边界
- [ ] **T-12** 主题色跟随：控制中心改主题色 → 侧栏选中态与搜索命中高亮跟随（Accent 走 `Highlight`）
- [ ] **T-13** 窄窗 922 下侧栏 200px 未被图标变大会挤压文字

---

## 五、遗留

| 项 | 说明 |
|---|---|
| **彩色图标名** | 本轮用「`-colored` / `_colored` / 原名」三级试探。若 deepin 图标库的彩色命名不是这几种，**T-4 会看到仍是单色** → 需按实际命名补 `mainwindow.cpp` 里的图标名 |
| 其它页面的 `iconBadge` | 已统一修好（`common.h` 是单一入口），但若有页面**绕过** `Ui::iconBadge` 直接 `setPixmap` 就会漏 —— 复测时留意 |
| V-30 暗色复核 | 仍需控制中心**手动切暗色**；本轮 T-6~T-9 会顺带覆盖大部分场景 |

---

## 六、编译验证（2026-10-03 23:10）

**T-1 通过：0 error / 0 warning ✅**，产物 `build2/panshi-manager`（29MB），
离屏启动真实跑通（`QT_QPA_PLATFORM=offscreen`）。

> ⚠️ **期间修掉一个真实缺陷**：`src/main.cpp` 的 `qMakeUnique<QList<QWidget*>>()`
> 是 **Qt5 API，Qt6 已移除** —— 属本轮 19:01 新增的「深度递归 update()」代码，
> 因当轮无编译环境而一直潜伏。已改为 `QList<QWidget*> stack;`（值语义）。
> → **教训：新增代码当轮不编译，缺陷会一直潜伏到下次有人编译。**

### 本项目构建方式（只读容器专用）

本机 `/etc` 与 `/usr` 不可写、`apt` 无 dpkg 锁、`sudo` 失效，但可联网 →
已用 `dpkg-deb -x` 自建用户态工具链到 `.toolchain/sysroot`。

```bash
cd /media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置
. .toolchain/env.sh          # ⚠️ 必须先 source
cmake -B build2 -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=$SR/usr/bin/g++-12 -DCMAKE_C_COMPILER=$SR/usr/bin/gcc-12 \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/gmake -DOpenGL_GL_PREFERENCE=LEGACY \
  -DCUPS_INCLUDE_DIR=$SR/usr/include -DCUPS_LIBRARIES=$SR/usr/lib/x86_64-linux-gnu/libcups.so
cmake --build build2 -j$(nproc)
```

| 注意 | 说明 |
|---|---|
| 必须 `source .toolchain/env.sh` | PATH / LD_LIBRARY_PATH / CMAKE_PREFIX_PATH / TMPDIR 全在里面 |
| 构建目录用 `build2` | `rm -rf build` 整目录被安全层拦截（单文件 `rm -f` 可以） |
| `/tmp` 仅 10MB tmpfs | `env.sh` 已设 `TMPDIR` 指向项目内 `.tmp`，否则必报「设备上没有空间」 |
| 编译器必须显式指定 | 旧 `CMakeCache.txt` 记着已消失的 linglong 路径，不覆盖会 configure 失败 |
| 依赖包可参考 | `.toolchain/env.sh` 与 `docs/开发文档.md`；方法见用户级 skill `readonly-container-toolchain` |

### 离屏启动的预期输出（非缺陷）

```
dtk.dsg.config: DConfig is invalid of appid= name=org.deepin.dtk.preference
"panshi-manager" can not find qm files for locales QList("zh_CN")
```
前者：该版 DTK 硬编码只认只读 `/usr/share/dsg`（平台约束，QSettings 兜底，GUI 功能正常）。
后者：`.qm` 在 `build2/` 根目录，**不在 `loadTranslator()` 查找列表** →
验证 i18n 须装到 `~/.local/share/panshi-manager/translations/`（见 `开发文档.md` §9.1b）。

### 仍待实机

T-2 ~ T-13：亮暗往返切换（T-6~T-10 需**控制中心手动切暗色**）、彩色图标确认（T-4）、
窄窗 922 下侧栏（T-13）。→ 见 §四 清单。
