# 磐石设置 (Panshi Manager) — DTK 版本

基于 **C++ / Qt6 / DTK6** 重写的 Deepin 25 磐石系统管理工具。
原项目为 PyQt6 实现，本版本使用 DTK（Deepin Tool Kit）原生控件重构，
完美融入 DDE 桌面环境，支持亮/暗主题自动切换与 X11 / Wayland。

## 与原版 (PyQt6) 的差异

| 方面 | PyQt6 原版 | DTK 重写版 |
|------|-----------|-----------|
| 技术栈 | Python 3.12 + PyQt6 + QSS | C++17 + Qt6 + DTK6 |
| 应用入口 | QApplication | DApplication（单实例/翻译/日志） |
| 主窗口 | QMainWindow + 自绘侧边栏 | DMainWindow + DTitlebar + 分组导航侧栏（自绘 `QListView` delegate）+ 设置搜索 |
| 按钮 | QPushButton + QSS 模拟 | DPushButton / DSuggestButton / DWarningButton |
| 开关 | 自绘 Switch 控件 | DSwitchButton（原生 DTK 开关） |
| 对话框 | QMessageBox | DDialog（DTK 原生对话框） |
| 输入控件 | QLineEdit / QTextEdit / QSpinBox / QComboBox | DLineEdit / DTextEdit / DSpinBox / DComboBox |
| 卡片 | QFrame + QSS | DFrame + DStyle + 主题图标徽章 |
| 主题 | 仅浅色 QSS | **默认跟随系统**（DTK themeType=0，不强制任何主题，尊重系统设置中的单独配置）；颜色职责全部交还 `DPalette`，QSS 只管布局 |
| 图标 | 内嵌 SVG 字符串 | **`DIconTheme::findQIcon()` 系统主题图标**（自动 symbolic 回退，随系统图标主题切换）；原自绘 SVG 徽章已删除 |
| 字体 | QSS 硬编码字号 | **DFontSizeManager 分级管理**（T2~T8），响应系统字体缩放；移除全部硬编码 font-size |
| 输入控件 | 固定宽度 | **自适应宽度**（minimumWidth + sizeHint），DSpinBox/DComboBox 内容不再截断 |
| 布局 | 固定网格 | 弹性布局防截断：状态卡值独占整行（垂直布局）、信息卡 2×2 弹性网格、长文本统一 wordWrap（DLabel）、**所有页面内容置于滚动容器（QScrollArea），窗口尺寸不足时自动出现滚动条，内容永不裁剪**；卡片行按可用宽度自动折列（`PanshiResponsiveCardRow`） |
| 加载态 | 全页遮罩 + DSpinner | **三档分级**：L1 骨架屏 / L2 局部加载 / L3 顶部细进度条（不再盖整页） |
| 反馈 | 模态 info/error 对话框 | **页内 `PanshiInlineBanner`**（成功自动消失 / 失败常驻可关），成功类操作 0 次模态弹窗 |
| 导航 | 7 个按钮平铺 | **分组 + 图标 + 键盘可达 + 设置搜索**（侧栏顶部搜索框，输入即定位并高亮目标行） |
| 跨页入口 | 只跳页，用户到目标页再找按钮 | **跳页 + 到达后动作**（总览「立即创建备份」直达并开框、「检查更新」自动触发；动作等目标页 refresh 完成后执行） |
| 控件层 | 混用 Qt 原生控件 | **DTK 控件全覆盖**：`DProgressBar` / `DDoubleSpinBox` / `DListView` / `DPushButton`（`QTableWidget` 因 DTK 无等价表格而有意保留） |
| 后台任务 | threading.Thread | QtConcurrent + QFutureWatcher |
| 命令执行 | subprocess | QProcess（sudo -n / pkexec 回退） |
| 国际化 | 硬编码中文 | **`tr()` + Qt Linguist**（`translations/*.ts` → CMake `qt_add_translations` 生成并安装 `.qm`，`DApplication::loadTranslator()` 装载）；`.desktop` 带 `[en_US]` 变体 |

## 功能页面

页面标识与导航顺序（`src/mainwindow.h` 的 `enum class PageId`）：

| 页面 | 功能 | 图标 |
|---|---|---|
| 总览 | 磐石保护状态、可写层、快照统计、仓库占用、系统信息、快速操作 | 状态卡：security/edit-rename/appointment-soon/drive-harddisk |
| 磁盘空间 | **持续监控系统盘空间**：时间线展示每日占用变化（近 7 天 ~ 一年）、异常增长定位到文件夹/文件、容量预测、告警通知、排除目录、存储预算控制（后台 systemd user 服务 + DConfig 配置） | drive-harddisk / utilities-system-monitor |
| 系统保护 | 只读保护状态、永久/临时开启/关闭可写层 | 状态卡：security/edit-rename/view-refresh |
| 备份与版本 | **备份快照 + 系统版本(部署) 合并**（DTabBar 标签切换）：快照列表/创建/删除/还原(含回滚前备份策略)、可启动系统版本列表/删除旧版本 | document-save |
| 系统更新 | 检查更新、原子升级、清理缓存 | software-updates / dialog-information / system-run |
| 策略与设置 | 备份保留策略（应用级·即时保存）+ 磐石系统参数（系统级·显式保存） | preferences-system / document-save |
| 关于 | 应用信息、技术说明、常用命令 | help-about / utilities-terminal |

> 侧栏按「概览 / 系统维护 / 设置」三组呈现，顶部带设置搜索框。
> 主题：默认跟随系统（DTK themeType=0），亮/暗自动切换；所有颜色使用 DPalette 角色管理，
> 图标使用 DIconTheme 系统主题图标（自动 symbolic 回退），随系统图标主题切换。

## UI 设计体系

重构后应用层只表达**结构与语义**，视觉控制权交还 DTK：

- **颜色**：一律经 `DPalette` / `DGuiApplicationHelper::applicationPalette()` 运行时取值。
  语义收敛为 `PanshiUi::Semantic`（Muted / Normal / Accent / Success / Warning / Danger），
  禁止 hex 色值、禁止 QSS 属性选择器（`[stat=...]`）染色。`style.h` 仅保留承担布局职责的 QSS
  （阶段 3-7b 后只剩 5 个结构性颜色，**无任何语义色插值**）。
- **度量**：`src/ui/tokens.h`（间距 4/8/12/16/24/32、圆角 Element4/Frame8、图标 16/20/24、动效时长），不含颜色。
- **组件层**（`src/ui/components.h`）：`PanshiPanel` / `PanshiSection` / `PanshiRow` /
  `PanshiBadge` / `PanshiInlineBanner` / `PanshiStateCard` / `PanshiResponsiveCardRow` /
  `PanshiEmptyState` / 三档加载 / `PanshiSeparator` / `PanshiCollapsible`。
- **确认流程**：全应用唯一实现 `PanshiUi::ConfirmRequest` + `askConfirm` / `askDanger`
  （支持后果清单、单选选项、倒计时）。
- **响应式**：卡片行按可用宽度自动折列；表格窄窗自兜横向滚动；文本用 `Ui::ElidedLabel` 中部省略 + tooltip。
- **跨页跳转**：`gotoPage(id, afterArrive)` —— 跳页并把动作挂到目标页 `idleReached()`
  （基类统一计数，refresh 完成才执行），带 4s 超时兜底、后到覆盖先到、切页/退出作废。
  页面侧以 `Q_INVOKABLE` 暴露动作契约（`requestCreateSnapshot` / `requestCheckUpdate` /
  `highlightEnableProtect`），总览页无需持有目标页头文件。
- **控件层**：优先 DTK 控件（自动跟随 deepin 主题）。`DProgressBar` / `DDoubleSpinBox` /
  `DListView` 均**继承**对应 Qt 原生控件，替换时行为等价；`DListView` 不自带模型，
  需配 `QStringListModel`。`QCalendarWidget` 无 DTK 替代，但用「单日历两次点击定区间」
  收敛交互（摘要以状态为唯一数据源，消除高亮/文字分叉）。

详细设计与逐阶段进度见 `docs/UI-重构设计方案.md` 与同目录 `docs/UI-重构进度-阶段*.md`。

> **准备改代码？先看 [`docs/开发指南.md`](docs/开发指南.md)** ——
> 含代码地图（每个文件的职责）、10 个任务的扩展开发模板（新增页面/设置项/组件/后端命令/
> 磁盘指标/DConfig 键/图标/语言/探针/跨页动作）、UI 编码红线、四层验证体系，
> 以及 **30 条按成因分组的经验教训**（含多条已造成返工的坑）。
> 字段级精确参考见 [`docs/开发文档.md`](docs/开发文档.md)。

## 构建与安装

```bash
# 依赖 (Deepin 25)
sudo apt install -y cmake g++ qt6-base-dev libdtk6core-dev libdtk6gui-dev libdtk6widget-dev
# 可选：翻译工具（lupdate / lrelease），仅在需要更新 .ts 时安装
sudo apt install -y qt6-tools-dev-tools

# 构建（含翻译 .qm 生成）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 安装到 /usr
sudo cmake --install build

# 或一键安装
sudo ./install.sh
```

启动：启动器搜索「磐石设置」或运行 `panshi-manager`。

## 目录结构

```
panshi-settings-dtk/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # DApplication 入口（--monitor / --show-disk）
│   ├── mainwindow.h/cpp      # 主窗口 + 侧边栏导航 + PageId 跳转 + 设置搜索接线
│   │                         #   + 跨页「到达后动作」队列
│   ├── backend.h/cpp         # 磐石命令封装 (QProcess)
│   ├── style.h               # 布局类 QSS（仅 5 个结构性颜色；颜色职责归 DPalette）
│   ├── ui/                   # UI 组件层（阶段 0 新增）
│   │   ├── tokens.h          # 度量 token（间距/圆角/尺寸/动效，不含颜色）
│   │   ├── components.h/cpp  # 通用组件（Panel/Section/Row/Badge/Banner/StateCard/…）
│   │   ├── confirmdialog.h/cpp # 全应用唯一确认组件
│   │   ├── sidebar.h/cpp     # 分组导航侧栏 + 设置搜索框
│   │   └── settingindex.h/cpp# 设置搜索索引
│   ├── pages/
│   │   ├── basepage.h/cpp    # 页面基类（三档加载 + 注册点：宽度/反馈/搜索定位）
│   │   ├── common.h          # 薄封装：makeScrollArea / pageHeader / ElidedLabel / 图标
│   │   ├── overviewpage.*    # 总览
│   │   ├── protectionpage.*  # 系统保护
│   │   ├── snapshotspage.*   # 备份快照
│   │   ├── versionpage.*     # 备份与版本（tab 外壳）
│   │   ├── deployspage.*     # 系统版本（部署）
│   │   ├── updatepage.*      # 系统更新
│   │   ├── policypage.*      # 策略与设置
│   │   ├── diskusagepage.*   # 磁盘空间
│   │   ├── aboutpage.*       # 关于
│   │   └── masterswitch.*    # 「磐石功能」总开关共享逻辑
│   ├── widgets/              # 自绘控件（时间线图表 / 扫描·检查·雷达覆盖层 / 倒计时）
│   └── disk/                 # 磁盘监控子系统（GUI 与 --monitor 进程共用）
├── data/icons/panshi.svg     # 应用图标
├── translations/             # 翻译源文件（i18n：zh_CN 恒等 + en_US）
│   ├── panshi-manager_zh_CN.ts
│   └── panshi-manager_en_US.ts
├── org.deepin.panshi-manager.desktop
└── install.sh
```

## 性能优化

- **启动秒开**：总览页分步加载（先读基本状态→再后台统计空间），窗口立即显示可交互
- **OSTree 树缓存**：`ostree ls` 结果缓存到 `~/.cache/panshi-manager/tree_*.json`（与 Python 原版一致），二次启动直接读缓存，不再遍历系统树
- **内存缓存**：快照大小 60 秒 TTL、du 结果 30 秒 TTL，页面切换/刷新不重复计算

## 技术说明

- 封装 `deepin-immutable-ctl`（快照/部署/升级）与 `deepin-immutable-writable`（只读保护）
- 快照占用空间 = 数据目录物理大小 + 独占 OSTree 对象大小（跨快照去重）
- 还原使用官方 `snapshot rollback`，重启后生效；可写层开启时拒绝还原
- 回滚前自动创建「回滚前备份」并记录状态，可选重启 3 次后自动删除（systemd timer）
- 需要 root 权限的操作自动走 `sudo -n`，失败回退 `pkexec`

## 开发运行（无需安装）

```bash
QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager
```
