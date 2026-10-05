# translations/ — 翻译资源（i18n）

本目录存放 **Qt Linguist** 翻译源文件（`.ts`）。构建时由 CMake 的
`qt_add_translations()`（见根目录 `CMakeLists.txt`）自动调用 `lrelease`
生成 `.qm` 并安装。

## 命名约定（必须遵守）

| 规则 | 原因 |
|---|---|
| basename 必须是 `panshi-manager` | `DApplication::loadTranslator()` 按 `applicationName`（= `panshi-manager`）在标准翻译目录查找 `.qm`；名字不一致会**静默找不到**（无报错，只是不翻译） |
| 语言后缀用 Qt 规范 `<lang>_<REGION>` | 如 `zh_CN`、`en_US`；`lrelease` 与 DTK 的 locale 匹配都依赖它 |
| 源语言 = 中文 | 代码里的 `tr("…")` 源串就是中文，因此 `zh_CN` 是**恒等翻译**（`<translation>` 与 `<source>` 相同），`en_US` 才是真正的译文 |

当前文件：

- `panshi-manager_zh_CN.ts` —— 恒等（保证中文环境下按键位/文案完全稳定，且给
  `lupdate` 一个可增量更新的基线）
- `panshi-manager_en_US.ts` —— 英文译文

## 如何更新

`.ts` 是 `lupdate` 的**产物**，正常情况下**不要手改**（手改的条目在下次
`lupdate` 时可能被覆盖或标为 obsolete）。标准流程：

```bash
# 1. 安装工具（开发机）
sudo apt install -y qt6-tools-dev-tools      # 提供 lupdate / lrelease
sudo apt install -y qttools6-dev-tools       # 部分发行版包名不同，择一

# 2. 从源码提取/更新 .ts（保留已有译文，新增条目留空待译）
lupdate src/ -ts translations/panshi-manager_zh_CN.ts \
                translations/panshi-manager_en_US.ts

# 3. 用 Qt Linguist 编辑译文（可选，也可以直接编辑 .ts XML）
linguist translations/panshi-manager_en_US.ts

# 4. 构建时 CMake 会自动 lrelease → .qm 并安装（无需手动 lrelease）
```

CMake 侧还提供了 `update_translations` 目标（`qt_add_translations` 自带）：

```bash
cmake --build build --target update_translations   # 等价于 lupdate
```

## 安装位置

`qt_add_translations()` 会把 `.qm` 安装到 CMake 的**翻译目录**（
`${CMAKE_INSTALL_PREFIX}/share/panshi-manager/translations/` 之类的标准位置）。
`DApplication::loadTranslator()` 会扫描这些目录。

> ⚠️ 若手工安装（`install.sh` 的用户级分支），需同步把 `build/*.qm`
> 拷到 `$PREFIX/share/panshi-manager/translations/`，否则用户级安装下不翻译。
> 见根目录 `install.sh`。

## 验证翻译是否生效（两个会导致假阴性的坑）

复测实测确认，以下两点都会让「翻译看起来没生效」而实际是**环境问题**：

| 坑 | 现象 | 正解 |
|---|---|---|
| **`LANGUAGE` 优先级** | deepin 会话进程默认继承 `LANGUAGE=zh_CN`；gettext 优先级下它**压过** `LANG`/`LC_ALL`，只设后两者无效 | 三个都设：`LANGUAGE=en_US.UTF-8 LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8` |
| **`.qm` 查找路径** | `build/` **根目录不在** `loadTranslator()` 的查找列表；`.qm` 放那里等于没放 | 装到 `~/.local/share/panshi-manager/translations/`（或 `cmake --install`），再启动 |

```bash
mkdir -p ~/.local/share/panshi-manager/translations
cp build/*.qm ~/.local/share/panshi-manager/translations/
LANGUAGE=en_US.UTF-8 LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 \
  QT_QPA_PLATFORMTHEME=deepin ./build/panshi-manager
```

**告警行为不对称**（排障时别指望日志）：`zh_CN`（源语言）缺 `.qm` 会告警
`can not find qm files for locales QList("zh_CN")`，而 `en_US`（目标语言）
缺 `.qm` **完全静默**。

> ⚠️ **改了 `.ts` 之后必须重新 `cp` 到上面那个目录**（2026-10-02 实测踩过）。
> 助手改完 `.ts` → 你构建出新的 `build/*.qm`，但 `~/.local/share/…/translations/`
> 里还是**旧的** —— DTK 只从安装目录加载，于是「补译根本没生效」。
> 复现症状：新 `.qm` 明显变大（如 7KB → 17KB）但界面**一字未变**。
> **判断法：先比对两处 `.qm` 的时间戳/大小，再怀疑翻译内容写错。**

## 手写 `.ts` 的三个坑（`lupdate` 不会踩）

1. **XML 注释内禁止连续 `--`**：用 `-----` 作注释分隔线会让 `lrelease`
   报 `Parse error`、**`.qm` 根本生成不出来**。手写注释分隔线一律用 `=`。
2. **`.ts` 里的 `\n` 是字面反斜杠+n，不是换行** ⚠️（2026-10-02 实测踩过）。
   `lrelease` **不做转义解释**：`.ts` 里写 `<source>a\nb</source>`，
   运行时 `tr("a\nb")` 传入的**是真换行**，两者哈希不同 → **永远匹配不上**，
   该条目静默回落源串（表现为「补了翻译但 tooltip 仍是中文」）。
   正解：**`.ts` 里必须写真实换行**（XML 里直接换行，或用 `&#10;`）。
   受影响的典型串：多行 `tr("…\n…")`（tooltip / 折叠区长文 / 监控状态行）。
3. **`context` 名要贴近 `lupdate` 的推断**：`lupdate` 按类名 / `QObject::tr`
   推 context。首跑 `lupdate` 后 context 会与手写版并存或被替换，属预期行为
   （`lupdate` 接管后条目会大幅增加）。

> **第 2、3 条的共同根源**：手写 `.ts` 与 `lupdate` 产物**不是同一种东西**。
> 手写版靠「字符串精确匹配」，对 context 名与转义形式都敏感。
> → **强烈建议：不要持续手工维护 `.ts`，首轮跑一次 `lupdate` 出全量骨架**，
> 之后只填 `<translation>` 内容。漏页在骨架里只是「空条目」（回落中文、不空白），
> 而手工 `.ts` 漏页是「整个 context 不存在」（该页全中文且无任何提示）。

## 一条排查「补译不生效」的固定顺序

按顺序排除，**不要跳步**：

1. **`.qm` 同步了吗** → 比对 `build/*.qm` 与 `~/.local/share/panshi-manager/translations/*.qm`
   的时间戳与大小（最常见原因，见上）
2. **locale 设对了吗** → 三个变量都设，尤其 `LANGUAGE`（见上）
3. **context 名匹配吗** → 同一串在别的 context 里**不算**已翻译
   （例：`创建备份` 原先只在 `PolicyPage` context，而实际调用方是
   `VersionManagementPage` / `SnapshotsPage` → 看似有译文却不生效）
4. **`.ts` 里是字面 `\n` 吗** → 见上手写坑 2
5. **代码里是 `QStringLiteral("中文")` 吗** → 那不是 `tr()`，`lupdate` 永远扫不到。
   本项目实例：`手动备份-`、`回滚前备份-` 两个备份名前缀。

## CMake 侧不可省的组件

`find_package(Qt6 … COMPONENTS … LinguistTools)` **必须包含 `LinguistTools`** ——
缺它则 `qt_add_translations` 未定义，configure 阶段报 `Unknown CMake command`。
实测于 Qt 6.8.0：该命令仍定义在 `Qt6LinguistTools` 包内，**不随「Qt 6.7 起
并入 Core」的官方变更而移动**。

## 已翻译范围

| 范围 | 状态 |
|---|---|
| 应用身份 / 窗口标题 / 侧栏 / 设置搜索 / 策略与设置页 / 关于页 | ✅ 已复测通过（2026-10-02 T1~T7 / D1） |
| **备份与版本页（页头、2 标签、6 表头、徽章、策略条、创建/删除/还原三套对话框、全部 Banner）** | ✅ 已复测通过（2026-10-02 V-19/V-20，0 漏译） |
| 磁盘监控页长文案 / 各页状态卡动态文案 / `--monitor` 进程文本 | ❌ 未译（回落中文） |

未译条目在运行时会**回落到源串（中文）**，不会显示空白 —— 机制先合入、译文增量补。

