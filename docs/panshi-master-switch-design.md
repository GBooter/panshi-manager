# 设计文档：磐石功能总开关

日期：2026-09-30
状态：已评审通过

## 1. 背景与目标

磐石设置（panshi-manager，DTK6/C++17/Qt6）需要新增"磐石功能"总开关，
让用户可以选择开启或关闭 deepin 25 的磐石只读保护。

系统机制约束（基于本机 deepin 25 实测）：

- `deepin-immutable-ctl` 1.0.39 无关闭整个磐石启动模式的官方命令；全盘 A/B 安装只能以磐石模式启动。
- 官方支持的"关闭磐石功能" = 可写层开关：`deepin-immutable-writable enable/disable`，
  与控制中心 → 开发者选项 → "磐石只读保护" 等价。
- 因此本开关语义为**磐石保护总开关**：
  - 开关 ON = 恢复只读保护（`writable disable`）
  - 开关 OFF = 关闭保护（`writable enable`），并区分两种模式：
    - **临时关闭**：重启后自动恢复只读（`enable -c`）
    - **完全关闭**：重启后仍保持关闭（`enable`，无 `-c`）——危险操作，需 30 秒倒计时警告

## 2. 实现方案（方案 A：页面内开关 + MainWindow 同步刷新）

用户已确认：

- 开关放在**总览页**与**系统保护页**两处，状态互相同步；
- 提供"完全关闭（重启后也关闭）"选项，用户选择时弹出 **30 秒警告倒计时**对话框，视觉需醒目美观。

### 2.1 文件改动清单

| 文件 | 改动 |
|---|---|
| `src/backend.h/cpp` | 无需改动：`getWritableStatus()` 已含 `enable`/`clearAfterReboot`；`enableWritable(clearAfterReboot, allDirs)`、`disableWritable()` 已覆盖全部动作 |
| 新增 `src/widgets/countdowndialog.h/cpp` | `PanshiCountdownDialog`：30 秒倒计时危险确认对话框，基于 `DAbstractDialog`（圆角 + 阴影，X11/Wayland 通用） |
| 新增 `src/pages/masterswitch.h/cpp` | 两页共享的开关逻辑：构造开关行控件、状态文案映射、"确认 → 后台执行 → 提示 → 刷新"流程 |
| `src/pages/overviewpage.cpp/h` | 页头下方插入"磐石功能"开关卡（徽章 + 标题 + 状态副标题 + DSwitchButton + "详情"跳转保护页） |
| `src/pages/protectionpage.cpp/h` | "保护开关"卡顶部插入同步开关行 |
| `src/mainwindow.h/cpp` | 新增 `refreshPanshiPages()`：切换成功后同时刷新总览页与保护页 |
| `CMakeLists.txt` | `PANSHI_SOURCES` 添加 2 个新源文件 |

### 2.2 开关交互流程

```
开关拨到 OFF
  └─ 选择对话框（DDialog）：「临时关闭(重启后自动恢复)」/「完全关闭(红色警告)」/「取消」
       ├─ 临时关闭 → 直接执行 enableWritable(clearAfterReboot=true, allDirs=true)
       │             （低风险，免双重确认）
       └─ 完全关闭 → PanshiCountdownDialog（30s 倒计时）
                     → 确认 → enableWritable(clearAfterReboot=false, allDirs=true)
开关拨到 ON
  └─ 普通确认对话框 → disableWritable() → 提示"重启后生效"
执行成功 → MainWindow::refreshPanshiPages() 两页同步刷新
执行失败 → errorDialog 显示 stderr（runProcess 已有 sudo -n → pkexec 回退）
```

### 2.3 PanshiCountdownDialog 视觉设计

- 顶部：QPainter 圆环进度动画（红色弧线随 30→0 递减），环心大号剩余秒数，旁置红色警告徽章；
- 标题：「即将完全关闭磐石保护」（DFontSizeManager T3）；
- 风险清单（图标 + 条目，wordWrap）：
  1. 核心目录将失去只读保护；
  2. 恶意软件与误操作可直接破坏系统；
  3. 重启后仍保持关闭，需手动重新开启保护。
- 底部按钮：取消（DPushButton）｜「我已知晓风险，完全关闭（30s）」
  —— DWarningButton，倒计时期间禁用并显示剩余秒数，倒计时结束启用；
- 亮暗主题自适应：DGuiApplicationHelper + DPalette，不硬编码字号；
- QTimer 1s tick 驱动；取消按钮任何时刻可点，返回 QDialog::Rejected。

### 2.4 状态文案映射

| writable.json 状态 | 开关位置 | 副标题文案 | 颜色 |
|---|---|---|---|
| `enable=false` | ON | 保护中 · 核心目录只读 | 绿 |
| `enable=true` + `clearAfterReboot=true` | OFF | 已临时关闭 · 重启后自动恢复 | 橙 |
| `enable=true` + `clearAfterReboot=false` | OFF | 已完全关闭 · 重启后仍保持关闭 | 红 |

状态查询复用 `getWritableStatus()`（页面 refresh() 时异步获取），无新增命令、无共享缓存。

### 2.5 错误处理

- 命令失败：复用现有 `CommandResult.errorMessage` → errorDialog 展示；
- 状态读取失败（writable.json 缺失/解析失败）：开关置灰并显示"未知状态"；
- 对话框期间禁止重复触发（模态）。

## 3. 验证计划

1. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"` 编译通过；
2. `QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager` 运行验证：
   - 总览页 / 保护页开关状态一致且与系统实际状态一致；
   - 开关 OFF → 选择对话框出现；选择"完全关闭" → 倒计时对话框出现，
     确认按钮 30s 内禁用、倒计时归零后启用；
   - 亮 / 暗主题下倒计时对话框观感正常；
   - "详情"链接正确跳转保护页。
3. 真实"完全关闭"操作不由开发过程自动触发（避免破坏当前系统保护状态），由用户自行决定执行。
