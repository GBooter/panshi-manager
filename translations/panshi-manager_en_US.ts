<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US" sourcelanguage="zh_CN">
<context>
    <name>AboutPage</name>
    <message>
        <location filename="../src/pages/aboutpage.cpp" line="+41"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>版本 1.0.0  ·  Deepin 25 磐石系统管理工具</source>
        <translation>Version 1.0.0  ·  Deepin 25 immutable system management tool</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>基于 deepin-immutable-ctl / deepin-immutable-writable / OSTree 开发
支持 Deepin 25 (X11 / Wayland), 采用 DDE 桌面风格 (DTK)</source>
        <translation>Built on deepin-immutable-ctl / deepin-immutable-writable / OSTree · Supports Deepin 25 (X11 / Wayland) · Uses the DDE desktop style (DTK)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>作者：Feelup</source>
        <translation>Author: Feelup</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>磐石系统技术参考</source>
        <translation>Immutable system technical reference</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>· 核心目录只读保护: /usr 只读 overlay, /etc /opt 可写 overlay
· 原子更新: 升级失败自动回滚 (auto-rollback-on-failure)
· 快照备份: 基于 OSTree 秒级增量快照, 支持导出/导入
· 初始备份受系统保护, 不可删除/修改
· 快照还原需重启生效, 可写层开启时禁止还原
· 系统与数据分区分层: /home 等数据分区不受还原影响</source>
        <translation>· Read-only core directories: /usr read-only overlay, /etc /opt writable overlay · Atomic updates: auto-rollback-on-failure · Snapshot backup: OSTree-based second-level incremental snapshots, export/import supported · The initial backup is system-protected and cannot be deleted or modified · Snapshot rollback requires a reboot and is blocked while the writable layer is active · System and data are layered separately: data partitions such as /home are unaffected by rollback</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>常用命令参考</source>
        <translation>Common command reference</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>列出快照</source>
        <translation>List snapshots</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>创建快照</source>
        <translation>Create snapshot</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>还原快照</source>
        <translation>Roll back snapshot</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>保护状态</source>
        <translation>Protection status</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>开/关可写层</source>
        <translation>Toggle writable layer</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>部署状态</source>
        <translation>Deployment status</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>复制版本信息</source>
        <translation>Copy version info</translation>
    </message>
</context>
<context>
    <name>CheckOverlay</name>
    <message>
        <location filename="../src/widgets/checkoverlay.cpp" line="+80"/>
        <source>✓ %1    %2</source>
        <translation>✓ %1 %2</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>⟳ %1    …</source>
        <translation>⟳ %1…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>✗ %1    %2</source>
        <translation>✗ %1 %2</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>检查完成 · 共 %1</source>
        <translation>Check complete · %1 in total</translation>
    </message>
</context>
<context>
    <name>DeploysPage</name>
    <message>
        <location filename="../src/pages/deployspage.cpp" line="+211"/>
        <source>不能删除当前启动的部署</source>
        <translation>Cannot delete the currently booted deployment</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>删除后该版本的系统将无法再启动，占用空间会被释放。</source>
        <translation>After deletion, this system version will no longer be bootable and its disk space will be released.</translation>
    </message>
    <message>
        <location line="-28"/>
        <source>删除该旧部署以释放空间</source>
        <translation>Delete this old deployment to free up space</translation>
    </message>
    <message>
        <location line="-2"/>
        <location line="+29"/>
        <source>删除部署</source>
        <translation>Delete deployment</translation>
    </message>
    <message>
        <location line="-45"/>
        <source>历史</source>
        <translation>History</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-57"/>
        <source>回滚前</source>
        <translation>Pre-rollback</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>回滚前自动创建的安全版本, 确认系统正常后可删除</source>
        <translation>Safety version created automatically before rollback; delete it after confirming the system works normally</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>回滚备份</source>
        <translation>Rollback backup</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>当前</source>
        <translation>Current</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>当前启动</source>
        <translation>Currently booted</translation>
    </message>
    <message>
        <location line="-10"/>
        <source>当前正在运行的系统版本</source>
        <translation>The system version currently running</translation>
    </message>
    <message>
        <location line="-125"/>
        <source>提交ID</source>
        <translation>Commit ID</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>系统部署列表</source>
        <translation>System Deployment List</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>暂无系统版本</source>
        <translation>No system versions yet</translation>
    </message>
    <message>
        <location line="+121"/>
        <source>删除部署：%1</source>
        <translation>Delete deployment: %1</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>正在删除部署...</source>
        <translation>Deleting deployment...</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>删除失败，请查看详情了解具体原因。</source>
        <translation>Deletion failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
    <message>
        <location line="-218"/>
        <location line="+72"/>
        <source>正在读取部署状态...</source>
        <translation>Reading deployment status...</translation>
    </message>
    <message>
        <location line="-30"/>
        <source>每次系统升级或回滚都会生成一个新的可启动系统版本（部署）。</source>
        <translation>Every system upgrade or rollback creates a new bootable system version (deployment).</translation>
    </message>
    <message>
        <location line="-37"/>
        <source>状态</source>
        <translation>Status</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>系统版本</source>
        <translation>System version</translation>
    </message>
    <message>
        <location line="+184"/>
        <source>系统版本: %1</source>
        <translation>System version: %1</translation>
    </message>
    <message>
        <location line="-184"/>
        <source>索引</source>
        <translation>Index</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>说明: 当前启动的版本带「当前」标记; 历史版本会占用磁盘空间, 可删除以释放空间, 但不能删除当前正在使用的版本。</source>
        <translation>Note: The currently booted version carries a “Current” tag; historical versions occupy disk space and can be deleted to free it, but the version currently in use cannot be deleted.</translation>
    </message>
    <message>
        <location line="+166"/>
        <source>部署已删除，占用空间已释放。</source>
        <translation>Deployment deleted and its disk space released.</translation>
    </message>
    <message>
        <location line="-206"/>
        <source>部署时间</source>
        <translation>Deployed at</translation>
    </message>
    <message>
        <location line="+184"/>
        <source>部署时间: %1</source>
        <translation>Deployed at: %1</translation>
    </message>
</context>
<context>
    <name>DiskUsagePage</name>
    <message>
        <location filename="../src/pages/diskusagepage.cpp" line="+384"/>
        <source>磁盘空间</source>
        <translation>Disk usage</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>持续监控系统盘空间，定位容量增长来源</source>
        <translation>Monitor system disk usage continuously and locate the source of growth</translation>
    </message>
    <message>
        <location line="+69"/>
        <location line="+738"/>
        <source>正在检测服务状态…</source>
        <translation>Detecting service status…</translation>
    </message>
    <message>
        <location line="-735"/>
        <source>立即启动</source>
        <translation>Start now</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>监控设置</source>
        <translation>Monitoring settings</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>时间线</source>
        <translation>Timeline</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>折线展示每日磁盘占用；点击数据点查看该日增长详情</source>
        <translation>A line chart of daily disk usage; click a data point for that day&apos;s growth details</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>立即扫描</source>
        <translation>Scan now</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>正常增长</source>
        <translation>Normal growth</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>异常增长</source>
        <translation>Abnormal growth</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>选中日</source>
        <translation>Selected day</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>趋势</source>
        <translation>Trend</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 天</source>
        <translation>%1 days</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>自定义</source>
        <translation>Custom</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>返回系统盘</source>
        <translation>Back to system disk</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>区间汇总</source>
        <translation>Range summary</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>增长详情</source>
        <translation>Growth details</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>占用排行</source>
        <translation>Usage ranking</translation>
    </message>
    <message>
        <location line="+60"/>
        <source>净增长</source>
        <translation>Net growth</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>日均增长</source>
        <translation>Daily average</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>最大单日</source>
        <translation>Largest single day</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>趋势起始日</source>
        <translation>Trend start date</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>区间增长 Top 项</source>
        <translation>Top growth items in range</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>说明：这里列出所选日期（点击时间线数据点）相对前一日有增长的目录/文件，按增长量排序，最多 8 项；每天增长的目录数量不同，列表项数也随之变化，当天无增长时则显示提示。点击目录行可切换上方时间线。</source>
        <translation>Note: this lists the directories/files that grew on the selected day (click a timeline data point) relative to the previous day, sorted by growth, up to 8 items. The number of growing directories varies each day, so the list length changes accordingly; a hint is shown when there is no growth that day. Click a directory row to switch the timeline above.</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+20"/>
        <source>名称</source>
        <translation>Name</translation>
    </message>
    <message>
        <location line="-20"/>
        <location line="+20"/>
        <source>大小</source>
        <translation>Size</translation>
    </message>
    <message>
        <location line="-20"/>
        <location line="+20"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="-18"/>
        <source>增长详情表</source>
        <translation>Growth Detail Table</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>返回上级</source>
        <translation>Back</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>占比</source>
        <translation>Share</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>占用排行表</source>
        <translation>Space Usage Ranking Table</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>磁盘空间设置</source>
        <translation>Disk usage settings</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>监控</source>
        <translation>Monitoring</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>启用磁盘监控</source>
        <translation>Enable disk monitoring</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>开启后立即建立基线，由后台服务每 15 分钟采样一次并自动告警</source>
        <translation>When enabled, a baseline is established immediately; the background service samples every 15 minutes and raises alerts automatically</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>告警规则</source>
        <translation>Alert rules</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>智能基线</source>
        <translation>Smart baseline</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+30"/>
        <source>固定阈值</source>
        <translation>Fixed threshold</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>告警模式</source>
        <translation>Alert mode</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>增长超过该值触发告警（固定阈值模式）</source>
        <translation>Raise an alert when growth exceeds this value (fixed-threshold mode)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>基线倍数</source>
        <translation>Baseline multiplier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>智能基线 = 近 7 天日均增长 × 该倍数，且不小于最小告警值</source>
        <translation>Smart baseline = average daily growth over the last 7 days × this multiplier, and not less than the minimum alert value</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>告警冷却</source>
        <translation>Alert cooldown</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>同一目录冷却期内只告警一次，避免刷屏</source>
        <translation>Each directory is alerted only once per cooldown period to avoid flooding</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>排除目录</source>
        <translation>Excluded directories</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>输入目录，如 /home/Test/Downloads</source>
        <translation>Enter a directory, e.g. /home/Test/Downloads</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>添加</source>
        <translation>Add</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>移除选中</source>
        <translation>Remove selected</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>数据存储</source>
        <translation>Data storage</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>保留天数</source>
        <translation>Retention days</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>存储预算</source>
        <translation>Storage budget</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>历史数据本地占用上限，超出预算自动降级保存</source>
        <translation>Upper limit for local history data; saving degrades automatically when the budget is exceeded</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>当前占用</source>
        <translation>Current usage</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+830"/>
        <source>清除全部监控数据</source>
        <translation>Clear all monitoring data</translation>
    </message>
    <message>
        <location line="-828"/>
        <source>数据管理</source>
        <translation>Data management</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>正在加载 %1 ~ %2 的数据…</source>
        <translation>Loading data for %1 ~ %2…</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>选择日期范围</source>
        <translation>Select a date range</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>第 1 次点击选择开始日期，第 2 次点击选择结束日期（第二次早于首次时会自动改为新的开始日期）</source>
        <translation>Click once to pick the start date, click again to pick the end date (if the second click is earlier, it becomes the new start date)</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>已选开始日期：%1，请再点击选择结束日期</source>
        <translation>Start date selected: %1. Click again to pick the end date</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>已选范围：%1 ~ %2</source>
        <translation>Range selected: %1 ~ %2</translation>
    </message>
    <message>
        <location line="+33"/>
        <location line="+657"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-656"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>加载中 %1/%2 …</source>
        <translation>Loading %1/%2…</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>该范围内暂无监控数据</source>
        <translation>No monitoring data in this range</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>已加载 %1 天记录</source>
        <translation>Loaded %1 days of records</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>已用 %1</source>
        <translation>Used %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>总容量 %1 · 可用 %2</source>
        <translation>Total %1 · Free %2</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>已用 --</source>
        <translation>Used --</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>总容量 -- · 可用 --</source>
        <translation>Total -- · Free --</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>● 监控中 · 每 %1 分钟采样一次 · 历史保留 %2 天</source>
        <translation>● Monitoring · Sampled every %1 min · History kept %2 days</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>○ 监控未开启：开启后将自动建立基线并持续监控</source>
        <translation>○ Monitoring off: enable it to establish a baseline and monitor continuously</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>读取历史数据索引</source>
        <translation>Reading history index</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>
最近记录 %1 · 该日增长 %2</source>
        <translation>Latest record %1 · That day&apos;s growth %2</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>%1 · %2 增长 %3</source>
        <translation>%1 · %2 grew %3</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>正在检查系统信息…</source>
        <translation>Checking system information…</translation>
    </message>
    <message>
        <location line="+65"/>
        <source>磁盘监控运行中</source>
        <translation>Disk monitoring is running</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>磁盘监控未启动，无法自动记录</source>
        <translation>Disk monitoring is not running, cannot record automatically</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>磁盘监控未开启</source>
        <translation>Disk monitoring is not enabled</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>正在启动磁盘监控…</source>
        <translation>Starting disk monitoring…</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>监控组件未安装：请先安装应用（sudo ./install.sh）</source>
        <translation>Monitoring component is not installed: install the app first (sudo ./install.sh)</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>%1（%2）</source>
        <translation>%1 (%2)</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>无连续增长</source>
        <translation>No sustained growth</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>按当前增速约 %1 天后写满</source>
        <translation>At the current rate, full in about %1 days</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>增长趋稳，暂无容量风险</source>
        <translation>Growth is leveling off; no capacity risk at present</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>区间内无明显增长</source>
        <translation>No notable growth in this range</translation>
    </message>
    <message>
        <location line="+23"/>
        <location line="+29"/>
        <source>所选日期不在当前加载范围内</source>
        <translation>The selected date is outside the loaded range</translation>
    </message>
    <message>
        <location line="-23"/>
        <source>目录 %1</source>
        <translation>Directory %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>无数据</source>
        <translation>No data</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>较前一日变化</source>
        <translation>Change vs. previous day</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>前一日暂无数据</source>
        <translation>No data for the previous day</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>该日无显著增长</source>
        <translation>No notable growth that day</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>暂无数据：请开启监控或点击“立即扫描”</source>
        <translation>No data: enable monitoring or click “Scan now”</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>当前显示：系统盘占用</source>
        <translation>Now showing: system disk usage</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>目录“%1”暂无逐日数据，请先开启监控或在该目录下钻</source>
        <translation>No daily data for directory “%1”; enable monitoring or drill into it first</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>当前显示：系统盘占用变化</source>
        <translation>Now showing: system disk usage change</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>当前显示：目录 %1 的占用变化</source>
        <translation>Now showing: usage change for directory %1</translation>
    </message>
    <message>
        <location line="+29"/>
        <location line="+33"/>
        <source>%1 / %2</source>
        <translation>%1 / %2</translation>
    </message>
    <message>
        <location line="-279"/>
        <location line="+307"/>
        <source>监控已开启，正在建立基线（由后台服务采样）</source>
        <translation>Monitoring enabled; establishing a baseline (sampled by the background service)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>监控已关闭，历史数据保留</source>
        <translation>Monitoring disabled; history retained</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>将删除所有历史占用记录与告警，且不可恢复。确定继续？</source>
        <translation>This will delete all history usage records and alerts, and cannot be undone. Continue?</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>清除</source>
        <translation>Clear</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>正在扫描系统盘目录…（可稍候，完成后自动刷新）</source>
        <translation>Scanning system disk directories… (this may take a while; it refreshes automatically when done)</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>扫描完成，今日记录已更新</source>
        <translation>Scan complete; today&apos;s record is updated</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>扫描失败</source>
        <translation>Scan failed</translation>
    </message>
    <message>
        <location line="+87"/>
        <source>正在读取 %1 的子目录…</source>
        <translation>Reading subdirectories of %1…</translation>
    </message>
    <message>
        <location line="-193"/>
        <source>监控服务%1失败：%2</source>
        <translation>Failed to %1 the monitoring service: %2</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>启动</source>
        <translation>start</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>停止</source>
        <translation>stop</translation>
    </message>
    <message>
        <location line="-292"/>
        <location line="+293"/>
        <source>未知错误</source>
        <translation>unknown error</translation>
    </message>
    <message>
        <location line="-293"/>
        <source>启动失败：%1</source>
        <translation>Failed to start: %1</translation>
    </message>
    <message>
        <location line="-60"/>
        <source>无法连接 systemd 用户会话</source>
        <translation>Cannot connect to the systemd user session</translation>
    </message>
    <message>
        <location line="+599"/>
        <source>systemd 未识别监控单元（panshi-monitor.timer）</source>
        <translation>systemd does not recognize the monitoring unit (panshi-monitor.timer)</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>systemctl --user 执行失败</source>
        <translation>systemctl --user failed</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/mainwindow.cpp" line="+36"/>
        <location line="+5"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+67"/>
        <source>概览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>系统维护</source>
        <translation>System maintenance</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>设置</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>总览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>磁盘空间</source>
        <translation>Disk usage</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>系统保护</source>
        <translation>System protection</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>备份与版本</source>
        <translation>Backups &amp; versions</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>系统更新</source>
        <translation>System update</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>策略与设置</source>
        <translation>Policies &amp; settings</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
</context>
<context>
    <name>OverviewPage</name>
    <message>
        <location filename="../src/pages/overviewpage.cpp" line="+38"/>
        <source>总览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统运行状态一览</source>
        <translation>At a glance: Panshi system status</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>磐石功能</source>
        <translation>Panshi features</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>总开关：控制磐石系统保护的启用状态</source>
        <translation>Master switch: controls whether Panshi system protection is enabled</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>磐石保护</source>
        <translation>Panshi protection</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统可写层</source>
        <translation>System writable layer</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>备份快照</source>
        <translation>Backup snapshots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统盘剩余</source>
        <translation>System disk free</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>系统信息</source>
        <translation>System information</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>当前运行的系统与内核</source>
        <translation>Currently running system and kernel</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>系统名称</source>
        <translation>System name</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统版本</source>
        <translation>System version</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>内核版本</source>
        <translation>Kernel version</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>磐石部署</source>
        <translation>Panshi deployments</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>存储空间</source>
        <translation>Storage</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统各仓库占用（物理）</source>
        <translation>Physical usage of each Panshi repository</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>系统仓库 /ostree</source>
        <translation>System repo /ostree</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>数据仓库 /persistent</source>
        <translation>Data repo /persistent</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>快照数据</source>
        <translation>Snapshot data</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统盘可用</source>
        <translation>System disk free</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>快速操作</source>
        <translation>Quick actions</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>常用功能快捷入口</source>
        <translation>Shortcuts to common features</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>立即创建备份</source>
        <translation>Create backup now</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>检查系统更新</source>
        <translation>Check for updates</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>保护设置</source>
        <translation>Protection settings</translation>
    </message>
    <message>
        <location line="+55"/>
        <source>正在读取磐石系统状态…</source>
        <translation>Reading Panshi system status…</translation>
    </message>
    <message>
        <location line="+51"/>
        <source>%1 个 · 计算中…</source>
        <translation>%1 · calculating…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>统计中…</source>
        <translation>Calculating…</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>正在统计存储空间…</source>
        <translation>Calculating storage usage…</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>%1 个 · %2</source>
        <translation>%1 · %2</translation>
    </message>
</context>
<context>
    <name>Panshi::DiskMonitor</name>
    <message>
        <location filename="../src/disk/monitor.cpp" line="+55"/>
        <source>监控周期已在运行</source>
        <translation>Monitoring cycle is already running</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>监控未开启</source>
        <translation>Monitoring is not enabled</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>df 查询失败</source>
        <translation>df query failed</translation>
    </message>
    <message>
        <location line="+109"/>
        <source>监控周期超时</source>
        <translation>Monitoring cycle timed out</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>监控完成</source>
        <translation>Monitoring complete</translation>
    </message>
    <message>
        <location line="+169"/>
        <source>磁盘空间异常增长</source>
        <translation>Abnormal disk usage growth</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统盘 %1 增长 %2，主要来源：%3</source>
        <translation>System disk %1 grew by %2. Main source: %3</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>查看详情</source>
        <translation>View details</translation>
    </message>
</context>
<context>
    <name>Panshi::DiskScanner</name>
    <message>
        <location filename="../src/disk/scanner.cpp" line="+213"/>
        <source>扫描已取消</source>
        <translation>Scan cancelled</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>扫描未完整完成</source>
        <translation>Scan did not complete</translation>
    </message>
</context>
<context>
    <name>PanshiCountdownDialog</name>
    <message>
        <location filename="../src/widgets/countdowndialog.cpp" line="+82"/>
        <source>即将完全关闭磐石保护</source>
        <translation>About to fully disable Panshi protection</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>此为高风险操作，请务必确认后再继续</source>
        <translation>This is a high-risk operation. Please make sure before continuing</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>核心目录(/usr 等)将失去只读保护</source>
        <translation>Core directories (such as /usr) will lose read-only protection</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>恶意软件与误操作可直接破坏系统</source>
        <translation>Malware and misoperations can damage the system directly</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重启后仍保持关闭，需手动重新开启保护</source>
        <translation>Stays off after reboot; you must re-enable protection manually</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+31"/>
        <source>我已知晓风险，完全关闭（%1s）</source>
        <translation>I understand the risk, disable fully (%1s)</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>我已知晓风险，完全关闭</source>
        <translation>I understand the risk, disable fully</translation>
    </message>
</context>
<context>
    <name>PanshiProtection</name>
    <message>
        <location filename="../src/pages/protectioncards.h" line="+95"/>
        <source>只读保护</source><translation>Read-only protection</translation></message>
    <message>
        <location line="+5"/>
        <source>可写层</source><translation>Writable layer</translation></message>
    <message>
        <location line="+5"/>
        <source>重启后状态</source><translation>State after reboot</translation></message>
    <message>
        <location line="+13"/>
        <source>已开启</source><translation>Enabled</translation></message>
    <message>
        <location line="+0"/>
        <source>已关闭</source><translation>Disabled</translation></message>
    <message>
        <location line="+2"/>
        <source>核心目录只读</source><translation>Core directories read-only</translation></message>
    <message>
        <location line="+0"/>
        <source>核心目录可写</source><translation>Core directories writable</translation></message>
    <message>
        <location line="+19"/>
        <source>已解锁</source><translation>Unlocked</translation></message>
    <message>
        <location line="+0"/>
        <source>已锁定</source><translation>Locked</translation></message>
    <message>
        <location line="+2"/>
        <source>开发调试模式</source><translation>Debug mode</translation></message>
    <message>
        <location line="+0"/>
        <source>未启用</source><translation>Not enabled</translation></message>
    <message>
        <location line="+17"/>
        <source>恢复只读</source><translation>Restores read-only</translation></message>
    <message>
        <location line="+0"/>
        <source>仍保持可写</source><translation>Stays writable</translation></message>
    <message>
        <location line="+2"/>
        <source>临时关闭</source><translation>Temporarily off</translation></message>
    <message>
        <location line="+0"/>
        <source>永久关闭</source><translation>Permanently off</translation></message>
    <message>
        <location line="+6"/>
        <source>仍为只读</source><translation>Stays read-only</translation></message>
    <message>
        <location line="+2"/>
        <source>未做变更</source><translation>No change</translation></message>
</context>
<context>
    <name>PanshiSwitch::PanshiSwitchRow</name>
    <message>
        <location filename="../src/pages/masterswitch.cpp" line="+37"/>
        <source>磐石功能</source>
        <translation>Panshi features</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>详情</source>
        <translation>Details</translation>
    </message>
</context>
<context>
    <name>PanshiUi::PanshiAboutDialog</name>
    <message>
        <location filename="../src/ui/aboutdialog.cpp" line="+268"/>
        <source>关于磐石设置</source>
        <translation>About Panshi Manager</translation>
    </message>
    <message>
        <location line="+110"/>
        <source>应用图标</source>
        <translation>Application icon</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>应用名称</source>
        <translation>Application name</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>版本 %1</source>
        <translation>Version %1</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>作者</source>
        <translation>Author</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>平台</source>
        <translation>Platform</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>致谢</source>
        <translation>Acknowledgements</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>感谢所使用的开源软件</source>
        <translation>Thanks to the open source software used</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>主页</source>
        <translation>Website</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>协议</source>
        <translation>License</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>磐石设置遵循 GPL-3.0-or-later 协议发布</source>
        <translation>Panshi Manager is released under the GPL-3.0-or-later license</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>磐石系统技术参考</source>
        <translation>Panshi system technical reference</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>· 核心目录只读保护: /usr 只读 overlay, /etc /opt 可写 overlay
· 原子更新: 升级失败自动回滚 (auto-rollback-on-failure)
· 快照备份: 基于 OSTree 秒级增量快照, 支持导出/导入
· 初始备份受系统保护, 不可删除/修改
· 快照还原需重启生效, 可写层开启时禁止还原
· 系统与数据分区分层: /home 等数据分区不受还原影响</source>
        <translation>· Read-only protection: /usr is a read-only overlay, /etc and /opt are writable overlays
· Atomic updates: failed upgrades roll back automatically (auto-rollback-on-failure)
· Snapshot backup: second-level incremental snapshots based on OSTree, export/import supported
· The initial backup is system-protected and cannot be deleted or modified
· Restoring a snapshot takes effect after reboot; not allowed while the writable layer is enabled
· System and data are layered: data partitions such as /home are unaffected by restore</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>常用命令参考</source>
        <translation>Common commands reference</translation>
    </message>
</context>
<context>
    <name>PolicyPage</name>
    <message>
        <location filename="../src/pages/policypage.cpp" line="+53"/>
        <source>策略与设置</source>
        <translation>Policies &amp; settings</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>备份保留策略与磐石系统参数</source>
        <translation>Backup retention policies and immutable system parameters</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+115"/>
        <source>备份保留策略</source>
        <translation>Backup retention policy</translation>
    </message>
    <message>
        <location line="-58"/>
        <location line="+59"/>
        <source>磐石系统参数</source>
        <translation>Immutable system parameters</translation>
    </message>
    <message>
        <location line="-114"/>
        <source>磐石系统升级时会自动创建备份(系统内置, 不可关闭)。应用通过以下策略避免备份无限累积。</source>
        <translation>The immutable system creates a backup automatically on upgrade (built in, cannot be disabled). The policies below keep backups from piling up indefinitely.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>自动清理超额备份</source>
        <translation>Auto-clean excess backups</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>超过数量上限时, 自动删除最旧的普通备份(初始备份永不删除)</source>
        <translation>When the limit is exceeded, delete the oldest regular backups (the initial backup is never deleted)</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>备份数量上限</source>
        <translation>Backup count limit</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>超过上限后自动删除最旧的普通备份, 释放存储空间</source>
        <translation>Delete the oldest regular backups once the limit is exceeded, freeing storage</translation>
    </message>
    <message>
        <location line="+3"/>
        <source> 个 (0=不限)</source>
        <translation> (0 = unlimited)</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>回滚前备份自动清理</source>
        <translation>Auto-clean pre-rollback backups</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原前自动创建“回滚前备份”, 回滚成功后重启3次正常即自动删除该备份</source>
        <translation>A “pre-rollback backup” is created before restore; after a successful rollback it is deleted once the system restarts normally three times</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>手动删除(默认, 最安全)</source>
        <translation>Delete manually (default, safest)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>重启3次正常后自动删除</source>
        <translation>Auto-delete after 3 normal restarts</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>写入 /etc/deepin-immutable-ctl/deepin-immutable-ctl.conf (需管理员权限)</source>
        <translation>Written to /etc/deepin-immutable-ctl/deepin-immutable-ctl.conf (requires administrator privileges)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>保存系统参数</source>
        <translation>Save system parameters</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>升级失败自动回滚</source>
        <translation>Auto-rollback on failed upgrade</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统升级失败时自动回滚到升级前状态, 避免系统无法启动</source>
        <translation>Automatically roll back to the pre-upgrade state when an upgrade fails, so the system stays bootable</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>最大备份部署数</source>
        <translation>Max backup deployments</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>升级/回滚时保留的旧系统部署数量(默认1, 新备份自动覆盖旧备份)</source>
        <translation>Number of old system deployments kept during upgrade/rollback (default 1; new backups overwrite old ones)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source> 个</source>
        <translation> </translation>
    </message>
    <message>
        <location line="+8"/>
        <source>自动刷新阈值</source>
        <translation>Auto-refresh threshold</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>修改层超过该阈值时自动刷新合并到系统层</source>
        <translation>Refresh and merge the writable layer into the system layer once it exceeds this threshold</translation>
    </message>
    <message>
        <location line="+3"/>
        <source> MiB</source>
        <translation> MiB</translation>
    </message>
    <message>
        <location line="+77"/>
        <source>正在读取配置...</source>
        <translation>Reading configuration…</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>正在保存系统参数...</source>
        <translation>Saving system parameters…</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>系统参数已保存</source>
        <translation>System parameters saved</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>保存失败: %1</source>
        <translation>Save failed: %1</translation>
    </message>
</context>
<context>
    <name>ProtectionPage</name>
    <message>
        <location filename="../src/pages/protectionpage.cpp" line="+37"/>
        <source>系统保护</source>
        <translation>System protection</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统只读保护(可写层)管理</source>
        <translation>Manage Panshi read-only protection (writable layer)</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>说明</source>
        <translation>Description</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>磐石系统将核心目录(/usr 等)以只读方式挂载, 防止误操作或恶意软件破坏系统。
开启“可写层”将解除只读保护(适合开发调试), 关闭后恢复保护。
注意: 开启可写层后系统将拒绝执行快照还原。</source>
        <translation>Panshi mounts core directories (such as /usr) read-only to prevent misoperations or malware from damaging the system. Enabling the “writable layer” lifts read-only protection (suitable for development and debugging); turning it off restores protection. Note: while the writable layer is on, the system refuses to perform snapshot rollback.</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+64"/>
        <source>保护开关</source>
        <translation>Protection switch</translation>
    </message>
    <message>
        <location line="-64"/>
        <source>永久开启或关闭磐石系统保护</source>
        <translation>Permanently enable or disable Panshi system protection</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>开启保护 (恢复只读)</source>
        <translation>Enable protection (restore read-only)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>恢复核心目录只读挂载, 需要重启系统后生效</source>
        <translation>Restore read-only mounting of core directories; takes effect after reboot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>开启保护</source>
        <translation>Enable protection</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>关闭保护（高级操作）</source>
        <translation>Disable protection (advanced)</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>关闭保护后核心目录将可写, 且系统拒绝执行快照还原。请仅在开发调试时短期使用。</source>
        <translation>After disabling, core directories become writable and the system refuses snapshot rollback. Use it only for short-term development and debugging.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>永久关闭保护 (开启可写层)</source>
        <translation>Disable protection permanently (enable writable layer)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重启后仍保持可写, 需手动再次开启保护</source>
        <translation>Stays writable after reboot; you must re-enable protection manually</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>永久关闭保护</source>
        <translation>Disable protection permanently</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>临时关闭保护 (重启后自动恢复)</source>
        <translation>Disable protection temporarily (restored after reboot)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>仅本次开机生效, 重启后自动恢复只读保护</source>
        <translation>Effective for this boot only; read-only protection is restored after reboot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>临时关闭保护</source>
        <translation>Disable protection temporarily</translation>
    </message>
    <message>
        <location line="+13"/>
        <location line="+12"/>
        <source>磐石功能</source>
        <translation>Panshi features</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>总开关：控制磐石系统保护的启用状态</source>
        <translation>Master switch: controls whether Panshi system protection is enabled</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>正在读取保护状态...</source>
        <translation>Reading protection status…</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>开启只读保护</source>
        <translation>Enable read-only protection</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>关闭只读保护(开启可写层)</source>
        <translation>Disable read-only protection (enable writable layer)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>

本次为临时关闭, 重启后系统将自动恢复只读保护。</source>
        <translation>This is temporary; read-only protection will be restored automatically after reboot.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>确定要%1吗？
此操作需要重启后生效。%2</source>
        <translation>Do you want to %1? This takes effect after reboot. %2</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>正在设置保护状态...</source>
        <translation>Setting protection status…</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>设置成功，请重启系统使设置生效。</source>
        <translation>Set successfully. Reboot for it to take effect.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>设置失败，请查看详情了解具体原因。</source>
        <translation>Setting failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/main.cpp" line="+84"/>
        <location line="+1"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Deepin 25 磐石系统管理工具</source>
        <translation>Deepin 25 immutable system management tool</translation>
    </message>
    <message>
        <location filename="../src/pages/common.h" line="+355"/>
        <location line="+4"/>
        <location line="+1"/>
        <location line="+20"/>
        <source>复制</source>
        <translation>Copy</translation>
    </message>
    <message>
        <location line="-24"/>
        <source>复制命令</source>
        <translation>Copy command</translation>
    </message>
    <message>
        <location filename="../src/pages/aboutpage.cpp" line="+5"/>
        <location filename="../src/pages/common.h" line="+16"/>
        <source>已复制</source>
        <translation>Copied</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>复制版本信息</source>
        <translation>Copy version info</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>磐石设置  </source>
        <translation>Panshi Manager  </translation>
    </message>
    <message>
        <location filename="../src/pages/common.h" line="-276"/>
        <location filename="../src/ui/confirmdialog.cpp" line="+99"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/pages/masterswitch.cpp" line="+130"/>
        <location filename="../src/ui/confirmdialog.cpp" line="-2"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>开启磐石保护</source>
        <translation>Enable Panshi protection</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>确定要恢复核心目录只读保护吗？此操作需要重启后生效。</source>
        <translation>Restore read-only protection on core directories? This takes effect after reboot.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>恢复保护</source>
        <translation>Restore protection</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>关闭磐石保护</source>
        <translation>Disable Panshi protection</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关闭后核心目录将解除只读保护，恶意软件与误操作可能直接破坏系统。</source>
        <translation>After this, core directories lose read-only protection; malware and misoperations may damage the system directly.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>核心系统目录将变为可写</source>
        <translation>Core system directories will become writable</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>恶意软件或误操作可直接破坏系统文件</source>
        <translation>Malware or misoperations can damage system files directly</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重启后按所选方式决定是否恢复保护</source>
        <translation>After reboot, protection is restored according to your choice</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>临时关闭（重启后自动恢复）</source>
        <translation>Temporarily disable (restored after reboot)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>完全关闭（重启后仍保持关闭）</source>
        <translation>Fully disable (stays off after reboot)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>关闭保护</source>
        <translation>Disable protection</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>设置成功，请重启系统使设置生效。</source>
        <translation>Set successfully. Reboot for it to take effect.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>设置失败，请查看详情了解具体原因。</source>
        <translation>Setting failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
    <message>
        <location filename="../src/ui/sidebar.cpp" line="+194"/>
        <source>主导航</source>
        <translation>Main navigation</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+4"/>
        <source>搜索设置</source>
        <translation>Search settings</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>输入设置名称可跳转到对应设置项</source>
        <translation>Enter a setting name to jump to it</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>设置搜索结果</source>
        <translation>Setting search results</translation>
    </message>
    <message>
        <location line="+204"/>
        <source>未找到匹配的设置项</source>
        <translation>No matching settings found</translation>
    </message>
    <message>
        <location filename="../src/backend.cpp" line="+77"/>
        <source>无法启动进程: %1</source>
        <translation>Cannot start process: %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>命令执行超时: %1</source>
        <translation>Command timed out: %1</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>命令执行失败</source>
        <translation>Command execution failed</translation>
    </message>
    <message>
        <location line="+491"/>
        <source>无法写入临时配置文件</source>
        <translation>Cannot write temporary config file</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>无法写入临时状态文件</source>
        <translation>Cannot write temporary state file</translation>
    </message>
    <message>
        <location line="+82"/>
        <source>安装 systemd 服务失败</source>
        <translation>Failed to install systemd service</translation>
    </message>
    <message>
        <location filename="../src/disk/scanner.cpp" line="-150"/>
        <source>无法启动 df 进程</source>
        <translation>Cannot start df process</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>df 执行超时</source>
        <translation>df execution timed out</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>无法解析 df 输出</source>
        <translation>Cannot parse df output</translation>
    </message>
    <message>
        <location filename="../src/pages/diskusagepage.cpp" line="-1629"/>
        <source>打开位置</source>
        <translation>Open location</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>打开位置：%1</source>
        <translation>Open location: %1</translation>
    </message>
    <message>
        <location filename="../src/pages/masterswitch.h" line="+37"/>
        <source>未知状态 · 无法读取磐石配置</source>
        <translation>Unknown status · cannot read Panshi configuration</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>保护中 · 核心目录只读</source>
        <translation>Protected · core directories read-only</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>已临时关闭 · 重启后自动恢复</source>
        <translation>Temporarily off · restored after reboot</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>已完全关闭 · 重启后仍保持关闭</source>
        <translation>Fully off · stays off after reboot</translation>
    </message>
    <message>
        <location filename="../src/pages/updatepage.cpp" line="+146"/>
        <source>系统升级</source>
        <translation>System upgrade</translation>
    </message>
    <message>
        <location filename="../src/ui/components.cpp" line="+587"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关闭提示</source>
        <translation>Close notice</translation>
    </message>
    <message>
        <location filename="../src/tools/navshot.cpp" line="+196"/>
        <source>概览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>系统维护</source>
        <translation>Maintenance</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>设置</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../src/ui/aboutdialog.cpp" line="-358"/>
        <source>Deepin 25 磐石（immutable）系统管理工具</source>
        <translation>Deepin 25 Panshi (immutable) system management tool</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Deepin 25（X11 / Wayland）· OSTree 快照与只读保护</source>
        <translation>Deepin 25 (X11 / Wayland) · OSTree snapshots and read-only protection</translation>
    </message>
    <message>
        <location line="+106"/>
        <source>—</source>
        <translation>—</translation>
    </message>
    <message>
        <location line="-23"/>
        <location line="+3"/>
        <location line="+39"/>
        <source>%1：%2</source>
        <translation>%1: %2</translation>
    </message>
    <message>
        <location filename="../src/pages/diskusagepage.cpp" line="-71"/>
        <source>systemctl --user %1 执行超时</source>
        <translation>systemctl --user %1 timed out</translation>
    </message>
</context>
<context>
    <name>ScanOverlay</name>
    <message>
        <location filename="../src/widgets/scanoverlay.cpp" line="+42"/>
        <source>正在扫描…</source>
        <translation>Scanning…</translation>
    </message>
</context>
<context>
    <name>SnapshotsPage</name>
    <message>
        <location filename="../src/pages/snapshotspage.cpp" line="+90"/>
        <source>名称</source>
        <translation>Name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>类型</source>
        <translation>Type</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>创建时间</source>
        <translation>Created</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>占用空间</source>
        <translation>Disk usage</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>ID</source>
        <translation>ID</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="-11"/>
        <location line="+112"/>
        <source>正在读取快照列表...</source>
        <translation>Reading snapshot list…</translation>
    </message>
    <message>
        <location line="-94"/>
        <source>系统备份列表</source>
        <translation>System Backup List</translation>
    </message>
    <message>
        <location line="+140"/>
        <source>未启用</source><translation>Not enabled</translation></message>
    <message>
        <location line="+3"/>
        <source>手动</source>
        <translation>Manual</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重启 3 次后</source>
        <translation>After 3 restarts</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>已完成</source>
        <translation>Completed</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>失败</source>
        <translation>Failed</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>已取消</source>
        <translation>Cancelled</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>进行中</source>
        <translation>In progress</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>备份数量上限: %1&#x3000;·&#x3000;当前 %2 个&#x3000;·&#x3000;回滚前安全备份: %3</source>
        <translation>Backup limit: %1  ·  %2 currently  ·  Pre-rollback safety backup: %3</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>不限制</source>
        <translation>Unlimited</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>%1
描述: %2</source>
        <translation>%1
Description: %2</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1，%2</source>
        <translation>%1, %2</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>初始</source>
        <translation>Initial</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>系统初始备份, 受磐石保护, 不可删除</source>
        <translation>Initial system backup, protected and not deletable</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>回滚前</source>
        <translation>Pre-rollback</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>还原前自动创建的安全备份, 回滚成功后可在本页手动删除</source>
        <translation>Safety backup created before restore; can be deleted manually here after a successful rollback</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>普通备份</source>
        <translation>Regular backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统初始备份</source>
        <translation>Initial system backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>回滚前备份</source>
        <translation>Pre-rollback backup</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>计算中...</source>
        <translation>Calculating…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>增量数据: %1
独占对象: %2</source>
        <translation>Incremental data: %1
Exclusive objects: %2</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>还原</source>
        <translation>Restore</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>将系统还原到此备份的状态（重启后生效）</source>
        <translation>Restore the system to this backup (takes effect after reboot)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>还原到备份：%1</source>
        <translation>Restore to backup: %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+104"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="-102"/>
        <source>系统初始备份受保护, 不可删除</source>
        <translation>The initial system backup is protected and cannot be deleted</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>删除此备份</source>
        <translation>Delete this backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>删除备份：%1</source>
        <translation>Delete backup: %1</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>上一项操作尚未完成，请稍候再试。</source>
        <translation>The previous operation is still running. Please try again shortly.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>备份名称</source>
        <translation>Backup name</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>例如: 更新前备份</source>
        <translation>e.g. Before-update backup</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>描述(可选)</source>
        <translation>Description (optional)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>记录该备份的用途...</source>
        <translation>Record the purpose of this backup…</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+59"/>
        <location line="+86"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-144"/>
        <source>创建</source>
        <translation>Create</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>备份名称不能为空，请填写后重试。</source>
        <translation>Backup name cannot be empty. Please fill it in and try again.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>正在创建备份, 请稍候...</source>
        <translation>Creating backup, please wait…</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>备份“%1”创建成功。</source>
        <translation>Backup “%1” created successfully.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>创建失败，请查看详情了解具体原因。</source>
        <translation>Creation failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+47"/>
        <location line="+135"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
    <message>
        <location line="-136"/>
        <source>删除失败，请查看详情了解具体原因。</source>
        <translation>Deletion failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+135"/>
        <source>还原失败，请查看详情了解具体原因。</source>
        <translation>Rollback failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="-163"/>
        <source>删除备份</source>
        <translation>Delete backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>删除后该备份的增量数据将被释放，且无法恢复。</source>
        <translation>The incremental data of this backup will be released and cannot be recovered.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+77"/>
        <location line="+95"/>
        <source>目标备份: %1</source>
        <translation>Target backup: %1</translation>
    </message>
    <message>
        <location line="-171"/>
        <location line="+77"/>
        <source>创建时间: %1</source>
        <translation>Created: %1</translation>
    </message>
    <message>
        <location line="-69"/>
        <source>正在删除备份...</source>
        <translation>Deleting backup…</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>备份已删除，占用空间已释放。</source>
        <translation>Backup deleted and its disk space released.</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>可写层冲突</source>
        <translation>Writable layer conflict</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>当前可写层（只读保护已关闭）处于开启状态，磐石会拒绝还原操作。
请先关闭可写层、恢复只读保护。</source>
        <translation>The writable layer (read-only protection off) is currently enabled, so the immutable system will refuse to restore.
Please disable the writable layer and restore read-only protection first.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>要还原的备份: %1</source>
        <translation>Backup to restore: %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>前往设置</source>
        <translation>Go to settings</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>自动创建回滚前备份，保留待手动删除（推荐）</source>
        <translation>Create a pre-rollback backup and keep it for manual deletion (recommended)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>自动创建，且系统重启 3 次正常后自动删除</source>
        <translation>Create it, then delete automatically after 3 normal restarts</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>不创建回滚前备份（不推荐）</source>
        <translation>Do not create a pre-rollback backup (not recommended)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>还原到备份</source>
        <translation>Restore to backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统将还原到所选备份的状态，重启后生效。</source>
        <translation>The system will be restored to the selected backup; takes effect after reboot.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>无</source>
        <translation>None</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>描述: %1</source>
        <translation>Description: %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>该备份之后对【系统层】的修改（安装的软件、系统配置）将丢失</source>
        <translation>Changes to the system layer after this backup (installed software, system configuration) will be lost</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>用户数据（/home 数据分区）不受影响</source>
        <translation>User data (the /home partition) is not affected</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原期间请勿断电，系统会在失败时自动回退</source>
        <translation>Do not power off during the restore; the system rolls back automatically on failure</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>开始还原</source>
        <translation>Start restore</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>回滚前备份-</source>
        <translation>Pre-rollback backup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>回滚到[%1]前的系统状态</source>
        <translation>System state before rolling back to [%1]</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>正在准备还原...</source>
        <translation>Preparing restore…</translation>
    </message>
    <message>
        <location line="+64"/>
        <source>还原已就绪</source>
        <translation>Restore is ready</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统将在重启后自动还原到所选备份。请先保存您的工作。</source>
        <translation>The system will be restored to the selected backup after reboot. Please save your work first.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>立即重启</source>
        <translation>Restart now</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>稍后手动重启</source>
        <translation>Restart later</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>还原已就绪，请稍后手动重启系统以生效。</source>
        <translation>Restore is ready. Restart the system manually later for it to take effect.</translation>
    </message>
    <message>
        <location line="-483"/>
        <location line="+226"/>
        <source>创建备份</source>
        <translation>Create backup</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>手动备份-</source>
        <translation>Manual backup-</translation>
    </message>
    <message>
        <location line="-239"/>
        <source>暂无备份</source>
        <translation>No backups yet</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>创建备份后可在系统出问题时还原到当前状态。系统初始备份由磐石自动创建，受保护、不可删除。</source>
        <translation>After creating a backup, you can restore the system to its current state if problems occur. The initial system backup is created automatically by Panshi; it is protected and cannot be deleted.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>说明: 系统初始备份受磐石系统保护, 不可删除或重命名。回滚前备份带「回滚前」标识, 可在回滚成功后手动删除。备份描述已并入名称列, 悬停行首即可查看全文。</source>
        <translation>Note: The initial system backup is protected by Panshi and cannot be deleted or renamed. Pre-rollback backups carry a “Pre-rollback” tag and can be deleted manually after a successful rollback. Backup descriptions are merged into the Name column; hover over the start of a row to see the full text.</translation>
    </message>
</context>
<context>
    <name>UpdatePage</name>
    <message>
        <location filename="../src/pages/updatepage.cpp" line="-113"/>
        <source>系统更新</source>
        <translation>System update</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>通过磐石系统原子更新机制升级系统</source>
        <translation>Upgrade the system via Panshi&apos;s atomic update mechanism</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>立即重启</source>
        <translation>Restart now</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>更新说明</source>
        <translation>Update notes</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统升级特性</source>
        <translation>Panshi system upgrade features</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>· 升级前系统自动创建备份, 升级失败自动回滚到之前状态
· 原子更新: 更新过程要么全部成功, 要么回退, 无需手动干预
· 升级会创建新的系统部署, 旧部署按“最大备份部署数”策略保留
· 升级完成需重启后进入新系统</source>
        <translation>· A backup is created automatically before upgrading, and a failed upgrade rolls back automatically · Atomic update: the process either fully succeeds or reverts, with no manual intervention · Upgrading creates a new deployment; old deployments are kept per the “max backup deployments” policy · Reboot after upgrading to enter the new system</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>检查更新</source>
        <translation>Check for updates</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>下载并升级</source>
        <translation>Download and upgrade</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>清理下载缓存</source>
        <translation>Clear download cache</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>正在检查更新...</source>
        <translation>Checking for updates…</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>检查完成</source>
        <translation>Check complete</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>检查更新失败: %1</source>
        <translation>Check for updates failed: %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>将开始下载并升级系统。

升级前系统会自动创建备份; 升级期间请勿关机或断电。
确定继续吗？</source>
        <translation>The system will be downloaded and upgraded. A backup is created automatically before upgrading; do not power off during the upgrade. Continue?</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>开始升级</source>
        <translation>Start upgrade</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>正在下载并升级系统, 请耐心等待...</source>
        <translation>Downloading and upgrading the system. Please wait…</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>系统升级完成，需重启后进入新系统。</source>
        <translation>Upgrade complete. Reboot to enter the new system.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>升级失败: %1</source>
        <translation>Upgrade failed: %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>正在清理下载缓存...</source>
        <translation>Clearing download cache…</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>缓存清理完成</source>
        <translation>Download cache cleared</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>清理失败: %1</source>
        <translation>Clear failed: %1</translation>
    </message>
</context>
<context>
    <name>UsageChart</name>
    <message>
        <location filename="../src/widgets/usagechart.cpp" line="+159"/>
        <source>正在扫描系统盘目录…</source>
        <translation>Scanning system disk directories…</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>正在准备…</source>
        <translation>Preparing…</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>已扫描 %1 项</source>
        <translation>Scanned %1 items</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>已扫描 %1 项 · %2</source>
        <translation>Scanned %1 items · %2</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>已加载 %1/%2 天</source>
        <translation>Loaded %1/%2 days</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>正在检查系统信息…</source>
        <translation>Checking system information…</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>正在加载历史数据…</source>
        <translation>Loading history data…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>正在读取…</source>
        <translation>Reading…</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>%1
已用 %2</source>
        <translation>%1 used %2</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>当日增长 %1</source>
        <translation>That day&apos;s growth %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>⚠ 异常增长</source>
        <translation>⚠ Abnormal growth</translation>
    </message>
    <message>
        <location line="+74"/>
        <source>暂无监控数据：请开启监控，或点击“立即扫描”</source>
        <translation>No monitoring data: enable monitoring, or click “Scan now”</translation>
    </message>
</context>
<context>
    <name>VersionManagementPage</name>
    <message>
        <location filename="../src/pages/versionpage.cpp" line="+42"/>
        <source>备份与版本</source>
        <translation>Backups &amp; versions</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统备份快照与已装系统版本管理</source>
        <translation>Manage system backup snapshots and installed system versions</translation>
    </message>
    <message>
        <location line="-14"/>
        <source>备份快照</source>
        <translation>Snapshots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统版本</source>
        <translation>System versions</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>刷新</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>创建备份</source>
        <translation>Create backup</translation>
    </message>
</context>
</TS>
