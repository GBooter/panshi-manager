# UI 重构进度 · 附：i18n（翻译）接入

> 承接阶段 5 文档 §七决策 1「是否引入 i18n 机制（`translations/` + CMake
> `qt_add_translations`）→ 答：**引入**」。本轮把该决策落地。
> 完成日期：2026-10-01　·　复测日期：2026-10-02
> **状态：已复测通过**（T1~T7 / I2 / D1 全过；I1 环境不可达；
> C3/I3 本机 `lrelease` 在位无法构造，静态确认；编译代修 2 处）。

---

## 一、为什么需要（问题的两层）

**第一层：机制完全缺失。** 虽然 README 与开发文档都写着「全部文案已用 `tr()` 包裹」，
但仓库里：

| 项 | 改造前 |
|---|---|
| `CMakeLists.txt` | **零翻译配置** —— 没有任何 `TRANSLATIONS` / `qt_add_translations` |
| `translations/` | **目录不存在** |
| `.qm` 安装 | 不存在 —— 即使手工 `lrelease` 也不会被安装 |

也就是说 `tr()` 只是「为将来预留」，**整条链路从未接通**：没有 `.ts` 就没有 `.qm`，
没有安装规则 `loadTranslator()` 也无从查找。

**第二层：两处真实缺陷，切换语言后必然失效。**

1. **`main.cpp` 的 `tr()` 时序倒置**。`setProductName` /
   `setApplicationDisplayName` / `setApplicationDescription` 三处
   `QObject::tr()` 排在 `app.loadTranslator()` **之前**：
   ```cpp
   app.setApplicationDisplayName(QObject::tr("磐石设置"));   // ← 第 69 行
   app.setProductName(QObject::tr("磐石设置"));               // ← 第 71 行
   app.setApplicationDescription(QObject::tr("…"));           // ← 第 73 行
   …
   app.loadTranslator();                                      // ← 第 106 行
   ```
   DTK 官方文档明确要求 `loadTranslator()` 必须在**创建任何可翻译对象之前**
   调用（`references/gotchas.md` §7.2、`references/widgets/application.md` §12）。
   在装载前取译文 → 拿到的是源串（中文）→ 英文 locale 下产品名/描述仍是中文。

2. **`aboutpage.cpp` 裸中文字面量**。复制版本信息时：
   ```cpp
   QApplication::clipboard()->setText(
       QStringLiteral("磐石设置  ") + versionLine);   // ← 未包 tr()
   ```
   同一 lambda 内紧邻的两处反而用了 `QObject::tr("已复制")` /
   `QObject::tr("复制版本信息")` —— 说明是**遗漏**而非有意。

---

## 二、机制设计（三条硬约束）

### 2.1 `.qm` 的 basename 必须等于 `applicationName`

`DApplication::loadTranslator()` 的查找逻辑是「按 `applicationName` 在标准翻译
目录中拼文件名」。本项目 `app.setApplicationName("panshi-manager")`，因此
`.qm` 必须叫 `panshi-manager_<locale>.qm`。

**违反的后果是静默的** —— 查找失败不报错、不警告，只是不翻译。这类缺陷在
中文环境下完全看不出来（源串本来就是中文），只有切到英文才暴露。

`qt_add_translations()` 会以传入的 target 名（`panshi-manager`）作为
`.qm` basename，天然满足该约束 —— 这也是选它而非手写 `qt_add_lrelease()` 的原因。

### 2.2 `loadTranslator()` 必须早于一切可翻译对象

见 §一第 2 层第 1 点。修正方式是把三个 setter 移到 `loadTranslator()` 之后。

**注意顺序细节**：`DLogManager::registerConsoleAppender()` 等日志注册与翻译
无关，可以留在原位；但把 `loadTranslator()` 提到 setter 之前后，
`setApplicationName`（决定 `.qm` basename）**必须仍在 `loadTranslator()` 之前**
—— 否则查找用的名字还没设定。最终顺序：

```
DApplication app
  ├─ setOrganizationName / setApplicationName / setApplicationVersion   ← 身份先立
  ├─ DLogManager 注册
  ├─ app.loadTranslator()                                              ← 装载译文
  └─ setApplicationDisplayName / setProductName / setProductIcon /
     setApplicationDescription / setApplicationLicense                 ← 可翻译对象
```

### 2.3 源语言是中文 —— `zh_CN` 是恒等翻译

代码里的 `tr()` 源串就是中文，这带来一个非典型但不罕见的情形：
**`zh_CN.ts` 的 `<translation>` 与 `<source>` 完全相同**。

保留它的价值有三：

1. `lupdate` 需要一个可增量更新的基线（新增条目追加、消失条目标 obsolete）；
2. 显式装载 `zh_CN` 时行为不依赖「找不到就回落」这条隐式路径；
3. 作为 `en_US` 的条目对照表，便于校对漏译。

**未译条目在运行期回落到源串（中文），不会显示空白** —— 这决定了本轮可以
「先合入机制、再增量补译」：先覆盖真正的对外可见文案，其余留给后续。

---

## 三、改动清单

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt` | 新增 `PANSHI_TS_FILES` 声明 + `qt_add_translations(panshi-manager TS_FILES …)`（含 `LUPDATE_OPTIONS -no-obsolete`、`RESOURCE_PREFIX "/i18n"`）。整体含 3 段说明注释（basename 约束 / 源语言 / 缺 `.ts` 时的行为） |
| `src/main.cpp` | 三处 `QObject::tr()` setter 从 `loadTranslator()` **之前**移到**之后**；补时序注释锚点 |
| `src/pages/aboutpage.cpp` | `QStringLiteral("磐石设置  ")` → `QObject::tr("磐石设置  ")` |
| `translations/panshi-manager_zh_CN.ts` | **新建**（恒等翻译） |
| `translations/panshi-manager_en_US.ts` | **新建**（真实译文） |
| `translations/README.md` | **新建**（命名约定 / 维护流程 / 已译范围 / 安装位置） |
| `install.sh` | 用户级分支：建 `$PREFIX/share/panshi-manager/translations/` 并拷 `build/*.qm`；`.qm` 缺失时给提示而非报错 |
| `docs/开发文档.md` | §9 重写（机制 / 硬约束 / 维护流程 / 桌面文件）；§2.3 安装内容补翻译；§3 目录树补 `translations/`；§11「是否引入 i18n 机制待定」→ 已接入 |
| `README.md` | 差异表新增「国际化」行；目录树补 `translations/`；构建命令补可选工具与说明 |

---

## 四、首轮翻译范围

`en_US.ts` 覆盖（按用途分组）：

| 组 | 内容 |
|---|---|
| 应用身份 | 产品名、应用描述、版本行 |
| 外壳 | 窗口标题、侧栏 3 个分组 + 7 个导航项、设置搜索框占位符与空结果提示 |
| 设置搜索 | 群组名（备份保留策略 / 磐石系统参数） |
| 策略与设置页 | 页头、2 个分组标题与描述、6 行设置的标题与描述、2 个下拉选项、保存按钮、3 条过程中/结果文案（含 `%1` 占位符） |
| 关于页 | 页头、名称、版本行、2 个折叠区标题、复制按钮与「已复制」 |
| 通用 | 「确定」/「取消」 |

**有意未译（首轮）**：各页状态卡的动态文案（含大量数值拼接与 `%1` 组合）、
磁盘监控页的告警/统计长文案、`--monitor` 进程文本。原因：这些是
`lupdate` 首次运行才会完整进入 `.ts` 的条目，且数量大、需要结合界面截图校对。
回落机制保证它们仍显示中文而非空白。

---

## 五、复测结果（2026-10-02）

### 5.0 总览

| 清单 | 结果 |
|---|---|
| C1 cmake 配置 | ✅ 代修 1 处后通过 |
| C2 `build/*.qm` | ✅ `panshi-manager_zh_CN.qm` 4845 B + `panshi-manager_en_US.qm` 7262 B |
| C3 lrelease 缺失分支 | ⚠️ 本机 `lrelease` 在位，**无法构造缺失**；`install.sh` 分支逻辑静态确认 |
| C4 编译 0 error / 0 warning | ✅ |
| T1 标题栏 | ✅ `Panshi Manager` |
| T2 侧栏 | ✅ 3 分组 + 7 导航项全英文 |
| T3 设置搜索 | ✅ `Search settings` + `No matching se…` |
| T4 策略页 | ✅ 页面/行/下拉/按钮全英文 + Banner 英文 |
| T5 关于页 + 剪贴板 | ✅ 剪贴板首段 `Panshi Manager  Version 1.0.0  ·  Deepin 25 immutable system management tool` |
| T6 磁盘页回落 | ✅ 中文完整、无空白、无 `%1` 残留 |
| T7 中文回归 | ✅ 逐字一致，`zh_CN` 恒等 `.qm` 装载无告警 |
| I1 系统级安装 | ⛔ 无免密 sudo，不可达 |
| I2 用户级 `install.sh` | ✅ 路径实测生效（见 §5.3） |
| I3 未装 lrelease 不中断 | ⚠️ 同 C3 |
| D1 `.desktop` | ✅ `Name=磐石设置` + `Name[en_US]=Panshi Manager` 齐备 |

### 5.1 编译代修 2 处（均有注释）

**① `CMakeLists.txt`：`find_package` 补 `LinguistTools` 组件**

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg DBus LinguistTools)
```

缺该组件时 configure 阶段报 `Unknown CMake command` —— **正是 §七风险 1 预想的
失败形态，但根因不是 Qt 版本门槛**：本机 Qt **6.8.0**，`qt_add_translations`
仍定义在 `Qt6LinguistTools` 包内（`Qt6CoreMacros` 不含），与「Qt 6.7 起并入 Core」
的官方变更不符。→ 教训：**不要凭官方文档推断发行版打包**，须以实测为准；
`Unknown CMake command` 优先怀疑 `find_package` 缺组件，而非 Qt 太老。

**② 两个 `.ts` 的头部注释：XML 注释体内禁止连续 `--`**

原注释用 `-----` 作分隔线，直接导致 `lrelease` 报 `Parse error 6:5`，
**两个文件双双编译不出 `.qm`**。改为 `=====`。

→ 教训：**手写 `.ts` 的规范坑**（`lupdate` 生成的文件不会踩）：
XML 注释 `--` 内部不允许再出现 `--`。手写 `.ts` 的注释分隔线一律用 `=`。

### 5.2 关键实测：`.qm` 查找路径与语言判定（本轮最有价值的补充）

原 §5.2 给的验证命令在本机会**静默失败**，两个原因均已实证：

**① 语言判定 —— deepin 会话进程默认继承 `LANGUAGE=zh_CN`**

gettext 优先级下 `LANGUAGE` **优先于** `LANG` / `LC_ALL`，
所以只设 `LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8` 无效。正确命令：

```bash
LANGUAGE=en_US.UTF-8 LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 \
  QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager
```

> 已在 §8.2 更新复测命令。**下轮复测务必带 `LANGUAGE`**，否则会误判「翻译没生效」。

**② 查找路径 —— `build/` 根目录不在 `loadTranslator()` 的查找列表**

`.qm` 放在 `build/` 根时**英文界面完全无效**；放到
`~/.local/share/panshi-manager/translations/` 后**立即生效**（与 `install.sh`
用户级分支建的路径一致）。

> **因此「构建目录直接启动看不到翻译」是预期行为**，不是缺陷。
> 复测前需先 `./install.sh` 或手动拷贝 `.qm` 到上述目录。

**③ 告警行为不对称（坐实 §2.1）**

| locale | `.qm` 缺失时的行为 |
|---|---|
| `zh_CN`（源语言） | **告警**：`can not find qm files for locales QList("zh_CN")` |
| `en_US`（目标语言） | **完全静默** |

→ 原文「静默不翻译」的约束**对目标语言成立**，源语言反而有告警。
实践含义：**排查时不要指望日志**，只能看界面；反之若看到 `zh_CN` 告警，
说明源语言 `.qm` 也没装上。

### 5.3 安装

- **I1** ⛔ 无免密 sudo，不可达。
- **I2** ✅ 用户级路径 `~/.local/share/panshi-manager/translations/` 实测生效
  —— 与 §5.2 ② 的手工拷贝是同一条路径，等效验证通过。
- **I3** ⚠️ 同 C3：`lrelease` 在位无法构造缺失；`install.sh` 的
  `compgen -G "build/*.qm"` 分支逻辑已静态确认（无 `.qm` 时走 else 分支打印提示、
  不 `exit`、不 `set -e` 中断）。

> **测试遗留**：`~/.local/share/panshi-manager/translations/` 下留有两枚 `.qm`
> （I2 验证产物）。手工复现英文启动可直接用；验证完可删。

---

## 六、附带收获（超出本轮范围但已验证）

**设置搜索的 `tr()` 快照机制在英文下自洽** —— 英文界面下搜索 `"x"`
命中 `Max backup de…`。这实证了 §七 中「`SettingEntry` /
`registerSearchTarget` 的 key 是运行期快照，因不做运行时语言切换而不阻塞
i18n 接入」的判断。

---

## 七、意外发现（非本轮改动引入，但修正了一条长期结论）

**策略页「保存系统参数」实际走 pkexec 提权，本机会弹出真授权框。**

复测中用户看到真实的 polkit 授权对话框（已取消、未输入密码）。
此前所有页面的 root 操作都是「sudo 直连失败 → Banner」，
**从不知道这条链能弹框**。

### 7.1 机制核实

`backend.cpp::runProcess` 的回退条件（已通读确认）：

```cpp
// 优先 sudo -n 免密
proc.start("sudo", { "-n", program, full });
…
// sudo 需要密码时回退 pkexec
if (!r.ok && asRoot && geteuid() != 0 && r.stderrText.contains("password")) {
    pk.start("pkexec", …);   // ← 阻塞等待 GUI 授权
}
```

即**回退的触发条件是「sudo 的 stderr 含 `password`」**。
`sudo -n` 在需要密码时输出的正是 `sudo: a password is required` → 命中回退。

**为什么只有策略页弹框、其它页不弹**：回退还要求 `r.stderrText` **含
`password` 字样**。其它页面的 sudo 失败原因不同（如「用户不在 sudoers 组」，
stderr 不含 `password`）→ **不满足回退条件，直接返回失败**，
于是只有策略页这条链会走到 pkexec。

### 7.2 对既有结论的修正

`MEMORY.md` §五 与开发文档 §10.2 记的是「本机后端命令走 sudo 且无免密
→ **任何开关/写操作必然失败**，成功分支无法端到端验证」。

**该结论需要修正为**：

| 层面 | 修正后 |
|---|---|
| **代码层面** | 提权链是 **`sudo -n` →（stderr 含 `password` 时）`pkexec`** 两级回退，**不是单级** |
| **本机 GUI 层面** | 凡 sudo 失败原因含 `password` 的操作，**会弹 polkit 授权框**；输入正确密码后**可以走通**成功分支 |
| **本机自动化层面** | 结论仍成立 —— 无免密时自动化脚本无法输入密码，**成功分支仍不可端到端自动验证** |
| **策略页** | **真机上「保存系统参数」是可通过 GUI 授权走通的** |

→ 实践含义：**「成功 Banner 自动消失」等长期挂起项，在真机上是可验证的**
（此前误以为不可达）。只需在本机触发对应操作、在授权框输入密码即可。
这为解锁一批挂起项提供了路径。

> ⚠️ 未在本轮实际输入密码验证成功分支（用户已取消授权框），
> 上述「可走通」是**基于代码链路推断 + 授权框确已弹出**这一实证，
> 严格的成功分支验证待补。

---

## 八、原待复测清单（存档，保留原始判断依据）

### 8.1 编译（首要，验证机制本身）

```bash
cd /media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

- [x] **C1** `cmake` 配置阶段**不报** `qt_add_translations` 相关错误
      —— ⚠️ 代修：需 `find_package(Qt6 … LinguistTools)`（见 §5.1 ①）
- [x] **C2** `qt_add_translations` 是否因 `.ts` 存在而生成 `.qm`：
      `ls build/*.qm` → `panshi-manager_zh_CN.qm` 4845 B、`panshi-manager_en_US.qm` 7262 B
- [ ] **C3** 若 `qt6-tools-dev-tools` 未安装 → `lrelease` 缺失时**行为可接受**
      —— ⚠️ 本机 `lrelease` 在位，**无法构造**；`install.sh` 分支逻辑静态确认
- [x] **C4** 构建 0 error / 0 warning（`main.cpp` 移动后无未使用变量/顺序告警）

### 8.2 翻译生效（真正的验收目标）

> ⚠️ **命令已按 §5.2 ② 实测修正**：`LANGUAGE` 必须显式设置，
> 且 `.qm` 必须先落到查找路径（`build/` 根目录不在查找列表内）。

```bash
# 系统级安装后（I1 本机不可达，此处仅存档）
sudo cmake --install build
find / -name 'panshi-manager_*.qm' 2>/dev/null      # 确认安装位置

# ① 先让 .qm 落到 DTK 查找路径（或直接 ./install.sh）
mkdir -p ~/.local/share/panshi-manager/translations
cp build/*.qm ~/.local/share/panshi-manager/translations/

# ② 强制英文 locale 启动（三个变量都要设，LANGUAGE 优先级最高）
LANGUAGE=en_US.UTF-8 LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 \
  QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager
```

- [x] **T1 标题栏**：`Panshi Manager`（而非「磐石设置」）—— 验证 `setApplicationDisplayName`/`setTitle` 时序修正生效
- [x] **T2 侧栏**：`Overview` / `Disk usage` / `System protection` / `Backups & versions` /
      `System update` / `Policies & settings` / `About`；分组 `Overview` /
      `System maintenance` / `Settings`
- [x] **T3 搜索框**占位符 `Search settings`；输无匹配串 → `No matching settings found`
- [x] **T4 策略页**：页头、两个分组标题、6 行标题/描述全英文；`Save system parameters` 按钮；
      点保存后的 Banner 文案英文
- [x] **T5 关于页**：`About` / `Panshi Manager` / 版本行英文 /
      `Immutable system technical reference` / `Common command reference`；
      **点「Copy version info」→ 剪贴板首段应是 `Panshi Manager  ` 而非「磐石设置」**
      （这一条直接验证 §一第 2 层第 2 点的修复）
- [x] **T6 未译条目回落**：进入磁盘空间页 → 状态卡文案仍为中文且**无空白/无 `%1` 残留**
- [x] **T7 中文环境不回归**：默认 locale 启动 → 与改造前**逐字一致**（`zh_CN` 恒等翻译 + 回落双重保证）

### 8.3 安装与打包

- [ ] **I1** 系统级 `sudo cmake --install build` → `.qm` 落到预期位置，`LANG=en_US` 下翻译生效
      —— ⛔ 无免密 sudo，不可达
- [x] **I2** 用户级 `./install.sh`（`/usr` 只读场景）→ `~/.local/share/panshi-manager/translations/`
      下有 `.qm`，且启动器拉起时翻译生效 —— ✅ 路径实测生效
- [ ] **I3** 未装 `qt6-tools-dev-tools` 时 `./install.sh` 不中断（走「跳过翻译」分支并给出提示）
      —— ⚠️ 同 C3，静态确认

### 8.4 桌面文件

- [x] **D1** 英文 locale 下启动器/应用名显示 `Panshi Manager`（`.desktop` 的 `Name[en_US]`）；
      中文下显示「磐石设置」。注意这走 `.desktop` 的 locale key，与 `.ts` 是**两套独立机制**，
      需**分别**确认 —— ✅ 两者齐备

---

## 九、与既有机制的关系

| 机制 | 关系 |
|---|---|
| **`SettingEntry` / `registerSearchTarget`** | key 是 `PanshiRow::title()` 的 `tr()` **运行期快照**。因为本项目**不做运行时语言切换**（切语言需重启应用），快照与索引在单次运行内自洽，**不阻塞** i18n 接入。若非要在运行中切语言，需重建索引 —— 列入「不做」原因。**✅ 已在英文界面实证**（见 §六）。 |
| **`PanshiUi::Semantic` / DPalette** | 无关。语义色与文案是两条正交的本地化维度。 |
| **`.desktop` 的 `Name[en_US]`** | 独立机制（XDG desktop entry 的 locale key），不由 `qt_add_translations` 生成，需手改同步。**✅ 复测确认两者齐备**。 |
| **`--monitor` 进程** | 用 `QCoreApplication` 且不调 `loadTranslator()`。该模式**无用户可见文案**（仅日志与通知文本）；后续若要翻译通知文案，需在此进程内**另建 QTranslator 手工装载**（`DApplication` 不可用）。 |

---

## 十、风险与已知取舍

1. ~~**`qt_add_translations` 的 CMake 版本门槛**~~ → ✅ **已实证并解决**（见 §5.1 ①）。
   真实门槛不是 Qt 版本，而是 **`find_package(Qt6 … COMPONENTS LinguistTools)`**。
   本机 Qt 6.8.0 的 `qt_add_translations` 仍在 `Qt6LinguistTools` 包内。
2. **`.ts` 是 `lupdate` 的产物**：本轮手工构造了初始 `.ts`（因此只含已覆盖的
   条目与手写 context）。首次跑 `lupdate` 后，条目会大幅增加且 context 名
   可能与手写的不一致（`lupdate` 用类名/`QObject::tr` 推 context）。
   **这是预期行为**：手写版的目的是「让机制立刻能跑通并有真实译文」，
   之后交给 `lupdate` 增量管理。已在 `translations/README.md` 写明。
   **另注意手写 `.ts` 的 XML 注释约束**：`--` 内不得再出现 `--`（见 §5.1 ②）。
3. **`RESOURCE_PREFIX "/i18n"`**：把 `.qm` 也嵌进可执行文件的 `:/i18n` 资源。
   **复测结论：主链路不需要它**（`loadTranslator()` 走文件系统查找，不读 `:/i18n`）。
   当前状态：**保留但未激活** —— 零成本（不占额外运行时路径），
   且为将来 `--monitor` 进程手工装载（见 §九）预留了资源。
   若日后确定要走 `QTranslator::load(":/i18n/…")` 路线，它就是必需的；
   若确定只走安装目录，可删。**不影响主链路，两种取舍都安全。**
4. **首轮译文未覆盖全部条目**：见 §四。回落机制保证不出现空白，但**英文界面
   会中英混排**（如 `Max backup de…` 这类截断显示），属预期的中间状态。
5. **`.qm` 查找路径不含 `build/` 根目录**（见 §5.2 ②）：开发期从构建目录直接
   启动看不到翻译是**预期行为**，不是缺陷。复测须先安装或手动拷贝。

---

## 十一、收尾状态

| 项 | 状态 |
|---|---|
| CMake 翻译配置（含 `LinguistTools`） | ✅ 完成并复测通过 |
| `translations/` 目录与 2 个 `.ts` | ✅ 完成并复测通过 |
| `main.cpp` 时序修正 | ✅ 完成并复测通过（T1/T5） |
| `aboutpage.cpp` 裸中文修正 | ✅ 完成并复测通过（T5） |
| `install.sh` 用户级拷贝 | ✅ 完成（I2 路径实测生效） |
| `开发文档.md` §9 / §11 / §3 / §2.3 | ✅ 完成 |
| `README.md` | ✅ 完成 |
| **编译验证** | ✅ C1~C4 通过（代修 2 处） |
| **实机翻译验证** | ✅ T1~T7 / D1 通过；I1 环境不可达；C3/I3 无法构造 |
| **提权链结论修正** | ✅ 已完成（见 §七） |


---

## 七、覆盖补齐与全量译文（2026-10-03 23:20）

**此前的状态**：`.ts` 严重滞后 —— context 只有 8 个（还是旧类名 `SnapshotsPage`/`DeploysPage`），
缺 `OverviewPage`/`ProtectionPage`/`UpdatePage`/`DiskUsagePage` 等重构后新增的页面；
英文界面 282 条未译。**当时判断需 `lupdate`，因工具链缺失标为「环境不可达」。**

### 现在的解法：`.toolchain/env.sh` + lupdate

`lupdate` 在 sysroot 里本来就有，但缺 `libclang-cpp.so.17` → 补装
`libclang-cpp17` / `libclang1-17` / `libllvm17` 即可（**不是 Qt 依赖问题**）。

```bash
. .toolchain/env.sh
# ⚠️ 必须传「目录 + -recursive」，传文件列表会报 has no recognized extension
lupdate -locations relative -noobsolete -recursive -extensions cpp,h \
  "$PWD/src" -ts translations/panshi-manager_zh_CN.ts translations/panshi-manager_en_US.ts
```

结果：**457 条源文（331 新增 / 126 已存在）**，context **8 → 19 个**。

### 译文写入：程序化替换，勿手写 XML

用 Python 按 `<message>` 块解析 → 命中译文表则替换 `<translation>`。
**这样避开了手写 `.ts` 的三个坑**（注释内禁 `--`、`\n` 不解释、context 名须匹配）。
注意查表前要 `re.sub(r'\s+',' ')` 归一化，否则多行源串匹配不上。

- `en_US`：**296 条新译 → 457 finished / 0 unfinished**
- `zh_CN`：源语言即中文，恒等翻译 → 去掉 331 处 `type="unfinished"` 标记

### 验证证据

| 项 | 结果 |
|---|---|
| XML 合法性 | `minidom.parse` 通过 |
| `lrelease` | 双语均 **457 finished and 0 unfinished** |
| `.qm` 体积 | 22KB/12KB → **63KB/34KB** |
| **启动告警** | **`can not find qm files` 消失**（此前 en_US 与 zh_CN 必现） |

最后一条是最强证据 —— 告警消失即 `.qm` 真的被 `loadTranslator()` 找到并加载。

> ⚠️ 验证 i18n 前必须 `cp build2/*.qm ~/.local/share/panshi-manager/translations/`：
> **构建目录不在 `loadTranslator()` 查找列表**，放在那里等于没放。
