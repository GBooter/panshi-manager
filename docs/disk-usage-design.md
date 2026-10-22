# 磐石设置 · 磁盘占用监控与分析（Disk Usage Monitor & Analytics）设计文档

> 版本：v3（新增：存储预算放宽至 500MB/1GB 的逐日文件+索引存储、动态按需查询加载与加载动画）
> 状态：设计稿，待评审后实施
> 适用：deepin/UOS v25 · C++17 · Qt6 · DTK6 · 独立 DTK 应用（非控制中心插件）

---

## 1. 功能定位与目标

磐石系统（OSTree 只读根）的磁盘增长主要来自 `/var`（可写层）、快照数据、ostree 仓库与家目录。
本功能为普通用户提供：

1. **持续动态监控**：用户开启监控开关后，后台进程持续采样，应用关闭也不中断。
2. **动态可视化**：时间线图实时展示磁盘占用变化；**默认展示最近 7 天，可按需动态加载任意时间范围（近 7 天 ~ 一年）**。
3. **异常感知**：占用异常暴增时主动推送 DDE 系统通知 + 应用内提示，可查看告警详情。
4. **原因定位**：从"哪一天暴涨"下钻到"哪个文件夹/文件"，一键打开所在位置。
5. **趋势洞察**：趋势起始日、日均增长、容量预测（预计多久写满）。
6. **零门槛**：全流程点击即可，无需命令行；提供安全清理建议但不在本页直接删除。

---

## 2. 总体架构

```
┌─────────────────────────────────────────────────────────┐
│  GUI: panshi-manager（现有应用）                          │
│    DiskUsagePage / UsageChart / 告警列表 / 设置          │
│    DiskHistoryStore（索引加载 + 按范围懒加载 + LRU 缓存）  │
└──────────────┬──────────────────────┬───────────────────┘
               │ QFileSystemWatcher   │ DConfig
               │ 监听数据变化          │ 读写配置
┌──────────────▼──────────────────────▼───────────────────┐
│  共享存储（逐日文件 + 轻量索引，见 §6）                     │
│  • ~/.local/share/panshi-manager/disk-history/          │
│  • DConfig: org.deepin.panshi-manager                   │
└──────────────┬───────────────────────────────────────────┘
               │ systemd --user enable/disable/start/stop
┌──────────────▼───────────────────────────────────────────┐
│  监控进程: panshi-manager --monitor（无 GUI）              │
│    三层采样 / 事件触发定位 / 告警判定 / 通知发送            │
│    systemd user service + timer（开机自启）                │
└───────────────────────────────────────────────────────────┘
```

- 同一二进制以 `--monitor` 参数进入无界面模式，完全复用 backend / src/disk 模块。
- systemd 单元为**用户级**（`/usr/lib/systemd/user/`），启停走 `systemctl --user`，无需 root。
- 告警通知由监控进程直接通过 `org.freedesktop.Notifications` 发送（应用未启动也可感知）。

---

## 3. 数据模型（backend.h 新增）

```cpp
struct DiskItem {                          // 单层目录/文件大小项
    QString path;
    qint64  size  = 0;
    bool    isDir = true;
};

struct DiskDayRecord {                     // 某一天/某次采样的完整记录（逐日文件）
    QDate   date;
    qint64  capacity = 0;                  // df 总容量
    qint64  total    = 0;                  // df 已用
    QList<DiskItem> topLevel;              // / 下各一级目录大小（完整保留，不截断）
    QMap<QString, QList<DiskItem>> drillCache; // 增长目录下钻缓存（2~4 层）
    QList<DiskSample> samples;             // 当日高频采样（支持日内回放）
    bool    complete = true;               // 完整扫描？(降级/排除时 false)
};

struct DiskSample {                        // 高频轮询轻量样本
    QDateTime ts;
    qint64    total = 0;
    QMap<QString, qint64> hotDirs;         // 已知增长源目录当前大小
};

struct DiskDaySummary {                    // 索引中的轻量摘要（时间线快速渲染用）
    QDate   date;
    QString file;                          // 对应逐日数据文件名
    qint64  capacity = 0, total = 0;
    qint64  growth = 0;
    bool    spike = false;                 // 是否飙升日（危险色渲染）
    QList<DiskItem> topGrow;               // 当日增长 Top 3（悬浮提示用，摘要级）
};

struct DiskHistoryIndex {                  // 全局索引
    QList<DiskDaySummary> days;            // 按日期升序
    QDate firstDate, lastDate;
    qint64 storageBytes = 0;               // 当前占用
    qint64 budgetBytes  = 0;               // 预算（默认 512MB）
};

struct DiskSpikeInfo {                     // 异常事件
    QDate   date;
    qint64  growth = 0;
    QString causeDir;                      // 定位到的增长目录
    QList<DiskItem> causes;                // 增长贡献 Top 项
    bool    alerted = false;               // 是否已推送告警
};

struct DiskRangeStats {                    // 指定时间范围汇总
    qint64 beginUsed = 0, endUsed = 0;
    qint64 netGrowth = 0;
    qint64 dailyAvg  = 0;                  // 日均增长
    qint64 maxDayGrowth = 0; QDate maxDay; // 最大单日增长
    QDate trendStart;                      // 连续增长起始日
    qint64 etaDays = -1;                   // 预计可写满天数（-1 表示趋稳）
    QList<DiskItem> topGrowItems;          // 区间增长 Top 项
};
```

---

## 4. 监控引擎（src/disk/monitor.{h,cpp}）

### 4.1 三层采样（性能关键）

| 层级 | 频率（默认，可配） | 成本 | 作用 |
|---|---|---|---|
| 高频轮询 | 每 15 分钟 | 几乎为零（仅 `df` + 已知增长源轻量采样） | 感知"总量突然上涨" |
| 事件触发定位 | 增长超阈值时按需 | 中 | 只对增长的一级目录 `du` 下钻，当场定位元凶 |
| 每日基线 | 每日 1 次（低优先级） | 中 | 完整 `du` 记录，供时间线/趋势/预测 |

- 所有 `du` 均以 `ionice -c3` + `nice -n 19` 执行，监控不拖慢日常使用。
- **双重确认**：连续两次采样均确认增长才触发定位/告警，过滤"下载→删除"的瞬时波动。
- **磐石特性优化**：默认重点跟踪已知增长源（`/var`、ostree 仓库、快照数据、家目录），定位更快更准。

### 4.2 监控开关

- GUI 设置卡 DSwitchButton：
  - **开启**：立即静默执行一次完整基线扫描 → `systemctl --user enable --now` 监控服务 → 开始记录。
  - **关闭**：`systemctl --user disable --now` → 停止采样与告警，**历史数据保留**。
- 开关状态写入 DConfig（`monitor.enabled`），GUI 与监控进程共同读取。

### 4.3 排除目录（DConfig: monitor.excludes，字符串列表，前缀匹配）

- 排除只影响**告警判定与下钻定位**，不影响总量统计与排行展示。
- 提供"在排行中隐藏排除目录"选项（默认关）。
- 提供"忽略本次告警"：对某目录暂停告警 N 小时（DConfig: monitor.snooze 到期时间）。
- 默认预置建议项（如 `~/.cache`），Downloads 由用户按需添加。

### 4.4 告警判定（DConfig 可配）

- **双模式**：
  - 智能基线（默认）：开启后先学习 7 天建立"正常日增长基线"，告警阈值 = max(基线×倍数, 最小绝对值)，自适应。
  - 固定阈值：高级用户自定义（MB）。
- **冷却与去重**：同一目录冷却期（默认 6h）内只报一次，且报"累计增量"；全局免打扰时段（DConfig，默认 23:00–07:00 只记录不推送）。
- **容量预测**：按近期趋势外推 `etaDays`（预计可写满天数），趋稳时为 -1。

---

## 5. 告警与用户感知

| 通道 | 形态 |
|---|---|
| DDE 系统通知 | `org.freedesktop.Notifications`，标题含增长量与目录；带"查看详情"action，点击拉起应用并定位到对应日期 |
| 应用内角标 | 侧边栏"磁盘占用"项未读告警角标（DLabel 徽章） |
| 总览横幅 | 最新未读告警横幅（样式沿用 style.h 的 warnBox/danger 语义色） |
| 告警列表 | 时间/目录/增量/状态，可标记已读、可"忽略本次" |

---

## 6. 历史数据持久化：逐日文件 + 轻量索引 + 存储预算

### 6.1 存储布局（支撑大预算与按需加载）

```
~/.local/share/panshi-manager/disk-history/
  index.json                     // 轻量索引：每日摘要 + 预算统计（KB 级，毫秒级读取）
  2026-09-21.json                // 当日完整记录: topLevel + drillCache + samples
  2026-09-22.json
  ...
```

- 每个文件原子写（tmp + rename）；**只追加/修改当日文件，永不整体重写历史**。
- 清理策略按保留天数（默认 400 天，可配 90~730）删除过期逐日文件并重建索引。

### 6.2 存储预算（放宽至 500MB / 1GB）

- DConfig `monitor.storageBudgetMB`，默认 **512**，可选 256 / 512 / **1024**（1GB）。
- 预算充分满足高频用户：du 数据本身紧凑，估算全年完整保留（topLevel 全量 + 全年 drillCache 2~4 层 + samples 保留 90 天）约 **20~80MB**；500MB 预算可支持更高采样频率、更深下钻或多年数据；1GB 档支持 samples 全年保留与更深定位。
- **预算自动降级机制**（占用达预算 80% 时按优先级从低到高触发，全程后台自动）：
  1. 缩短高频 samples 保留天数（90 → 30 → 7）；
  2. 裁剪 drillCache 仅保留飙升日；
  3. 将最旧 30 天合并为聚合摘要（只留 topLevel 完整 + 日汇总，丢弃 samples/drillCache）；
  4. 仍未缓解则提示用户调大预算或清理历史。
- 降级状态写入 index.json 并反映在设置页（当前占用 / 预算、数据完整度说明）。

---

## 7. 时间范围查询与动态按需加载

### 7.1 快捷预设（按钮组）

| 预设 | 跨度 |
|---|---|
| **近 7 天（默认）** | 7 |
| 近 30 天 | 30 |
| 近 90 天 | 90 |
| 近半年 | 180 |
| **近一年** | **365** |
| 自定义 | 双日期选择器（DTK DDateEdit），约束：结束 ≥ 开始、跨度 ≤ 当前保留天数 |

### 7.2 动态按需加载（DiskHistoryStore，src/disk/history.{h,cpp}）

- **打开页面**：仅读 `index.json`（KB 级、毫秒完成）→ 默认渲染**最近 7 天**（只加载 7 个逐日文件）。
- **切换/查询范围**：仅加载该范围的逐日文件（QtConcurrent 后台流式读取，逐日回调进度）；**绝不加载全部历史**。
- **取消与防抖**：每次查询带 `requestId`，切换范围时自动取消上一请求，快速连点不堆积、不闪错。
- **内存 LRU 缓存**：已加载记录缓存复用，内存预算（默认保留最近加载 ~90 天，超出淘汰最旧），避免长时间浏览累积。
- 告警列表、增长详情、区间汇总全部与所选范围联动懒加载，状态统一。

### 7.3 加载动态效果（避免"以为卡死"）

UsageChart 加载状态机：

1. **加载中**：图表区显示**骨架占位**（浅色渐变占位条，主题自适应）+ 进度文字"正在加载 2026-05-01 ~ 06-01（12/30）…"；主线程不阻塞，页面其余区域可正常交互。
2. **逐日入场**：数据逐条到达时，柱高从 0 **平滑生长**（错峰 30~60ms/柱，QPropertyAnimation 驱动重绘参数），用户直观看到数据"流进来"。
3. **范围切换**：旧柱**淡出 → 新柱淡入**（交叉渐变），配合骨架过渡，视觉连续不突兀。
4. **减少动态效果**：遵循系统"减少动态效果"设置（DDE 外观动画开关）时，降级为无生长动画的直接淡入，保障无障碍。

### 7.4 区间汇总（DiskRangeStats 卡片）

期初/期末占用、净增长、日均增长、最大单日增长（含日期）、**趋势起始日**（区间内最长连续增长段的第一天）、增长 Top 项、容量预测"预计 X 天后写满"。

---

## 8. 页面与交互（DiskUsagePage）

沿用 BasePage + Ui::card/pageHeader/makeScrollArea，滚动容器内六张卡：

| 卡片 | 内容 |
|---|---|
| ① 磁盘概览 | DProgressBar 使用率 + 总/已用/可用 + 容量预测；**监控状态条**（监控中/未监控、上次采样时间、今日增长）；未读告警横幅 |
| ② 时间线 | UsageChart + 时间范围选择（§7，默认近 7 天）+ 图例 + "立即扫描"（进度条/取消，走 pkexec 提权） |
| ③ 区间汇总 | DiskRangeStats 关键指标 + 趋势起始日（随范围懒加载） |
| ④ 增长详情 | 选中日的 Top N 增长项（名称/大小/占比/打开位置）；无选中时显示最近飙升日 |
| ⑤ 占用排行 | 一级目录 Top 列表，可展开下钻到子目录/文件；每行"打开所在位置"（`gio open` 调起 DDE 文件管理器）；含排除目录隐藏开关 |
| ⑥ 监控设置 | 监控开关(DSwitchButton)、告警模式(智能基线/固定阈值+MB)、冷却时长、免打扰时段、排除目录管理(添加/删除/前缀匹配)、保留天数、**存储预算(256/512/1024MB)与当前占用/数据完整度**、清除历史 |

- 全部文案 `tr()` 包裹；图标用 DIconTheme（`drive-harddisk` 等）。
- 页面打开时若数据文件变化（QFileSystemWatcher 监听 index.json）自动刷新时间线与告警。

---

## 9. DConfig 配置项汇总（org.deepin.panshi-manager）

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| monitor.enabled | bool | false | 监控总开关 |
| monitor.pollMinutes | int | 15 | 高频轮询间隔 |
| monitor.baselineDays | int | 7 | 智能基线学习天数 |
| monitor.alertMode | string | baseline | baseline / fixed |
| monitor.fixedThresholdMB | int | 1024 | 固定阈值 |
| monitor.baselineMultiplier | double | 3.0 | 基线倍数 |
| monitor.cooldownHours | int | 6 | 同目录告警冷却 |
| monitor.quietStart/End | string | 23:00/07:00 | 免打扰时段 |
| monitor.excludes | string[] | [~/.cache] | 排除目录（前缀匹配） |
| monitor.retentionDays | int | 400 | 历史保留天数（90~730） |
| **monitor.storageBudgetMB** | int | **512** | 存储预算（256/512/1024） |
| **monitor.samplesRetentionDays** | int | 90 | 高频样本保留天数 |
| **monitor.memoryCacheDays** | int | 90 | 页面内存 LRU 缓存天数 |
| monitor.snoozeUntil | string | "" | 忽略告警到期时间 |

---

## 10. 集成与构建

- `CMakeLists.txt`：新增 `src/disk/{scanner,history,monitor}.cpp`、`src/monitormain.cpp`、`src/pages/diskusagepage.cpp`、`src/widgets/usagechart.cpp`；无新增第三方依赖。
- 安装：systemd user 单元 `data/systemd/panshi-monitor.service` + `.timer` → `/usr/lib/systemd/user/`；DConfig meta JSON → `/usr/share/dsg/configs/org.deepin.panshi-manager/`。
- mainwindow：侧边栏新增"磁盘占用"项（图标 `drive-harddisk`，位于"总览"之后），gotoPage 触发 refresh。
- style.h：仅扩展必要的语义色/徽章/骨架占位样式，沿用 DPalette 主题体系。

---

## 11. 边界与异常

- 首次开启无历史 → 引导"正在建立基线…"，基线期仅记录不告警。
- 扫描中途切页/关窗：扫描器与应用级单例绑定，不随页面销毁；GUI 关闭后由监控进程接管。
- root 不可用（自动扫描降级）：记录 `complete=false`，时间线以灰色区分；手动扫描才走 pkexec 提权。
- 磁盘满导致写历史失败：保留旧文件、记录错误并提示。
- 目录被删/挂载变化：缺失项按 0 处理并跳过。
- 数据损坏：单日文件校验失败仅跳过该日并以摘要占位；index.json 损坏则备份（.bak）重建索引，不崩溃。
- 预算达到上限且无法再降级：停止新增 samples，仅保留日汇总，并在设置页提示扩容或清理。

---

## 12. 实施里程碑

1. **M1 数据层**：backend 数据模型 + **逐日文件/索引存储 + 存储预算降级** + 懒加载 Store（loadIndex / loadRange / cancel / LRU 缓存）+ 区间统计/趋势/预测 + scanner 三层采样（进度、取消、降级）。
2. **M2 监控进程**：`--monitor` 入口 + systemd user service/timer + 告警判定/冷却/排除/通知 + DConfig。
3. **M3 页面与图表**：UsageChart 自绘（多跨度自适应、区间选择、点击定位、**骨架/逐柱生长/淡入淡出加载动画**）+ DiskUsagePage 六卡 + 设置交互（开关/排除/阈值/预算/清除）。
4. **M4 集成与验证**：CMake/安装/翻译、侧边栏接入、亮暗主题与 X11/Wayland 验证、通知 action 拉起定位、**大预算数据下的加载性能与动画流畅度验证**。
