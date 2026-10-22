# 磐石设置 (Panshi Manager) 完整开发文档

## 1. 项目概述

### 定位与目标

磐石设置（Panshi Manager）是 Deepin 25 磐石系统（OSTree 只读根 + 可写层）的图形化管理工具[1]。它面向维护该系统根文件系统只读保护机制、备份快照、系统更新等核心能力的日常管理场景，是系统管理中这些底层操作的可视化入口。

本项目是原 PyQt6 实现的 **C++ / Qt6 / DTK6 原生重写版**，目标不仅是功能等价迁移，更是将应用从"自绘控件 + QSS 模拟"的独立视觉体系迁移到 DTK 原生控件体系之上，从而获得三个关键收益[1][3]：

- **主题一致性**：默认跟随系统主题（DTK themeType=0），亮/暗自动切换，不再强制任一主题，尊重系统设置中的单独配置[1]；
- **控件一致性**：按钮、开关、对话框、输入控件、进度条等全部使用 DTK 控件或 DTK 控件的继承类，自动跟随 deepin 主题的圆角、动效与配色规范[1]；
- **生态一致性**：图标使用 `DIconTheme` 系统主题图标（自动 symbolic 回退），字体使用 `DFontSizeManager` 分级管理，随系统图标主题与字体缩放联动[1]。

从工程角度，重写的另一个目的是收敛维护成本：原 PyQt6 版本依赖 Python 运行时与 QSS 自绘体系，控件行为与主题适配需要额外维护；DTK 版本将这些职责交还平台层，应用层只表达结构与语义[1]。

代码库实际版本为 1.0.0（`CMakeLists.txt` 的 `project()` 声明与两份开发文档均一致）[2][3][4]。截至 2026-10 的打包工作已将 deb 包推进至 1.0.4（详见 `debian/changelog`），后续小版本迭代集中在缺陷修复与打包规范适配[4]。

### 功能一览

应用由七个页面组成，页面标识与导航顺序由 `src/mainwindow.h` 的 `enum class PageId` 强类型定义[3]。侧栏按「概览 / 系统维护 / 设置」三组呈现，顶部带**设置搜索框**（输入即列出匹配的设置项，回车/点击跳转并高亮目标行）[1][3]。

| 页面 | 功能 |
|---|---|
| 总览 | 磐石保护状态、可写层、快照统计、仓库占用、系统信息、快速操作 |
| 磁盘空间 | **持续监控系统盘空间**：时间线展示每日占用变化（近 7 天 ~ 一年）、异常增长定位到文件夹/文件、容量预测、告警通知、排除目录、存储预算控制（后台 systemd user 服务） |
| 系统保护 | 只读保护状态、永久/临时开启/关闭可写层 |
| 备份与版本 | 备份快照 + 系统版本（部署）合并页：快照列表/创建/删除/还原（含回滚前备份策略）、可启动系统版本列表/删除旧版本 |
| 系统更新 | 检查更新、原子升级、清理缓存 |
| 策略与设置 | 备份保留策略（应用级·即时保存）+ 磐石系统参数（系统级·显式保存） |
| 关于 | 应用信息、技术说明、常用命令 |

除七个 GUI 页面外，项目还包含一个**后台监控进程**（`--monitor` 无界面模式），由 systemd 用户 timer 每 15 分钟拉起，执行磁盘占用采集与异常增长判定[1][3][4]。

### 技术栈

- **语言/标准**：C++17（`CMAKE_CXX_STANDARD 17`，强制开启）[2]
- **Qt 6**：Core / Gui / Widgets / Concurrent / Svg / DBus[1][3]
- **DTK6**：Dtk6Core / Dtk6Gui / Dtk6Widget（CMake 侧为 `Dtk6::Core`、`Dtk6::Gui`、`Dtk6::Widget`）[2]
- **持久化**：DConfig（系统配置）、QSettings（降级兜底）[1][3]
- **后台调度**：systemd 用户级 timer + service（`Type=oneshot`，nice/ionice 低优先级）[3]
- **进程间通信**：D-Bus（单实例定位转发、系统通知）[1][3]
- **后台命令**：`df` / `du` / `ostree` / `deepin-immutable-ctl` / `deepin-immutable-writable` / `systemctl` / `gio`[1][3]
- **特权执行**：`sudo -n` 自动尝试，失败回退 `pkexec`[1]
- **异步任务**：QtConcurrent + QFutureWatcher（替代 PyQt6 版的 `threading.Thread`）[1]
- **构建工具**：CMake（minimum 3.16）+ AUTOMOC[2]
- **国际化**：`tr()` + Qt Linguist（`translations/*.ts` → CMake `qt_add_translations()` 生成并安装 `.qm`）[1][2]

### 与原版 PyQt6 的差异

重写版在以下维度与原版存在系统性差异[1]：

| 方面 | PyQt6 原版 | DTK 重写版 |
|---|---|---|
| 技术栈 | Python 3.12 + PyQt6 + QSS | C++17 + Qt6 + DTK6 |
| 应用入口 | QApplication | DApplication（单实例/翻译/日志） |
| 主窗口 | QMainWindow + 自绘侧边栏 | DMainWindow + DTitlebar + 分组导航侧栏（自绘 `QListView` delegate）+ 设置搜索 |
| 主题 | 仅浅色 QSS | **默认跟随系统**（DTK themeType=0）；颜色职责全部交还 `DPalette`，QSS 只管布局 |
| 图标 | 内嵌 SVG 字符串 | **`DIconTheme::findQIcon()` 系统主题图标**（自动 symbolic 回退） |
| 字体 | QSS 硬编码字号 | **DFontSizeManager 分级管理**（T2~T8），响应系统字体缩放 |
| 布局 | 固定网格/固定宽度 | 弹性布局防截断：所有页面内容置于滚动容器（QScrollArea），卡片行按可用宽度自动折列（`PanshiResponsiveCardRow`） |
| 加载态 | 全页遮罩 + DSpinner | **三档分级**：L1 骨架屏 / L2 局部加载 / L3 顶部细进度条 |
| 反馈 | 模态 info/error 对话框 | **页内 `PanshiInlineBanner`**（成功自动消失 / 失败常驻可关），成功类操作 0 次模态弹窗 |
| 导航 | 7 个按钮平铺 | 分组 + 图标 + 键盘可达 + 设置搜索 |
| 跨页入口 | 只跳页 | 跳页 + **到达后动作**（总览快速操作直达目标操作，动作等目标页 refresh 完成后执行） |
| 后台任务 | threading.Thread | QtConcurrent + QFutureWatcher |
| 命令执行 | subprocess | QProcess（sudo -n / pkexec 回退） |
| 国际化 | 硬编码中文 | `tr()` + Qt Linguist，`.desktop` 带 `[en_US]` 变体 |

> ⚠️ **版本号说明**：代码库内所有来源（README、开发文档、`CMakeLists.txt` 的 `project(... VERSION 1.0.0)`）均标注版本 1.0.0；打包阶段已推进至 1.0.4（任务上下文所述，详情以 `debian/changelog` 为准）。撰写本文档时 `debian/` 目录内容不在可用来源范围内，无法交叉核验 1.0.4 的具体变更条目，故版本号引用以代码库声明为基准。

## 2. 环境与依赖

项目构建需要 C++17 编译器、CMake 与 Qt6/DTK6 开发包。在 Deepin 25 上安装如下依赖[1][3][4]：

```bash
sudo apt update
sudo apt install -y cmake g++ make pkg-config
sudo apt install -y qt6-base-dev          # Qt6 Core/Gui/Widgets/Concurrent/DBus
sudo apt install -y qt6-svg-dev           # Qt6::Svg
sudo apt install -y libdtk6core-dev libdtk6gui-dev libdtk6widget-dev
sudo apt install -y qt6-tools-dev-tools   # 可选：lupdate / lrelease（仅更新 .ts 时需要）
```

`CMakeLists.txt` 要求 `cmake_minimum_required(VERSION 3.16)`，并声明 `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg DBus LinguistTools)`[2]。有一条环境相关注意：`qt_add_translations()` 命令在本仓库的 Qt 6.8.0 发行版中仍定义在 `Qt6LinguistTools` 包内（`Qt6CoreMacros` 不含），**缺少此组件会导致 configure 阶段报 "Unknown CMake command"**，因此 LinguistTools 必须保留在必选组件列表中[2]。

包名搜索备用命令[3]：

```bash
apt-cache search qt6 | grep -iE "svg|base-dev"
apt-cache search dtk6 | grep -i dev
```



**正常开发机（deepin 25）**[1][3][4]：

```bash
cd panshi-settings-dtk
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

**本仓库实际开发环境是只读容器**（`/etc`、`/usr` 不可写，apt 无 dpkg 锁，sudo 失效），工具链为项目内自建 sysroot，**每次开新终端必须先 source 环境脚本**[4]：

```bash
source .toolchain/env.sh          # PATH / LD_LIBRARY_PATH / CMAKE_PREFIX_PATH / TMPDIR
cmake -B build2 -DCMAKE_BUILD_TYPE=Release
cmake --build build2 -j"$(nproc)"
```

容器环境的关键细节[4]：

- `.toolchain/env.sh` 除设置 PATH / LD_LIBRARY_PATH / CMAKE_PREFIX_PATH 外，还会把 `TMPDIR` 指到项目内 `.tmp` 目录——容器 `/tmp` 仅有 10MB，构建过程的大临时文件会写满它；
- **两个构建目录都是活的**（`build/` 与 `build2/` 同源，2026-10-04 已两边都补做过 cmake 配置）。改完代码两边都要编，若 `build/` 里没有 `CMakeCache.txt` 说明它是陈旧产物，跑的不是最新代码（曾出现过 `build/` 3.4MB vs `build2/` 30MB 的数量级差异）[4]。

CMake 配置要点（均在 `CMakeLists.txt` 内声明）[2]：

- C++17 强制（`CMAKE_CXX_STANDARD_REQUIRED ON`）、`AUTOMOC`/`AUTORCC` 开启、`CMAKE_EXPORT_COMPILE_COMMANDS ON`（生成 `compile_commands.json` 供编辑器/静态分析使用）；
- `qt_add_translations()` 在 configure 阶段生成并安装 `.qm`（细节见"国际化"章节）；
- **`-DCMAKE_INSTALL_PREFIX=/usr` 在打包时是硬性要求**——见下节 DConfig 路径陷阱。

开发运行（免安装）[1][4]：

```bash
QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager
```



主安装路径（需 root）[1][3]：

```bash
sudo cmake --install build
```

安装内容[3]：

| 条目 | 目标路径 |
|---|---|
| 二进制 | `/usr/bin/panshi-manager` |
| 图标 | `/usr/share/icons/hicolor/scalable/apps/panshi.svg` |
| 桌面文件 | `/usr/share/applications/org.deepin.panshi-manager.desktop` |
| DConfig schema | `/usr/share/dsg/configs/org.deepin.panshi-manager/` |
| systemd 用户单元 | `/usr/lib/systemd/user/panshi-monitor.{service,timer}` |
| 翻译 | `/usr/share/panshi-manager/translations/*.qm` |

> ⚠️ **DConfig 路径陷阱**：`-DCMAKE_INSTALL_PREFIX` 默认是 `/usr/local`，但 **DSG 库硬编码只扫描 `/usr/share/dsg`**，`DSG_DATA_DIRS` 环境变量无效。若以默认前缀安装，schema 会装到 `/usr/local/share/dsg/configs/`，DSG 永远不会扫到它，运行时表现为 "Can't load resource /org.deepin.panshi-manager" 且 DConfig 对象无效，监控功能静默回落到默认配置。因此无论开发期还是打包，构建时都必须显式传 `-DCMAKE_INSTALL_PREFIX=/usr`。



`install.sh` 是一键构建安装脚本，与 `cmake --install` 的区别在于它能**自动判断安装级别**，同时支持开发机与无 root 权限的容器两种情况[1][4]。

**模式判断**：`/usr/bin` 可写 → 系统级安装；不可写 → 用户级安装到 `~/.local`[4]。

| 条目 | 系统级（/usr 可写） | 用户级（~/.local） |
|---|---|---|
| 二进制 | `/usr/bin/panshi-manager` | `~/.local/bin/panshi-manager` |
| 图标 | `/usr/share/icons/hicolor/scalable/apps/` | `~/.local/share/icons/hicolor/scalable/apps/` |
| 桌面文件 | `/usr/share/applications/` | `~/.local/share/applications/` |
| DConfig | `/usr/share/dsg/configs/…` | `~/.local/share/dsg/configs/…` |
| systemd | `/usr/lib/systemd/user/` | `~/.config/systemd/user/` |
| 翻译 | `/usr/share/panshi-manager/translations/` | `~/.local/share/panshi-manager/translations/` |

`install.sh` 两个模式共用要点[4]：

- **`@PANSHI_BINDIR@` 占位符**：systemd service 的 `ExecStart` 里写的是 `@PANSHI_BINDIR@/panshi-manager` 占位符（CMake 侧经 `configure_file` 替换、install.sh 侧经 sed 替换），**任何模式都不得硬编码绝对路径**——硬编码会直接破坏另一个模式的启动器；
- **用户级安装绝不能运行 `update-desktop-database` / `gtk-update-icon-cache`**：Deepin 25 的 `dde-application-manager` 会自动监控应用目录并重建缓存，手动重建会与 dde-shell 竞争，可能冻结任务栏。install.sh 已处理此行为，二次开发时不要"好心"补上这两条命令。

## 3. 目录结构与代码模块地图

源码树延续原版 PyQt6 的模块划分思路，但按 C++/DTK 重写后的架构重新组织了边界。整体分四个层次：入口与主窗口、UI 组件层、页面层、自绘控件与磁盘监控子系统，外加一套开发期专用的探针工具[1][4]。

```
panshi-settings-dtk/
├── CMakeLists.txt              # 16 个可执行 target + 安装规则（§5.1）
├── install.sh                  # 一键构建安装：自动判断系统级 / 用户级
├── verify-runtime.sh           # C 类实机验证脚本（8 个可单跑段）
├── verify-unlocked.sh          # 权限解锁后的补充验证
├── make-testdata.py            # 造测试数据（badge / usage / spike）
├── org.deepin.panshi-manager.desktop
├── README.md
├── translations/               # 翻译源文件（i18n，见 §9）
│   ├── README.md                    # 命名约定与维护流程
│   ├── panshi-manager_zh_CN.ts      # 简体中文（恒等翻译，源串即中文）
│   └── panshi-manager_en_US.ts      # 英文
├── data/
│   ├── icons/
│   │   ├── panshi.svg              # 应用图标（安装到 hicolor/scalable/apps）
│   │   ├── icons.qrc               # 内嵌资源：自带 SVG 导航图标
│   │   └── nav/*.svg               # shield-check / layers-snapshot / cycle-update / info-badge
│   ├── dsg/configs/org.deepin.panshi-manager/
│   │   └── org.deepin.panshi-manager.json   # DConfig schema（16 键）
│   └── systemd/
│       ├── panshi-monitor.service   # Type=oneshot，ExecStart 含 @PANSHI_BINDIR@ 占位符
│       └── panshi-monitor.timer     # OnCalendar=*:00/15，Persistent=true
├── docs/                       # 设计稿、UI 重构进度、根因修复记录
├── src/
│   ├── main.cpp                # 入口：参数解析 / D-Bus 转发 / 单实例 / 主题信号 / 关于接管
│   ├── mainwindow.{h,cpp}      # DMainWindow：分组侧栏 + QStackedWidget + PageId + 到达动作
│   ├── backend.{h,cpp}         # 磐石系统后端：数据模型 + 同步命令封装（QProcess，799 行）
│   ├── style.h                 # 仅布局职责的 QSS（阶段 3-7b 后仅剩 5 个结构性颜色）
│   ├── ui/                     # UI 组件层（不含业务，阶段 0 新增）
│   │   ├── tokens.h            # 度量 token：间距/圆角/尺寸/动效/透明度（不含颜色）
│   │   ├── components.{h,cpp}  # 13 个通用组件 + 语义色 + 无障碍 helper（1083+494 行）
│   │   ├── confirmdialog.{h,cpp} # 全应用唯一确认组件（ConfirmRequest / askConfirm / askDanger）
│   │   ├── sidebar.{h,cpp}     # 分组导航侧栏 + 设置搜索框（自绘 QListView + delegate）
│   │   ├── settingindex.{h,cpp} # 设置搜索索引（纯数据，无 Q_OBJECT）
│   │   └── aboutdialog.{h,cpp} # 接管 DAboutDialog 的自绘关于窗
│   ├── pages/                  # 页面层
│   │   ├── basepage.{h,cpp}    # 页面基类：三档加载 + 注册点 + idleReached
│   │   ├── common.h            # 薄封装：makeScrollArea / pageHeader / ElidedLabel / 图标
│   │   ├── masterswitch.{h,cpp} # 「磐石功能」总开关共享逻辑（总览页 + 保护页共用）
│   │   ├── overviewpage.*      # 总览
│   │   ├── diskusagepage.*     # 磁盘空间（1729 行，最大页面）
│   │   ├── snapshotspage.*     # 备份快照（644 行）
│   │   ├── deployspage.*       # 系统版本（部署）
│   │   ├── versionpage.*       # 备份与版本 = 上面两个的 tab 复合页
│   │   ├── updatepage.*        # 系统更新
│   │   ├── policypage.*        # 策略与设置（搜索索引的主要来源）
│   │   ├── protectionpage.*    # 系统保护
│   │   └── aboutpage.*         # 关于
│   ├── widgets/                # 自绘控件（全部 paintEvent 内取 palette()）
│   │   ├── usagechart.*        # 时间线图表（柱/折线自适应 + 加载动画）
│   │   ├── scanoverlay.*       # 扫描动态面板
│   │   ├── checkoverlay.*      # 检查步骤面板
│   │   ├── radarwidget.*       # 雷达动画
│   │   └── countdowndialog.*   # 倒计时确认环
│   ├── disk/                   # 磁盘监控子系统（GUI 与 --monitor 进程共用同一份代码）
│   │   ├── disktypes.h         # 7 个数据结构
│   │   ├── history.{h,cpp}     # 逐日 JSON + index.json + 懒加载 + 预算裁剪 + 统计（697 行）
│   │   ├── scanner.{h,cpp}     # du 流式扫描 / 下钻 / root 降级 / df
│   │   ├── monitor.{h,cpp}     # 监控状态机：增长判定 → 下钻 → 冷却/排除/免打扰 → 通知
│   │   ├── monitorconfig.{h,cpp} # DConfig 封装（QSettings 兜底）
│   │   └── monitormain.{h,cpp} # --monitor 无界面入口
│   └── tools/                  # 开发期探针（不进安装）
│       ├── colorprobe.cpp      # 配色探针：离屏渲染 + 像素采样 + WCAG
│       ├── dciquery.cpp        # DCI 图标查询：名字是否存在、解析成哪个文件
│       ├── iconsheet.cpp       # 图标接触表：按文件路径加载 .dci（绕开 DConfig 不可用）
│       ├── navshot.cpp         # 侧栏渲染 + 饱和度/彩占比双指标断言
│       ├── aboutshot.cpp       # 关于窗渲染 + 15 条结构断言
│       └── dragmodalprobe.cpp  # 模态对话框阻塞拖拽探针
└── tools/                      # ⚠️ 不进 CMake 的独立探针（手工 g++ 编译）
    ├── probe_table_a11y.cpp    # 表格单元格无障碍
    ├── probe_history_flicker.cpp
    └── verify-history-flicker.sh
```

### 模块职责

**入口与主窗口**（`main.cpp` / `mainwindow.{h,cpp}`）是应用骨架。`main.cpp` 承担 DApplication 初始化、参数解析（`--monitor` / `--show-disk <date>`）、D-Bus 单实例转发与主题信号接线；`mainwindow` 负责侧栏导航、`QStackedWidget` 页面栈、`PageId` 强类型枚举跳转与「到达后动作」队列，同时完成设置搜索索引的接线。这一层是唯一同时认识所有页面的代码，页面与页面之间不允许直接持有彼此的头文件[1][3]。

**backend**（`backend.{h,cpp}`）是磐石系统命令层的同步封装，集中了 `deepin-immutable-ctl`、`deepin-immutable-writable`、`ostree` 等二进制调用的数据模型与 QProcess 流程。它是纯业务模块，不依赖任何 UI 头文件，页面层直接调用其同步方法 + `runAsync` 组合出异步 UI[1][4]。

**ui/** 是纯组件层，不含业务逻辑。`tokens.h` 定义度量 token（间距/圆角/尺寸/动效，**刻意不含颜色**）；`components.{h,cpp}` 提供 13 个通用组件（Panel/Section/Row/Badge/InlineBanner/StateCard/ResponsiveCardRow/EmptyState/三档加载等）与语义色映射；`confirmdialog` 是全应用唯一的确认组件；`sidebar` 与 `settingindex` 共同实现分组导航与设置搜索。组件层默认不用 `Q_OBJECT`（唯一例外是 `PanshiRow`，高亮动画需要属性），并且**禁止反向 include 任何页面头或 mainwindow**——组件与页面的协作完全通过基类注册点与信号完成[1][3][4]。

**pages/** 是页面层，每个页面继承 `BasePage`，通过基类提供的注册点（`registerResponsiveCardRow` / `setFeedbackBanner` / `runAsyncQuiet` / `registerSearchTarget`）把自己的结构交给共享逻辑，反向只依赖基类接口——这样共享逻辑不需要认识具体页面[3]。

**widgets/** 是自绘控件目录，全部在 `paintEvent` 内运行时取 `palette()`，不在构造时缓存颜色（这是暗色主题切换不出错的前提）。`usagechart` 承担时间线图表与加载动画，四个覆盖层/对话框控件（scan/check/radar/countdown）服务于磁盘页的扫描与确认交互[1][3][4]。

**disk/** 是磁盘监控子系统，代码在 GUI 与无界面 `--monitor` 进程间共用，因此必须保持**零 UI 依赖**——只允许依赖 QtCore/Concurrent，禁止 include `DWidget`。七个数据结构集中在 `disktypes.h`；`history` 实现逐日 JSON + 索引的懒加载存储；`scanner` 封装 `du` 流式扫描与 root 降级；`monitor` 是告警判定状态机；`monitorconfig` 封装 DConfig（带 QSettings 兜底）；`monitormain` 是监控模式的无界面入口[3][4]。

**src/tools/ 与 tools/** 两处探针都是**开发期专用、不进安装**。前者进入 CMake 构建（采用同一份 `include_directories` 与链接配置），后者需要手工 g++ 编译。探针分两类：一类做**像素级断言**（离屏渲染真实控件、采样实际像素色，如 `colorprobe`/`navshot`/`aboutshot`），另一类做**行为验证**（如表可达性、历史数据闪烁）。它们的存在源于一个反复出现的教训：视觉与行为缺陷光靠读代码猜根因会走弯路，把判据固化成语义明确的探针才能把"看起来对"变成"可回归地验证"[2][4]。

**依赖方向的硬约束**是理解这套地图的关键。`ui/*` 只依赖 `ui/tokens.h` + DTK + Qt，`backend.*` 与 `disk/*` 只依赖 Qt，页面间跳转走 `gotoPage()` + `Q_INVOKABLE` 动作契约，禁止 include 对方头文件。后端与磁盘子系统的零 UI 依赖不是风格偏好，而是架构前提——`--monitor` 无界面进程会直接链接这两个模块，一旦引入 UI 依赖整个后台监控服务就会在无显示环境下崩溃[4]。

## 4. 架构设计

### DApplication 单实例与 D-Bus 通信

应用入口在 `main.cpp` 中构造 `DApplication` 并完成四件事：参数解析、单实例认定、主题信号接线与关于窗接管。

**单实例机制**通过 `DApplication::setSingleInstance("org.deepin.panshi-manager")` 实现。该调用向 DDE 注册一个全局唯一标识，后续启动的实例会被 DApplication 自动识别为"非第一实例"，从而走转发路径而非重复初始化。这是整个 `--show-disk <YYYY-MM-DD>` 参数能够工作的前提[3]。

`--show-disk` 参数的语义是"通知查看详情"：用户从某个通知或入口点击"查看详情"时，可能应用并未运行，也可能已经运行。两种情形分别处理[3][4]：

- **冷启动**（无实例运行）：新实例直接进入磁盘页并定位到指定日期；
- **热转发**（已有实例运行）：新实例尝试向 D-Bus 服务 `org.deepin.panshi-manager.disknav` 注册对象，注册失败即说明已有实例在运行。此时调用运行中实例暴露的 `org.deepin.panshi.disknav.ShowDisk` 方法，将"查看某日期"的意图转交后自行退出。

这里有一个**命名上的硬约束**值得记牢：D-Bus 总线服务名允许连字符，但接口名不允许。因此服务名取 `org.deepin.panshi-manager.disknav`（带连字符、与桌面文件命名一致），接口名必须取 `org.deepin.panshi.disknav`（用点号分隔，去掉 manager 段）。同时 `registerObject()` 调用时必须**显式传入接口名**——不传则 D-Bus 使用类名做默认接口，调用端按 `org.deepin.panshi.disknav` 调用会失败。这两条都曾导致实际返工[4 §D7]。

除了单实例，`DApplication` 作为入口还统一承担了翻译装载（`loadTranslator()`）与日志初始化，这两项能力是普通 `QApplication` 不具备的，也是重写版替换 PyQt6 版 `QApplication` 的直接理由[1]。

### 页面基类与跨页跳转契约

#### 页面基类（BasePage）注册点

`BasePage` 是全部页面的基类，它不替页面排版，而是提供一组**注册点**：页面在构造时把自己的结构交上去，共享逻辑只与基类对话，从而避免组件层或共享逻辑反向依赖具体页面。这一"注册类方法漏调＝静默失效"的范式是理解页面层与组件层协作方式的关键[3][4]。

四个注册点及其用途如下[3 §3.1][4 §2.3]：

| 注册点 | 用途 |
|---|---|
| `registerResponsiveCardRow(row, scrollArea, hPadding)` | 把卡片行宽度喂给响应式折列逻辑。双路径触发（viewport Resize + `showEvent`），解决隐藏页首次显示时采不到真实尺寸的问题 |
| `setFeedbackBanner(banner)` / `showFeedback(s, text, autoHide)` | 页面级反馈宿主。「磐石功能」总开关的共享逻辑（`masterswitch`）经此写入操作结果 |
| `runAsyncQuiet(fn, onDone)` | 完全不碰整页遮罩的静默刷新，用于切标签预读 |
| `registerSearchTarget(key, row, scroll)` / `highlightSetting(key)` | 设置搜索定位：滚动到目标行并高亮 |

组件层默认**不使用 `Q_OBJECT`**——唯一例外是 `PanshiRow`，其高亮动画需要 `Q_PROPERTY` 支持属性动画。这一选择让组件层保持轻量，同时不牺牲动画能力[3 §3.1]。

新增组件的硬性约束是四件事：颜色取自 `DPalette`、度量取自 `tokens.h`、文案用 `tr()` 包裹、动效遵守 `reduceMotionRequested()`。任何违反其中一条的新组件，都会在主题切换或字体缩放时暴露为视觉缺陷[3 §3.1]。

#### 跨页跳转「到达后动作」队列

总览页的快速操作是一次完整意图，不该拆成"跳页 + 用户自己再找一次按钮"。但目标页的 `refresh()` 是异步的，跳页后立即执行动作会踩两个坑：动作作用在尚未刷新的旧数据上；动作入口普遍带 `if (m_running) return;` 防重入守卫，refresh 在途时动作被静默丢弃——用户观感是"点了没反应"[3 §3.2]。

因此 `MainWindow` 提供两个重载，把动作挂到目标页的空闲信号上、refresh 结束后执行[3 §3.2]：

```cpp
void gotoPage(PageId id, std::function<void()> afterArrive);
void gotoPage(PageId id, SubPageId sub, std::function<void()> afterArrive);
```

实现要点分布在七个环节，每一个都有明确的失败场景作为依据：

| 环节 | 实现 | 失败场景 |
|---|---|---|
| 空闲信号 | `BasePage::idleReached()`——基类用 `m_inFlight` 计数归零时发出 | 缺失则异步刷新完成时机无法被感知 |
| 回调顺序 | **`onDone(res)` 必须排在 `endTask()` 之前** | 否则"完成回调里再起一段"的页面（总览页快速状态→慢速统计）会在半程误发空闲 |
| 复合页 | `VersionManagementPage::isBusy()` 覆写为聚合两个子页（自身 `refresh()` 只是转发，计数恒为 0），并把子页 `idleReached` 转达上来 | 缺失则"备份与版本"tab 页的空闲永远检测不到 |
| 立即执行 | `!target->isBusy()` 时直接执行（`UpdatePage::refresh` 是空实现） | 等一个永不到来的信号会卡死 |
| 超时兜底 | `kArrivalTimeoutMs = 4000` | 后端阻塞导致页面迟迟不空闲时仍放行动作 |
| 后到覆盖先到 | 同一时刻只保留一个待执行动作，覆盖时解绑旧宿主 | 缺失则快速连点产生动作堆积 |
| 切页作废 | `switchTo()` 开头 `flushArrivalAction(false)` | 用户改点侧栏时旧动作不应迟到弹出 |
| 退出作废 | `closeEvent()` 里 `flushArrivalAction(false)` | 窗口关闭后异步回调不应再弹框 |

页面侧的**动作契约**：各页暴露一个 `Q_INVOKABLE` 方法供跨页调用，总览页不持有目标页头文件[3 §3.2]：

| 页面 | 对外方法 | 内部动作 |
|---|---|---|
| `VersionManagementPage` | `requestCreateSnapshot()` | `showSubPage(0)` → 转发 `SnapshotsPage::onCreate` |
| `UpdatePage` | `requestCheckUpdate()` | `onCheck()` |
| `ProtectionPage` | `highlightEnableProtect()` | 滚入视野 + `m_rowEnable->flashHighlight()` |

跨页调用统一用 `QMetaObject::invokeMethod(..., Qt::DirectConnection)` 执行；`PageId` 是强类型枚举，页面注册在 `QMap<PageId, BasePage*>` 中，枚举顺序＝导航顺序＝QStackedWidget 下标[4 §2.3]。

值得注意的设计取舍：**保护页的动作只做定位高亮、不自动执行**。关闭可写层是危险操作，跨页跳转负责"把用户带到正确的地方并指出那一行"，不代替用户做决定。这一原则也体现在总览页快速操作列表中——只有"创建快照"与"检查更新"有直达动作，"关闭保护"只定位[3 §3.2]。

与设置搜索的界限也要分清：搜索落地走 `gotoSetting()` → `switchTo()` + `highlightSetting()`，**不经到达动作队列**。两套机制互不干扰，且子页切换必须在 `switchTo()` 之前完成，否则动作会落在错误的子页上[3 §3.2][4 §3.10]。

### UI 设计体系

#### 颜色体系

重写版最核心的视觉决策是**颜色职责全部交还 `DPalette`**——应用层不再用 hex 值定义颜色，一律经 `DGuiApplicationHelper::applicationPalette()` 运行时取值，并通过 `applicationPaletteChanged` / `themeTypeChanged` 信号自动跟随系统亮暗主题[3 §4.4][4 §4.1]。

语义收敛为 `PanshiUi::Semantic` 枚举，在 `components.h` 中经 `semanticColor()` / `applySemantic()` / `softFill()` 派生为具体 DPalette 角色[3 §4.4]：

| 语义 | 映射角色 | 用途 |
|---|---|---|
| Muted | TextTips | 次级说明文字 |
| Normal | TextTitle | 主文字 |
| Accent | Highlight | 强调 / 交互高亮 |
| Success | Highlight | 成功状态（复用强调色） |
| Warning | TextWarning | 警告 |
| Danger | TextWarning | 危险（刻意与 Warning 同色） |

两条反直觉的设计决策需要解释：

- **Danger 与 Warning 同色**（同为 TextWarning）是刻意设计。初版 Danger 曾映射 `TextLively`，实测该角色在系统主题下解析为中性黑，视觉效果完全失效。为保证 WCAG 1.4.1（非文本对比度）通过，危险与警告靠图标＋文案＋控件类型区分，而非靠颜色[3 §4.4]。
- **QSS 优先级高于 DPalette**——只要 QSS 里留一条颜色规则，控件就不再跟随主题。这是暗色主题反复出问题的根因之一[4 §F7]。

由此得出三条编码红线：禁止写 hex 色值、禁止用 QSS 属性选择器（`[stat=...]`）染色、**禁止在构造时缓存颜色**——所有自绘控件必须在 `paintEvent` 内运行时取 `palette()`，否则首次构造于亮色、之后切暗色时控件会永久停留在旧色[3][4 §4.1]。

`style.h` 在阶段 3-7b 后只剩 5 个结构性颜色（背景/侧栏/卡片/边框/次级文字），无任何语义色插值——布局职责归 QSS，语义颜色归 DPalette，这是二者清晰的职责分界[1][4 §F3]。

字体与图标是颜色体系的两翼：字号一律走 `DFontSizeManager` 分级（T2~T8），响应系统字体缩放，禁 QSS `font-size`；图标一律走 `DIconTheme::findQIcon()`，带 5 级回落链（自带 SVG→DCI 彩色首选→DCI 回落→QStyle），随系统图标主题切换并自动 symbolic 回退[1][4 §3.7/§4.2/§4.7]。

#### 度量 Token（tokens.h）

`src/ui/tokens.h` 集中定义全部度量 token，与颜色彻底分离（tokens.h 不含任何颜色，与 style.h 的职责分界一致）[1][4 §4.3]：

| 分组 | Token | 值 |
|---|---|---|
| Space | XS / S / M / L / XL / XXL | 4 / 8 / 12 / 16 / 24 / 32 |
| Radius | Element / Frame | 4 / 8 |
| Size | Icon | 16 / 20 / 24 / 32 |
| Size | RowControlMinWidth | 200 |
| Size | StateCardMinWidth | 168 |
| Motion | Micro / State / PulsePeriod / HighlightHold / HighlightFade | 150ms / 250ms / 1200ms / 1200ms / 320ms |
| Alpha | SoftFill / BannerFill / Outline | 38（≈15%）/ 51（≈20%）/ 51 |

Token 的价值在于消除"每个组件各自定义一遍"的漂移。新增组件时度量取自 tokens.h 是四条硬性约束之一，与颜色取自 DPalette 并列[3 §3.1]。

动效还要额外遵守 `reduceMotionRequested()`——该 API 读取 `com.deepin.wrap.gnome.desktop.interface` 的 `enable-animations` 键。注意两点坑：键所在 schema 不是 `org.deepin.dde.appearance`；DTK6 的取值 API 是 `keyList()` 而非 Qt 惯用的 `keys()`[4 §4.8]。

#### 控件层策略

**原则：能用 DTK 控件就不用 Qt 原生控件。** 前者自动跟随 deepin 主题、圆角与动效规范，后者需要额外 QSS 才能勉强对齐，且主题切换时容易失配[3 §3.3]。

阶段 3-7b 把磁盘页最后一批 Qt 原生控件替换为 DTK 对应物，全部保持行为等价[3 §3.3]：

| Qt 原生 | DTK 替代 | 关系 | 注意 |
|---|---|---|---|
| `QProgressBar` | `DProgressBar` | 继承 | 仅覆写 `sizeHint` / `minimumSizeHint`；`setFormat` / `setTextVisible` / `setRange` / `setValue` 全部沿用 |
| `QDoubleSpinBox` | `DDoubleSpinBox` | 继承 | 新增 `setAlert` 支持错误态 |
| `QListWidget` | `DListView` | **不自带模型** | 需配 `QStringListModel`，替换时行为不等价，须改代码 |
| `QPushButton` | `DPushButton` | 兄弟类 | 成员声明应写 `DPushButton*`，用 `QPushButton*` 声明会失去 DTK 风格 |
| `QCalendarWidget` | 无 DTK 替代 | 保留 | **「单日历两次点击定区间」范式**：摘要文字以 from/to 状态为唯一数据源，消除高亮与文字分叉 |
| `QTableWidget` | 无 DTK 等价表格 | **有意保留** | DTK 无等价表格控件，替换会引入更多自绘成本 |

`DProgressBar` 与 `DDoubleSpinBox` 因为是继承关系，替换零风险；`DListView` 是行为不等价的替换，必须同时改模型代码；`QCalendarWidget` 与 `QTableWidget` 是 DTK 生态缺口，前者用交互范式收敛（两次点击定义区间），后者明确保留[1][3 §3.3]。

组件层（`components.{h,cpp}`）提供 13 个通用组件：`PanshiPanel` / `PanshiSection` / `PanshiRow` / `PanshiBadge` / `PanshiInlineBanner` / `PanshiStateCard` / `PanshiResponsiveCardRow` / `PanshiEmptyState` / 三档加载 / `PanshiSeparator` / `PanshiCollapsible` 等[1]。三档加载分级是重构的关键收益：L1 骨架屏、L2 局部 spinner、L3 顶部细进度条，取代了原版的全页遮罩，页面切换不再有"整页盖住"的阻塞感[4 §2.3]。

反馈流程方面：全应用唯一的确认组件是 `PanshiUi::ConfirmRequest` + `askConfirm` / `askDanger`（支持后果清单、单选选项、倒计时）；操作反馈统一走页内 `PanshiInlineBanner`（成功自动消失 / 失败常驻可关），成功类操作达到 0 次模态弹窗[4 §4.4/§4.5]。

### 后台任务与命令执行

#### 异步任务

线程模型是"双区"架构：主线程承载全部 UI 与 QProcess 事件循环；QtConcurrent 工作线程只承担两类纯计算——`BasePage::runAsync(fn, onDone)` 里的 `fn`（即 backend 同步调用）与 `DiskHistoryStore::requestRange` 的逐日读文件。回调统一回主线程[3 §4.2][4 §2.2]。

`BasePage::runAsync(fn, onDone)` 的**时序契约**是三类加载分级的共同基础[4 §2.2]：

```cpp
hideLoading();   // 先收加载态
onDone(res);     // 再派发结果
endTask();       // 最后归零计数、可能发出 idleReached
```

`onDone` 必须排在 `endTask()` 之前，否则"完成回调里再起一段"的页面（总览页快速状态→慢速统计）会在半程误发空闲信号——这正是跨页到达动作队列的触发时机，时序错位会直接破坏"到达后动作"的可靠性[4 §2.2][3 §3.2]。

三个加载等级的使用场景：L1 骨架屏用于冷启动页面占位；L2 局部 spinner 用于页面内部的小块数据加载；L3 顶部细进度条用于静默刷新。`runAsyncQuiet` 完全不碰整页遮罩，专用于切标签预读——用户提前切到某页时，数据已经在后台加载完，正式切到时秒开[4 §2.3]。

磁盘监控子系统的异步与页面层不同，属于**数据流式处理**，不能靠一句 `runAsync` 解决[3 §6.3]：

- `DiskScanner` 基于 QProcess 流式解析 `du` 输出——扫描大目录时 UI 不被阻塞，进度经信号逐段上报（`progress` / `topLevelFinished` / `drillFinished` / `scanFinished`）；
- root 权限策略经 `setAsRoot` + `RootPolicy` 控制：`AutoNoPrompt` 用于自动扫描（仅 `sudo -n` 免密），`AllowPrompt` 用于手动扫描（可 `pkexec` 弹授权框）；
- `DiskHistoryStore::requestRange` 逐日读取历史文件，带 `requestId` 取消与防抖；内存 LRU 缓存跨线程访问时加锁

## 5. 数据与持久化

### DConfig Schema（16 键全 readwrite）

磁盘监控的全部配置通过 DConfig 系统配置存储，schema 位于 `data/dsg/configs/org.deepin.panshi-manager/org.deepin.panshi-manager.json`[3][4]。这套配置是**系统级**配置（与策略页的「磐石系统参数」同层），GUI 写入、监控进程读取，因此 16 个键全部是应用可读可写（readwrite）的，schema 中每个键的 flags 均为空数组（无访问限制）[4 §3.6]。

16 个键全部以 `monitor.` 为前缀，属于磁盘监控配置域[3 §7.1]：

| 键 | 类型 | 默认值 | 用途 |
|---|---|---|---|
| `monitor.enabled` | bool | `false` | 后台监控总开关 |
| `monitor.pollMinutes` | int | `15` | 轮询间隔（分钟） |
| `monitor.baselineDays` | int | `7` | 基线窗口天数，用于判定异常增长 |
| `monitor.alertMode` | string | `"baseline"` | 告警模式：基线（`baseline`）或固定阈值（`fixed`） |
| `monitor.fixedThresholdMB` | int | `1024` | 固定告警阈值（MB），仅 alertMode=fixed 时生效 |
| `monitor.baselineMultiplier` | double | `3.0` | 基线倍数阈值，日均增长超过基线 × 该倍数即告警 |
| `monitor.minAlertMB` | int | `512` | 最小告警量（MB），防止噪声触发 |
| `monitor.cooldownHours` | int | `6` | 告警冷却时长（小时），冷却期内不再重复告警 |
| `monitor.quietStart` | string | `"23:00"` | 免打扰时段开始（HH:MM） |
| `monitor.quietEnd` | string | `"07:00"` | 免打扰时段结束（HH:MM） |
| `monitor.excludes` | string[] | `["~/.cache"]` | 排除的扫描目录 |
| `monitor.retentionDays` | int | `400` | 历史数据保留天数（GUI 可配 90–730） |
| `monitor.storageBudgetMB` | int | `512` | 历史数据存储预算（MB，GUI 可选 256/512/1024） |
| `monitor.samplesRetentionDays` | int | `90` | 采样明细保留天数 |
| `monitor.memoryCacheDays` | int | `90` | 内存缓存天数 |
| `monitor.snoozeUntil` | string | `""` | 免打扰挂起截止时间（用于「一周不再提醒」类交互） |

封装层在 `src/disk/monitorconfig.{h,cpp}`，提供 `MonitorConfig::setRawValue(key, value)` 等读写接口[3 §6.6]。需要注意两点环境约束：

- **路径硬编码**：DSG 库把配置搜索路径硬编码为 `/usr/share/dsg/configs/`，`DSG_DATA_DIRS` 环境变量在当前 DTK 版本上不生效[4 §5.3]。schema 装到别处应用就读不到，因此打包必须用 `-DCMAKE_INSTALL_PREFIX=/usr`（详见「deepin 打包规范」章节）。
- **开发机降级**：开发机上若没有 schema 文件，`DConfig` 会报 "Can't acquire config manager" 的良性告警，此时 `MonitorConfig` 自动回退到 QSettings 读写同键（见下节）[4 §9.1a]。

### QSettings 降级

开发机或非 deepin 环境下 DConfig schema 不可用时，`MonitorConfig` 回退到 QSettings，存储路径为 `~/.config/deepin/panshi-manager.conf`[3 §6.6][4 §1.1]。

回退机制有一个**关键坑（已修复）**：DConfig 的键名用点号分隔（`monitor.enabled`），而 QSettings 的 INI 层级用斜杠（`monitor/enabled`）——降级读写时**必须做 `.` → `/` 的转换**，否则读写不一致，配置会静默丢失[3 §6.6]。

除磁盘监控配置外，策略页的备份保留策略是**应用级**配置，走 QSettings 即时保存；不经过 DConfig。这与「磐石系统参数」（系统级、显式保存）是两套不同的持久化语义，分别对应 GUI 上「自动生效」与「需点保存」的差异[1][4 §3.6]。

### 磁盘监控历史数据

监控历史数据存放在 `~/.local/share/panshi-manager/disk-history/`（`GenericDataLocation` 下），采用**逐日 JSON 文件 + 轻量索引 + 去重告警**三文件结构[3 §6.1][3 §7.2]：

| 文件 | 内容 | 要点 |
|---|---|---|
| `index.json` | 轻量索引（KB 级） | 保存 `version`、`first`/`last` 日期、`storage`/`budget` 统计与 `days[]` 数组（每项含 `date` 与 `f`/`c`/`t`/`g`/`spike`/`top` 缩写字段）[3 §7.2] |
| `YYYY-MM-DD.json` | 当日完整记录 | 含 `date`、`capacity`、`total`、`complete`（false 表示降级扫描）、`topLevel[]`、`drillCache{}`、`samples[]`[3 §7.2] |
| `alert.json` | 去重后的告警记录 | 保留最近 200 条[3 §6.1] |

三个文件的定位不同：索引为时间线渲染提供毫秒级加载；逐日文件承载完整明细（下钻与采样）；告警文件与 GUI 的告警横幅联动[3 §6.4][4 §2.4]。

两个容量优化的设计决策：

- **字段缩写**：JSON 字段名大量使用单字母缩写（`ts`/`h`/`n`、`c`/`t`/`g`），因为要写入成千上万个记录，长字段名会显著膨胀存储体积。新增字段必须为旧数据提供中性默认值（0 或空），保证向后兼容[4 §3.5]。
- **原子写入**：逐日文件与索引均通过 `QSaveFile` 临时文件 + rename 原子写入，**从不整文件重写**——监控进程与 GUI 同时读写同一目录，原子写避免读侧拿到半截文件[3 §6.4]。

`DiskHistoryStore` 提供懒加载访问：`requestRange(start, end)` 异步逐日读取，带 `requestId` 取消与 LRU 内存缓存，进度经 `rangeProgress`/`rangeLoaded` 信号上报；`loadIndex()` 毫秒级返回索引数据[3 §6.4][4 §2.4]。索引缺失或损坏时 `rebuildIndex()` 会重扫整个目录[4 §2.4]。

存储生命周期由两个约束管理[3 §6.4]：

- **保留期**：`pruneExpired(retentionDays)`，默认 400 天（GUI 可配 90–730）；
- **预算降级**：默认存储预算 512MB（可配 256/512/1024），超预算时按级裁剪——先裁剪非近期的采样明细 → 再裁剪非近期的下钻缓存 → 把最旧记录合并成汇总摘要 → 最后停止降级。降级后当日记录标记 `complete=false`，UI 显示为不完整数据。

GUI 侧通过 `QFileSystemWatcher` 监听 `disk-history/` 目录，配合 400ms 防抖在监控进程写入新数据后自动刷新时间线[3 §6.9]。这一多进程共享同一份代码、同一种存储格式的架构，是 `src/disk/*` 保持零 UI 依赖的直接原因——GUI 与 `--monitor` 无界面进程都只靠这套磁盘存储交换数据[3 §6.1][4 §2.1]。

## 6. 后台监控服务

磁盘空间监控的完整闭环由三部分组成：systemd 用户单元（调度）、`--monitor` 无界面模式（执行一轮监控）、DConfig 配置（行为控制）。三者共享 `src/disk/` 目录下的同一份代码——监控进程与 GUI 通过磁盘上的历史数据文件交换信息，这是 `disk/` 保持零 UI 依赖的直接原因：`--monitor` 进程不加载任何 DWidget，只依赖 QtCore/Concurrent[4 §1.2]。

### systemd 用户单元

两个单元文件位于 `data/systemd/`：

**`panshi-monitor.service`**——`Type=oneshot`，`ExecStart` 带 `@PANSHI_BINDIR@` 占位符，经 `configure_file` 按实际安装前缀生成（默认 `/usr/local`，打包镜像 `/usr`），与 `.desktop` 文件的处理方式相同[2]。进程以最低调度优先级运行：`Nice=19`、`IOSchedulingClass=idle`、`IOSchedulingPriority=7`，确保监控扫描不抢用户交互的 IO[2][3]。

**`panshi-monitor.timer`**——`OnCalendar=*:00/15`（每 15 分钟触发）、`Persistent=true`（错过即补跑，覆盖休眠/注销时段）、`RandomizedDelaySec=90`（每次触发随机延迟 0–90 秒，分散多实例的 IO 峰值）[2][3]。

安装规则：两个文件都装到 `lib/systemd/user`（用户级单元目录），CMake 侧通过 `install(FILES ... DESTINATION lib/systemd/user)` 实现[2]。注意 `lib/systemd` 在 `/usr` 前缀下，这要求最终安装前缀必须是 `/usr`——与 DConfig schema 的路径硬编码约束一致（见「数据与持久化」章节）。

GUI 侧的开关联动在策略页实现：用户切换「磁盘空间监控」开关时，先写 `MonitorConfig.setEnabled(true)`，随后调用 `systemctl --user enable --now panshi-monitor.timer` 激活单元[3 §6.7]。单元未安装（如仅编译未安装的开发机）时 `systemctl` 报 `Unit file does not exist`，此时配置已写入但不会执行——GUI 需给出相应提示[3 §10.2]。

**崩溃自愈**是 oneshot + timer 组合的内建属性：每轮 `runCycle` 跑完即退出，监控进程崩溃不残留常驻状态，下一个 15 分钟 tick 自动重新拉起。不需要独立的看门狗进程[3 §6.1]。

### --monitor 模式

入口在 `src/disk/monitormain.{h,cpp}`，由 `main.cpp` 在参数解析阶段识别 `--monitor` 后进入。该模式不创建任何窗口、不加载 DWidget——因此 `disk/` 目录对 UI 的零依赖是一项硬约束[3][4 §1.2]。

**`DiskMonitor::runCycle()` 是单轮状态机**，7 步顺序执行，跑完即退出[3 §6.5]：

1. **读配置**——从 MonitorConfig（DConfig）加载全部阈值与开关；未启用直接退出。单轮空转成本为零（仅读配置 + 退出）；
2. **df 采样**——追加一条当日采样到当前日期记录；
3. **基线计算**——取最近一个有完整 topLevel 数据的历史日做基准，计算日均增长；
4. **增长判定与定位**——若增长达到告警阈值（双模式：默认 `baseline` 近 7 天正向日均增长 × `baselineMultiplier` 默认 3.0 且 ≥ `minAlertMB` 512MB；或 `fixed` 固定 `fixedThresholdMB` 1024MB），对增长最大的目录做顶层 `du` → diff 定位最大增长目录 → 下钻 `du` 找到具体子项。所有 `du` 以 `nice -n 19` + `ionice -c3` 执行[3 §6.2]；
5. **告警汇出**——经过冷却周期（`cooldownHours` 默认 6）、排除目录（`excludes` 默认 `~/.cache`）、免打扰时段（`quietStart`/`quietEnd` 默认 23:00–07:00）三重过滤后，经 `org.freedesktop.Notifications` 发送 DDE 通知，并写入 `alert.json` 去重。通知携带 `x-deepin-action-view` hint `panshi-manager,--show-disk,<date>`，用户点击「查看详情」即触发 `--show-disk` 参数冷启动或 D-Bus 转发（见「架构设计」章节）；
6. **补全基线**——当日无完整基线时补一次全量 `du`；
7. **落盘与清理**——`saveDayRecord()` 写当日记录，内部执行保留期裁剪与存储预算降级，然后退出。

**三层采样**是性能与完整性的平衡[3 §6.2]：高频轮询每 15 分钟仅做 `df`（几乎零成本），感知总量突然上涨；事件触发定位在增长超阈值时按需对增长目录 `du` 下钻；每日基线每日 1 次全量 `du` 供时间线、趋势与容量预测。另有两个防误报机制：双重确认（增长需连续两次采样确认才触发定位与告警，过滤瞬时波动）；root 自动降级（自动扫描仅试 `sudo -n` 免密，失败回退普通用户权限扫描并标记当日 `complete=false`，时间线以灰色呈现不完整数据）[3 §6.2]。

监控进程与 GUI 的数据交换完全通过磁盘文件完成：`~/.local/share/panshi-manager/disk-history/` 下的 `index.json` + 逐日 JSON + `alert.json`（数据结构见「数据与持久化」章节）。GUI 侧的 `QFileSystemWatcher` 监听该目录，配合 400ms 防抖在监控进程写入新数据后自动刷新时间线——多进程共享同一份代码、同一种存储格式，无需进程间通信[3 §6.9]。

`--monitor` 模式也支持开发运行：无需 root 即可跑一轮（只是数据不完整），便于本地验证状态机与告警链路[4 §0.3]。

## 7. 国际化（i18n）

界面文案全部走 `tr()`，**源语言即简体中文**（代码中的源串直接写中文），目标语言为 `zh_CN`（恒等）与 `en_US`。命名与安装约定对齐 DTK 官方要求。

### 7.1 文件与命名约定

```
translations/
├── panshi-manager_zh_CN.ts      # lupdate 生成/更新的翻译源
├── panshi-manager_en_US.ts
└── README.md                    # 维护流程说明
```

- `.qm` 的 basename 必须与 `DApplication::applicationName` 一致（`panshi-manager`），否则 `loadTranslator()` 按 appName 查找时找不到；
- `.ts` 缺失时 `qt_add_translations()` 仍能正常工作（只是不生成该语言的 `.qm`），首次导入需先跑一次 `lupdate` 生成 `.ts`；
- `.desktop` 入口带 `Name[en_US]` / `Comment[en_US]` 变体，实现启动器层面的双语显示。

### 7.2 构建与安装（两个实测坑）

`CMakeLists.txt` 中：

```cmake
qt_add_translations(panshi-manager
    TS_FILES ${PANSHI_TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
    QM_FILES_OUTPUT_VARIABLE PANSHI_QM_FILES
)
install(FILES ${PANSHI_QM_FILES}
        DESTINATION share/panshi-manager/translations)
```

实测结论（2026-10-03 B3 系统级安装验证）：

1. **`RESOURCE_PREFIX` 无效**：`qt_add_translations()` 的 `RESOURCE_PREFIX` 只把 `.qm` 编进 Qt 内嵌资源（`:/i18n/`），而 DTK 的 `loadTranslator()` **不读 Qt 资源、只搜文件系统**。因此本项目**不用** `RESOURCE_PREFIX`，改经 `QM_FILES_OUTPUT_VARIABLE` 取出产物后显式 `install`。
2. **不自动生成 install 规则**：`qt_add_translations()` 不会为 `.qm` 添加安装规则，漏掉显式 `install(FILES ...)` 会导致干净系统上安装版没有任何翻译、英文静默回落中文。

安装位置 `share/panshi-manager/translations/` 即 `GenericDataLocation/appName/translations`（在 XDG_DATA_DIRS 内），与用户级安装 `~/.local/share/panshi-manager/translations` 同构。

### 7.3 维护流程

1. 改代码新增 `tr("...")` 后运行 `lupdate`（`qt_add_translations` 的 LUPDATE_OPTIONS 带 `-no-obsolete`，自动清理废弃条目）；
2. 编辑 `.ts` 补齐 `en_US` 译文（`zh_CN` 恒等翻译；`en_US` 留空条目会回落中文）；
3. 重新构建生成 `.qm`，检查无 `unfinished` 条目；
4. 语言切换入口在标题栏菜单，切换后重启生效（端到端验证见 `verify-runtime.sh i18n` 段）。

## 8. 测试与验证体系

本项目贯彻「**颜色必须实测，不能靠'我以为'**」的工程方法：视觉与交互缺陷（图标看不清、深底黑字、弹窗无响应、点击失效）全部用**离屏渲染 + 像素采样**转成可断言的数值，探针固化判据，防回归。验证分三层：构建期探针（自动、离屏）、实机验证脚本（半自动、默认干跑）、人工实机走查（亮暗主题 / X11+Wayland）。

### 8.1 开发期探针（一律不安装）

探针目标统一加在 `CMakeLists.txt`，`target_include_directories` 指向 `src`，**没有任何 `install(TARGETS ...)`**——探针只存在于构建目录：

| 探针 | 验证对象 | 关键判据 |
|---|---|---|
| `panshi-dragmodalprobe` | 模态对话框阻塞拖拽 | 5 个场景各打一行 PASS/FAIL；**场景 2（ApplicationModal）必须 FAIL、场景 4（WindowModal）必须 PASS**，两者同时成立才说明探针真能测到缺陷（防假阴性） |
| `panshi-colorprobe` | 使用率分档颜色 | 离屏渲染真实 DLabel 并采样实际像素色，输出 JSON 供脚本断言；进程内 setThemeType() 强制主题，亮暗两套自动跑 |
| `panshi-protectionprobe` | 系统保护页状态卡 | 用页面同一份映射（`protectioncards.h`）离屏渲染三分支，断言常态色 ≠ Muted 灰，每主题 12 条、亮暗各跑 |
| `panshi-darkprobe` / `panshi-palettetest` / `panshi-darkfix` | 暗色「深底黑字」根因 | 带/不带 `setPalette` 两条路径像素对比（darkprobe）；逐步量测因果链（palettetest）；4 个候选修复方案亮→暗→亮三轮对比可逆性（darkfix） |
| `panshi-dciquery` | DCI 图标解析 | 直接问 `DIconTheme`：搜索路径、某名字各主题下解析成哪个文件 |
| `panshi-iconsheet` | 图标候选接触表 | 按文件路径加载 .dci，按侧栏真实尺寸 20px + 3 倍放大，输出浅底/深底两张对照表 |
| `panshi-aboutshot` / `panshi-navshot` / `panshi-deployshot` | 自绘/复杂界面结构 | 离屏渲染成图 + 结构断言（清空重铺是否干净、图标饱和度、徽章层级） |
| `panshi-a11y-probe` | 无障碍/键盘可达 | 侧栏与控件键盘导航断言 |
| `panshi-animprobe` | 「减少动态效果」开关 | 三方对照：gsettings 真值 / DTK HasAnimations / DConfig 旧路径；`PANSHI_ANIMPROBE_EXPECT=0|1` 自动化断言 |
| `panshi-backendprobe` | 总览页取值来源 | 打印整个 deploy_list 并断言「当前运行」用 IsBooted 判定（含反向对照：不得再取 first()） |
| `panshi-deployprobe` | 部署删除规则 | R1 隐藏主线不可删、R2 删除传 commitId；ctl 不可用时 SKIP |
| `panshi-iconprobe` | 图标解析链路 | 三段定位：desktop Icon 名 → XDG 主题解析 → DTK `findQIcon` |
| `panshi-clippprobe` | wordWrap 标签裁切 | 判据 `QLabel::height() < heightForWidth(width())`；**禁止**改用兄弟控件几何相交（假阴性） |
| `panshi-overlapprobe` / `panshi-dpitest` / `panshi-chk` | 弹窗层级 / 高DPI / 综合 | 构建目录中的其余探针，见 `CMakeLists.txt` 注释 |

### 8.2 实机验证脚本（默认干跑）

- **`verify-runtime.sh`**：C 类实机验证清单。段：`dbus`（D-Bus 定位转发自动断言）/ `dconf`（DConfig 16 键静态交叉 + 实机回退）/ `i18n`（翻译加载自动）/ `usage`、`colors`（像素采样 + WCAG 对比度）/ `backend`、`theme`（人工项）。默认只打印计划，`--go` 才执行；构建目录自动探测 `$PANSHI_BUILD → build2 → build`。
- **`verify-unlocked.sh`**：需要 sudo 密码的验证（B3 系统级安装、C7 sudo 阻塞、C4 还原冲突跳转）。密码从环境变量 `SUDO_PASS` 取，不写进命令历史；破坏性步骤只输出步骤、永不自动执行。

### 8.3 回归基线

发布前检查（见「版本发布流程」）：干净全量构建 0 error 0 warning → 探针全绿 → i18n 无 unfinished → 验收计数 → 安装到 staging 目录检查落点。

## 9. deepin 打包规范与商店投稿

本项目的 `debian/` 目录参照 deepin 官方示例（`linuxdeepin/deepin-skills` 的 dtk-codeviewer 与 dde-shell-sysmon）编写，1.0.4 已按此规范实际打出 `panshi-manager_1.0.4_amd64.deb` 并通过全部校验。以下为字段级说明与实测结论。

### 9.1 包结构与 source format

```
debian/
├── source/format        # "3.0 (native)" — 本项目无上游 tarball，用 native 格式
├── control              # 二进制包与构建依赖声明
├── rules                # dh 构建规则（cmake 构建系统 + 打包加固）
├── changelog            # 版本历史（RFC 5322 日期格式）
├── copyright            # 机器可读版权声明
├── postinst             # 安装/升级后维护脚本
└── postrm               # 卸载后维护脚本
```

### 9.2 control 要点

```text
Source: panshi-manager
Section: admin
Priority: optional
Build-Depends: debhelper-compat (= 13), cmake, pkg-config,
 qt6-base-dev, qt6-tools-dev, libdtk6core-dev, libdtk6gui-dev,
 libdtk6widget-dev, systemd
Standards-Version: 4.6.2
Rules-Requires-Root: no

Package: panshi-manager
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}
Description: Deepin 25 磐石系统管理工具
 （续行描述，'.' 起行为段落分隔）
```

- `Build-Depends`：**`systemd` 必须显式列出**（systemd 用户单元文件打包需要，官方 gotchas 点）；
- `Depends` 用 `${shlibs:Depends}` 占位，由 `dpkg-shlibdeps` 在打包时自动计算（见 9.6）；
- `Maintainer` 为开发者真实姓名与邮箱（1.0.4 起为 `Feelup <testangel@163.com>`，投稿审核会核对）；

### 9.3 rules 要点（dh + cmake + 加固）

```makefile
DPKG_EXPORT_BUILDFLAGS = 1
include /usr/share/dpkg/default.mk

export DEB_BUILD_MAINT_OPTIONS = hardening=+all
export DEB_CXXFLAGS_MAINT_APPEND = -Wall
export DEB_LDFLAGS_MAINT_APPEND = -Wl,--as-needed -Wl,-z,relro \
  -Wl,-z,now -Wl,-z,noexecstack -Wl,-E

%:
    dh $@ --buildsystem=cmake

override_dh_auto_configure:
    dh_auto_configure -- \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_SKIP_BUILD_RPATH=ON
```

- 全量 hardening + 常见链接加固（relro/now/noexecstack）；
- **`CMAKE_INSTALL_PREFIX` 必须是 `/usr`**：DConfig schema 的查找路径在 DSG 里**硬编码**为 `/usr/share/dsg`（`DSG_DATA_DIRS` 等环境变量一律不生效），装到 `/usr/local/share/dsg/configs` 时运行时必然报 `DConfig is invalid`、配置读写退化为默认值；
- `CMAKE_SKIP_BUILD_RPATH=ON` 避免构建目录绝对路径被打进产物（与官方示例一致）。

### 9.4 desktop 入口（deepin 特有字段）

```ini
[Desktop Entry]
Type=Application
Name=磐石设置
Name[en_US]=Panshi Manager
Comment=Deepin 25 磐石系统管理工具
Comment[en_US]=Deepin 25 immutable system management tool
Exec=/usr/bin/panshi-manager
Icon=panshi
Terminal=false
Categories=System;
Keywords=immutable;ostree;snapshot;panshi;
X-Deepin-Vendor=deepin
```

- `X-Deepin-Vendor=deepin`：deepin 商店识别标志；
- `Categories` **只保留一个主分类** `System;`——实测 1.0.3 曾含 `System;Settings;` 两个主分类，`desktop-file-validate` 提示应用可能在启动器中出现多次；
- `Icon=panshi` 与安装到 `share/icons/hicolor/scalable/apps/panshi.svg` 的图标一一对应；
- `Exec` 经 CMake `configure_file` 按 `@PANSHI_BINDIR@` 占位符生成（打包镜像 `/usr`、磐石只读系统用户级 `/usr/local` 两种形态都正确）。

### 9.5 copyright / changelog / postinst / postrm

- `copyright`：机器可读格式（`Format: https://www.debian.org/...`），`Files: *` + GPL-3+ 许可证全文指向 `/usr/share/common-licenses/GPL-3`；
- `changelog`：条目格式 `panshi-manager (1.0.4) unstable; urgency=medium`，签名行日期为 RFC 5322（`Tue, 06 Oct 2026 22:35:00 +0800`）；
- `postinst` / `postrm`：**不**尝试重载 systemd 用户单元，原因（2026-10-04 实测）：
  1. `systemctl --global daemon-reload` 是**无效命令**（systemd 255 输出 `--global is not supported for this operation.`，退出码 1）；
  2. 即使换成 `systemctl --user daemon-reload` 也没用——dpkg 以 root 运行，连不上登录用户的会话总线（`DBUS_SESSION_BUS_ADDRESS` 指向用户自己的 `/run/user/<uid>/bus`）。
  → 用户单元只能由**应用进程在用户会话内**重载（`DiskUsagePage::ensureMonitorUnitLoaded()` 已实现）。postinst/postrm 只做 `update-icon-caches` 与 `update-desktop-database`（兼容纯净系统与手动 `dpkg -i` 场景）。

### 9.6 依赖计算：dpkg-shlibdeps

二进制包的 `Depends` 由 `dpkg-shlibdeps` 从 ELF 的 `DT_NEEDED` 自动推导，**不要手写**：

```bash
dpkg-shlibdeps -O /path/to/panshi-manager
# 输出（1.0.4 实测）：
# libc6 (>= 2.36), libdtk6core (>= 6.7.47.1), libdtk6gui (>= 6.7.47),
# libdtk6widget (>= 6.7.47.1), libgcc-s1 (>= 4.2), libgl1,
# libqt6core6 (>= 6.8.0), libqt6dbus6 (>= 6.1.2),
# libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.3.0)
```

注意：**主程序并不直接链接 `Qt6::Svg`**（`libQt6Svg` 只在探针里用），因此依赖里**没有 `libqt6svg6`**——老包 1.0.0~1.0.3 手写的依赖列表含 `libqt6svg6`，1.0.4 起改为 dpkg-shlibdeps 计算值，更精确。

### 9.7 实际打包命令（1.0.4 流程，免 root）

```bash
# 1. 构建（需开发机有完整工具链；无 g++ 时可用既有 build/ 产物）
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"

# 2. 按 /usr 布局安装到打包暂存目录（--prefix 覆盖前缀，不写系统）
cmake --install build --prefix /tmp/panshi-stage/usr

# 3. 组装 DEBIAN/ 控制信息
#    control（Version、Depends=dpkg-shlibdeps 结果）
#    postinst / postrm（chmod 755）
(cd /tmp/panshi-stage && find usr -type f -printf '%P\n' | sort \
  | xargs -I{} md5sum "usr/{}" | sed 's|  usr/|  |' > DEBIAN/md5sums)

# 4. 生成 deb（--root-owner-group 免 root 强制 root:root 属主）
dpkg-deb --build --root-owner-group /tmp/panshi-stage panshi-manager_1.0.4_amd64.deb
```

### 9.8 发布前校验清单

```bash
dpkg-deb -I panshi-manager_1.0.4_amd64.deb          # 元信息：版本/依赖/描述
dpkg-deb -c panshi-manager_1.0.4_amd64.deb          # 内容：root:root、bin 755、其余 644
desktop-file-validate <解包>/usr/share/applications/org.deepin.panshi-manager.desktop  # 退出码 0
python3 -m json.tool <解包>/usr/share/dsg/configs/.../org.deepin.panshi-manager.json  # schema 合法
sh -n DEBIAN/postinst && sh -n DEBIAN/postrm        # 维护脚本语法
(cd <解包> && md5sum -c DEBIAN/md5sums)             # 校验和一致
grep -r "/usr/local" <解包>/usr/ || echo "无 /usr/local 残留"  # 全部 /usr 布局
```

### 9.9 商店投稿注意事项

- `Maintainer`、`copyright` 已填写开发者真实信息（`Feelup <testangel@163.com>` / `Copyright: 2026 Feelup`）；
- 版本号语义化推进（1.0.0 → 1.0.4），每个版本对应 changelog 条目；
- 包名沿用反向域名惯例 `org.deepin.panshi-manager`（desktop 文件名与 DConfig appid 一致）；
- 若在磐石只读系统上自测：`/usr` 是 usr-overlay 只读挂载、`/usr/local` 才是独立 rw ext4——本包面向打包镜像装 `/usr`，本地验证用 `dpkg -i` 或解包比对即可。

## 10. 版本发布流程

### 10.1 changelog 条目写法

```text
panshi-manager (1.0.4) unstable; urgency=medium

  * 修复弹窗「透明 + 无响应」：disableDialogTransparency() 关闭 DTK 窗口模糊后，
    DDialog 的半透明背景直接透出页面文字……
  * 「开启保护」区域收敛为唯一入口……
  * 修复内容区点击失效回归……

 -- Feelup <testangel@163.com>  Tue, 06 Oct 2026 22:35:00 +0800
```

要点：每条变更**写清现象、根因、修法**；签名行日期用 RFC 5322 格式（`dpkg-parsechangelog` 可校验）；新条目放在文件顶部。

### 10.2 发布步骤

1. 代码改动完成，`CMakeLists.txt` 若涉及版本号同步更新；
2. 更新 `debian/changelog`（新增顶部条目）；
3. 干净全量构建：0 error 0 warning；
4. 探针全绿（§8.1 列表逐一运行）；
5. i18n 无 unfinished 条目；
6. 按 §9.7 刷新暂存目录、打包；
7. 按 §9.8 校验清单逐项通过；
8. 产出 `panshi-manager_<版本>_amd64.deb` 提交商店。

## 11. 常见问题与经验教训

（均来自项目实测，附日期；每条对应一段可回溯的根因记录）

1. **DConfig 键缺 `permissions` 字段 = 只读**（1.0.1）：schema 键未标 `readwrite` 时写入被 daemon 静默拒绝（set 退出码 0 但读回仍是默认值），表现为开关弹回「关」。全部 16 个键必须显式 `permissions: "readwrite"`。
2. **DTitlebar 拖动信号在宏未定义时不编译**：`dtitlebar.h` 把 `mousePosPressed/Moving` 包在 `#ifdef DTK_TITLE_DRAG_WINDOW` 里，本工程编译时宏未定义 → X11 拖拽只能靠 dxcb 的 `DNoTitlebarWindowHelper`（只对无标题栏窗口安装钩子）。「CSD 开关」与「点击可用」曾表现为互斥，实为钩子行为，勿再假定二选一。
3. **禁用状态沿对象树传播**（2026-10-06 点击失效根因）：弹窗以页面为父对象时，冻结内容区的 `setEnabled(false)` 会沿父链把弹窗自身也禁用 → 悬停/点击/按键全部被忽略。修复：弹窗 Show 前重挂到顶层窗口、置 `Qt::Window` 标志。
4. **DDialog 半透明背景依赖窗口模糊垫底**（2026-10-06 弹窗透明根因）：`DPlatformHandle::setEnableBlurWindow(false)` 关掉模糊后，半透明底色直接透出页面文字；补 `setAutoFillBackground` 无效（DDialog 背景由 DStyle 专用绘制路径覆盖）。修复：不再对抗 DTK 渲染，保留原生模糊。
5. **`qApp->setPalette()` 会钉死/冻结主题**：DTK6 对应用级 setPalette 明确报警告；实测它让 `setPaletteType()` 整体失效且**不可逆**（切回亮色后 Base 永久卡暗色）。颜色一律经 `DPalette` 角色运行时取值。
6. **`qt_add_translations` 的 `RESOURCE_PREFIX` 对 DTK 无效**：DTK `loadTranslator()` 只搜文件系统、不读 Qt 内嵌资源；且该命令**不自动生成 install 规则**。必须 `QM_FILES_OUTPUT_VARIABLE` 取出后显式 install（见 §7.2）。
7. **`CMAKE_INSTALL_PREFIX` 非 `/usr` 时 DConfig 静默失效**：DSG 查找路径硬编码 `/usr/share/dsg`，环境变量无效。打包必须 `-DCMAKE_INSTALL_PREFIX=/usr`，否则磁盘监控配置读写退化为默认值且无任何报错。
8. **磐石系统 `/usr` 只读**：`/usr/local` 才是独立 rw；用户级安装必须用 `sed` 把 desktop/service 里的 `@PANSHI_BINDIR@` 替换成用户级 bin 绝对路径，否则开始菜单点击落到不存在的 `/usr/bin/panshi-manager`。
9. **`systemctl --global daemon-reload` 是无效命令**（systemd 255）：输出 `--global is not supported` 退出码 1；且 root 连不上用户会话总线。用户单元只能在应用进程内（用户会话）重载。
10. **探针判据必须含反例**：`dragmodalprobe` 的场景 2 必须 FAIL、场景 4 必须 PASS 才说明探针有效；`clippprobe` 禁止用「兄弟控件几何相交」判据（wordWrap 下控件矩形不相交，是假阴性）。全绿 ≠ 可信，判据本身要有灵敏度。
11. **QSS 只承担布局、禁止语义色插值**：`style.h` 仅保留 5 个结构性颜色；语义色（Muted/Success/Warning/Danger）一律走 `DPalette`，禁止 hex 色值与 QSS 属性选择器染色。
12. **`dpkg-deb --build --root-owner-group` 免 root 生成 root 属主包**：无 root 环境也能产出属主与权限完全合规的 deb（1.0.4 即此方式产出）。
13. **成功类反馈 0 次模态弹窗**：成功用页内 Banner（`autoDismiss` 单发定时器 + `setText` 换内容即停），失败/警告常驻可关；`autoDismiss` 每次调用若 `new QTimer` 会叠加误杀新 Banner。
14. **`sudo -n` 失败回退 `pkexec`**：特权命令先试免密、失败弹 polkit 授权框；`verify-unlocked.sh` 的密码从 `SUDO_PASS` 环境变量取，不写进命令历史。

## 参考资料

1. README.md - `/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置/README.md`
2. CMakeLists.txt - `/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置/CMakeLists.txt`
3. 开发文档.md - `/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置/docs/开发文档.md`（字段级参考手册）
4. 开发指南.md - `/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置/docs/开发指南.md`（代码地图 / 扩展模板 / UI 红线 / 教训）
5. debian/ - `/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置/debian/`（打包规范文件，含 changelog 1.0.4）
6. 根因修复文档（2026-10-06）- `docs/弹窗透明与无响应-根因与修复-2026-10-06.md`、`docs/点击失效-根因与修复-2026-10-06.md`、`docs/拖拽与语言切换-根因与现状-2026-10-06.md`
7. 验证脚本 - `verify-runtime.sh`、`verify-unlocked.sh`
8. 设计文档 - `docs/disk-usage-design.md`、`docs/UI-重构设计方案.md`、`docs/UI-重构进度-阶段*.md`