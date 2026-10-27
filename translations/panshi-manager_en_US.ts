<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US" sourcelanguage="zh_CN">
<context>
    <name>AboutPage</name>
    <message>
        <location filename="../src/pages/aboutpage.cpp" line="+65"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>关于磐石设置</source>
        <translation>About Panshi Manager</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>磐石系统技术参考</source>
        <translation>Immutable system technical reference</translation>
    </message>
    <message>
        <location line="-32"/>
        <source>技术基础</source>
        <translation>Technology stack</translation>
    </message>
    <message>
        <location line="-10"/>
        <source>版本 %1  ·  Deepin 25 磐石系统管理工具</source>
        <translation>Version %1  ·  Deepin 25 immutable system management tool</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>deepin-immutable-ctl / deepin-immutable-writable / OSTree</source>
        <translation>deepin-immutable-ctl / deepin-immutable-writable / OSTree</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>支持平台</source>
        <translation>Supported platforms</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Deepin 25（X11 / Wayland），DDE 桌面风格（DTK）</source>
        <translation>Deepin 25 (X11 / Wayland), DDE desktop style (DTK)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>作者</source>
        <translation>Author</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Feelup</source>
        <translation>Feelup</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>不可变系统的六条核心机制</source>
        <translation>Six core mechanisms of the immutable system</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>核心目录只读保护</source>
        <translation>Read-only system directories</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>/usr 只读 overlay，/etc /opt 可写 overlay</source>
        <translation>/usr read-only overlay; /etc and /opt writable overlay</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>原子更新</source>
        <translation>Atomic updates</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>升级失败自动回滚（auto-rollback-on-failure）</source>
        <translation>Automatic rollback on upgrade failure (auto-rollback-on-failure)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>快照备份</source>
        <translation>Snapshot backups</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>基于 OSTree 秒级增量快照，支持导出/导入</source>
        <translation>Second-level incremental snapshots based on OSTree, with export/import support</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>初始备份保护</source>
        <translation>Initial backup protection</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>初始备份受系统保护，不可删除/修改</source>
        <translation>The initial backup is system-protected and cannot be deleted or modified</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原需重启</source>
        <translation>Restore requires reboot</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>快照还原需重启生效，可写层开启时禁止还原</source>
        <translation>Snapshot restore takes effect after reboot; restore is disabled while the writable layer is enabled</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>分层存储</source>
        <translation>Layered storage</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>系统与数据分区分层，/home 等数据分区不受还原影响</source>
        <translation>System and data partitions are separated; data partitions such as /home are unaffected by restore</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>常用命令参考</source>
        <translation>Common command reference</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>点击命令右侧图标复制，可直接粘贴到终端执行</source>
        <translation>Click the icon at the right of a command to copy it, then paste it into a terminal</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>列出快照</source>
        <translation>List snapshots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>创建快照</source>
        <translation>Create snapshot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原快照</source>
        <translation>Roll back snapshot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>保护状态</source>
        <translation>Protection status</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>开/关可写层</source>
        <translation>Toggle writable layer</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>部署状态</source>
        <translation>Deployment status</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>复制全部</source>
        <translation>Copy all</translation>
    </message>
    <message>
        <location line="-57"/>
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
        <location filename="../src/pages/deployspage.cpp" line="+337"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="-45"/>
        <location line="+40"/>
        <source>删除部署</source>
        <translation>Delete deployment</translation>
    </message>
    <message>
        <location line="-62"/>
        <source>历史</source>
        <translation>History</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>还原系统时自动创建的安全部署，确认系统正常后可以删除。</source>
        <translation>A safety deployment created automatically before a rollback. Delete it once the system is confirmed working.</translation>
    </message>
    <message>
        <location line="-190"/>
        <source>部署总数</source>
        <translation>Total deployments</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>可删除部署</source>
        <translation>Deletable</translation>
    </message>
    <message>
        <location line="+67"/>
        <source>提示：带「当前启动」「待生效」标记的部署受系统保护，不能删除；要回到某个时间点，请到「系统快照」页还原。</source>
        <translation>Deployments marked Current or Pending are system-protected and cannot be deleted; to return to a point in time, restore from the Snapshots page.</translation>
    </message>
    <message>
        <location line="+125"/>
        <source>已不再使用的旧系统，可删除以释放磁盘空间。</source>
        <translation>An older system no longer in use. Can be deleted to free disk space.</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>这是当前正在运行的系统，不能删除。</source>
        <translation>This is the system currently running. It cannot be deleted.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>这是系统更新后待生效的新系统，重启电脑后才会启用，目前受系统保护，不能删除。</source>
        <translation>This is the new system pending activation after an update. It takes effect after a reboot and is currently protected by the system, so it cannot be deleted.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>该部署缺少标识信息，无法删除，请刷新列表后重试。</source>
        <translation>This deployment has no identifier and cannot be deleted. Refresh the list and try again.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>删除该部署实例以释放磁盘空间，删除后该系统将无法再启动。</source>
        <translation>Delete this deployment to free disk space. Once deleted, that system can no longer boot.</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>删除后该版本的系统将无法再启动，占用空间会被释放。此操作不可撤销。</source>
        <translation>After deletion this system can no longer boot and its disk space is released. This cannot be undone.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>提交ID: %1</source>
        <translation>Commit ID: %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-304"/>
        <source>该版本是系统更新后待生效的新系统，重启电脑后才会启用，目前受系统保护，无法删除。</source>
        <translation>This is the new system pending activation after an update. It takes effect after a reboot and is currently protected, so it cannot be deleted.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>这是当前正在运行的系统版本，无法删除。</source>
        <translation>This is the system currently running. It cannot be deleted.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>找不到对应的系统版本（索引越界或已不存在），请刷新列表后重试。</source>
        <translation>The target deployment was not found (index out of range or already gone). Refresh the list and try again.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>没有获得管理员授权，删除未执行。请确认已正确输入密码；若仍失败，请检查系统的授权（polkit）设置。</source>
        <translation>Administrator authorization was not granted, so nothing was deleted. Check that the password was entered correctly; if it still fails, check the system&apos;s polkit authorization settings.</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>版本号</source>
        <translation>Version</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>暂无系统部署实例</source>
        <translation>No system deployments</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>每次系统升级或回滚都会生成一个新的可启动系统（部署实例）。</source>
        <translation>Every system upgrade or rollback creates a new bootable system (a deployment).</translation>
    </message>
    <message>
        <location line="+120"/>
        <source>待生效</source>
        <translation>Pending</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>此刻正在运行的系统，受系统保护，不能删除。</source>
        <translation>The system running right now. It is protected and cannot be deleted.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>系统更新后待生效的新系统，重启电脑后才会启用，届时它会变成「当前启动」。受系统保护，不能删除。</source>
        <translation>The new system pending activation after an update. It takes effect after a reboot, at which point it becomes &quot;Running&quot;. It is protected and cannot be deleted.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>回滚备份</source>
        <translation>Rollback backup</translation>
    </message>
    <message>
        <location line="-188"/>
        <location line="+180"/>
        <source>当前启动</source>
        <translation>Currently booted</translation>
    </message>
    <message>
        <location line="-164"/>
        <source>提交ID</source>
        <translation>Commit ID</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>系统部署列表</source>
        <translation>System Deployment List</translation>
    </message>
    <message>
        <location line="+203"/>
        <source>删除部署：%1</source>
        <translation>Delete deployment: %1</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>正在删除部署...</source>
        <translation>Deleting deployment...</translation>
    </message>
    <message>
        <location line="-304"/>
        <source>删除失败，请查看详情了解具体原因。</source>
        <translation>Deletion failed. See the details for the specific reason.</translation>
    </message>
    <message>
        <location line="+322"/>
        <source>完整输出</source>
        <translation>Full output</translation>
    </message>
    <message>
        <location line="-277"/>
        <location line="+87"/>
        <source>正在读取部署状态...</source>
        <translation>Reading deployment status...</translation>
    </message>
    <message>
        <location line="-81"/>
        <source>状态</source>
        <translation>Status</translation>
    </message>
    <message>
        <location line="+241"/>
        <source>系统版本: %1</source>
        <translation>System version: %1</translation>
    </message>
    <message>
        <location line="-241"/>
        <source>索引</source>
        <translation>Index</translation>
    </message>
    <message>
        <location line="+265"/>
        <source>部署已删除，占用空间已释放。</source>
        <translation>Deployment deleted and its disk space released.</translation>
    </message>
    <message>
        <location line="-264"/>
        <source>部署时间</source>
        <translation>Deployed at</translation>
    </message>
    <message>
        <location line="+241"/>
        <source>部署时间: %1</source>
        <translation>Deployed at: %1</translation>
    </message>
</context>
<context>
    <name>DiskUsagePage</name>
    <message>
        <location filename="../src/pages/diskusagepage.cpp" line="+442"/>
        <source>磁盘空间</source>
        <translation>Disk usage</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>持续监控系统盘空间，定位容量增长来源</source>
        <translation>Monitor system disk usage continuously and locate the source of growth</translation>
    </message>
    <message>
        <location line="+73"/>
        <location line="+1557"/>
        <source>正在检测服务状态…</source>
        <translation>Detecting service status…</translation>
    </message>
    <message>
        <location line="-1554"/>
        <source>立即启动</source>
        <translation>Start now</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>监控设置</source>
        <translation>Monitoring settings</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>时间线</source>
        <translation>Timeline</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>折线展示每日磁盘占用；点击数据点查看该日增长详情</source>
        <translation>A line chart of daily disk usage; click a data point for that day&apos;s growth details</translation>
    </message>
    <message>
        <location line="-32"/>
        <source>立即扫描</source>
        <translation>Scan now</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+1443"/>
        <source>告警历史</source>
        <translation>Alert History</translation>
    </message>
    <message>
        <location line="-1441"/>
        <source>查看全部历史告警，包括已手动关闭的</source>
        <translation>View all alert history, including manually dismissed ones</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>正常</source>
        <translation>Normal</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>正常增长</source>
        <translation>Normal growth</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>异常</source>
        <translation>Abnormal</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>异常增长</source>
        <translation>Abnormal growth</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>选中</source>
        <translation>Selected</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>选中日</source>
        <translation>Selected day</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>趋势</source>
        <translation>Trend</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>趋势线</source>
        <translation>Trend</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>所选范围内的增长结论与容量预测</source>
        <translation>Growth summary and capacity forecast for the selected range</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>%1 天</source>
        <translation>%1 days</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>自定义</source>
        <translation>Custom</translation>
    </message>
    <message>
        <location line="+79"/>
        <source>容量预测</source>
        <translation>Capacity Forecast</translation>
    </message>
    <message>
        <location line="+59"/>
        <source>增长起点</source>
        <translation>Growth Start</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>磁盘分析</source>
        <translation>Disk Analysis</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>同一份目录占用数据的三种看法：看结构（构成）、看变化（增长）、看全量（排行）</source>
        <translation>Three views of the same directory usage data: structure (Composition), change (Growth), and the full list (Ranking)</translation>
    </message>
    <message>
        <location line="+15"/>
        <location line="+2"/>
        <location line="+321"/>
        <source>返回系统盘</source>
        <translation>Back to system disk</translation>
    </message>
    <message>
        <location line="-543"/>
        <source>区间汇总</source>
        <translation>Range summary</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>净增长</source>
        <translation>Net growth</translation>
    </message>
    <message>
        <location line="+103"/>
        <source>日均增长</source>
        <translation>Daily average</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>最大单日</source>
        <translation>Largest single day</translation>
    </message>
    <message>
        <location line="+106"/>
        <source>名称</source>
        <translation>Name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>大小</source>
        <translation>Size</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>操作</source>
        <translation>Actions</translation>
    </message>
    <message>
        <location line="-33"/>
        <location line="+2"/>
        <source>返回上级</source>
        <translation>Back</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>占比</source>
        <translation>Share</translation>
    </message>
    <message>
        <location line="+481"/>
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
        <location line="-9"/>
        <source> 小时</source>
        <translation> h</translation>
    </message>
    <message>
        <location line="+8"/>
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
        <location line="+3"/>
        <source> 天</source>
        <translation> days</translation>
    </message>
    <message>
        <location line="+8"/>
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
        <location line="+962"/>
        <source>清除全部监控数据</source>
        <translation>Clear all monitoring data</translation>
    </message>
    <message>
        <location line="-960"/>
        <source>数据管理</source>
        <translation>Data management</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+501"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location line="-449"/>
        <source>正在加载 %1 ~ %2 的数据…</source>
        <translation>Loading data for %1 ~ %2…</translation>
    </message>
    <message>
        <location line="+46"/>
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
        <location line="+773"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-772"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location line="+34"/>
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
        <location line="+12"/>
        <source>读取历史数据索引</source>
        <translation>Reading history index</translation>
    </message>
    <message>
        <location line="+60"/>
        <source>%1 · %2 增长 %3</source>
        <translation>%1 · %2 grew %3</translation>
    </message>
    <message>
        <location line="+92"/>
        <source>%1 · %2 · 增长 %3</source>
        <translation>%1 · %2 · +%3</translation>
    </message>
    <message>
        <location line="+44"/>
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
        <location line="+73"/>
        <source>无连续增长</source>
        <translation>No sustained growth</translation>
    </message>
    <message>
        <location line="-1225"/>
        <source>所选日期不在当前加载范围内</source>
        <translation>The selected date is outside the loaded range</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>目录 %1</source>
        <translation>Directory %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>无数据</source>
        <translation>No data</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>较前一日变化</source>
        <translation>Change vs. previous day</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>前一日暂无数据</source>
        <translation>No data for the previous day</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>该日无显著增长</source>
        <translation>No notable growth that day</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>暂无数据：请开启监控或点击“立即扫描”</source>
        <translation>No data: enable monitoring or click “Scan now”</translation>
    </message>
    <message>
        <location line="+1257"/>
        <source>当前显示：系统盘占用</source>
        <translation>Now showing: system disk usage</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>目录“%1”暂无逐日数据，请先开启监控或在该目录下钻</source>
        <translation>No daily data for directory “%1”; enable monitoring or drill into it first</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>当前显示：系统盘占用变化</source>
        <translation>Now showing: system disk usage change</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>当前显示：目录 %1 的占用变化</source>
        <translation>Now showing: usage change for directory %1</translation>
    </message>
    <message>
        <location line="+27"/>
        <location line="+33"/>
        <source>%1 / %2</source>
        <translation>%1 / %2</translation>
    </message>
    <message>
        <location line="-225"/>
        <location line="+253"/>
        <source>监控已开启，正在建立基线（由后台服务采样）</source>
        <translation>Monitoring enabled; establishing a baseline (sampled by the background service)</translation>
    </message>
    <message>
        <location line="-1505"/>
        <source>其它分区</source>
        <translation>Other Partitions</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统盘之外的其它分区（按设备去重，容量/已用取自 df）；点击行右侧按钮可查看该分区内部构成</source>
        <translation>Partitions other than the system disk (deduplicated by device; capacity/used taken from df). Click the button at the right of a row to view that partition&apos;s internal composition</translation>
    </message>
    <message>
        <location line="+176"/>
        <source>其他</source>
        <translation>Other</translation>
    </message>
    <message>
        <location line="+224"/>
        <source>未检测到其它分区（单分区布局）</source>
        <translation>No other partitions detected (single-partition layout)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>%1 · %2 分区
容量 %3 · 已用 %4</source>
        <translation>%1 · %2 partition
Capacity %3 · Used %4</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>查看 %1 内部构成</source>
        <translation>View the internal composition of %1</translation>
    </message>
    <message>
        <location line="+503"/>
        <source>每 %1 分钟采样一次 · 历史保留 %2 天</source>
        <translation>Sampled once every %1 minutes · history kept for %2 days</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>开启后自动建立基线并持续监控</source>
        <translation>Once enabled, a baseline is established automatically and monitoring continues</translation>
    </message>
    <message>
        <location line="+5"/>
        <source> · 最近记录 %1 · 该日增长 %2</source>
        <translation> · Last record %1 · growth that day %2</translation>
    </message>
    <message>
        <location line="+560"/>
        <source>监控已关闭，历史数据保留</source>
        <translation>Monitoring disabled; history retained</translation>
    </message>
    <message>
        <location line="+56"/>
        <source>将删除所有历史占用记录与告警，且不可恢复。确定继续？</source>
        <translation>This will delete all history usage records and alerts, and cannot be undone. Continue?</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>清除</source>
        <translation>Clear</translation>
    </message>
    <message>
        <location line="+27"/>
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
        <location line="-1233"/>
        <location line="+80"/>
        <source>正在读取 %1 的子目录…</source>
        <translation>Reading subdirectories of %1…</translation>
    </message>
    <message>
        <location line="-507"/>
        <source>时间线恢复为系统盘占用，列表回到顶层目录</source>
        <translation>The timeline returns to system disk usage and the list goes back to the top-level directory</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>回到上一层目录</source>
        <translation>Go up one directory level</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>构成</source>
        <translation>Composition</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>增长</source>
        <translation>Growth</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>排行</source>
        <translation>Ranking</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>磁盘分析表</source>
        <translation>Disk analysis table</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>构成：系统盘顶层目录的占用结构（前 5 项 + 其他），占比 = 占系统盘已用</source>
        <translation>Composition: usage structure of the top-level system disk directories (top 5 + others); percentage = share of used system disk space</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>增长：所选日期相对前一日有增长的目录/文件（最多 8 项），占比 = 该目录增量占当日总增量</source>
        <translation>Growth: directories/files that grew on the selected date versus the previous day (up to 8 items); percentage = that directory&apos;s increase as a share of the day&apos;s total increase</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>排行：当前目录下所有子项从大到小；单击行让上方时间线跟随，双击进入子目录</source>
        <translation>Ranking: all sub-items under the current directory, largest first; click a row to make the timeline above follow it, double-click to enter a subdirectory</translation>
    </message>
    <message>
        <location line="+81"/>
        <source>点击时间线上任意数据点，查看该日的增长来源</source>
        <translation>Click any data point on the timeline to see that day&apos;s growth sources</translation>
    </message>
    <message>
        <location line="+44"/>
        <source>正在读取子目录…</source>
        <translation>Reading subdirectory…</translation>
    </message>
    <message>
        <location line="+117"/>
        <source>系统盘</source>
        <translation>System Disk</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>返回系统盘时间线与顶层目录</source>
        <translation>Return to the system disk timeline and top-level directory</translation>
    </message>
    <message>
        <location line="+30"/>
        <location line="+2"/>
        <source>回到 %1</source>
        <translation>Back to %1</translation>
    </message>
    <message>
        <location line="+725"/>
        <source>告警历史（%1 条新增）</source>
        <translation>Alert History (%1 new)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>告警历史（%1）</source>
        <translation>Alert History (%1)</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>告警已关闭，可随时在「告警历史」中查看</source>
        <translation>Alert dismissed; you can review it anytime in &quot;Alert History&quot;</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>历史告警</source>
        <translation>Alert History</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>暂无告警记录</source>
        <translation>No alert records</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>按时间倒序；手动关闭过的告警仍保留在此</source>
        <translation>In reverse chronological order; manually dismissed alerts are still kept here</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>清空历史</source>
        <translation>Clear History</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>清空告警历史</source>
        <translation>Clear alert history</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>将删除全部告警记录，且不可恢复。确定继续？</source>
        <translation>This will delete all alert records and cannot be undone. Continue?</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>清空</source>
        <translation>Clear</translation>
    </message>
    <message>
        <location line="+158"/>
        <source>%1 ~ %2 · %3 天 · %4 条记录</source>
        <translation>%1 ~ %2 · %3 days · %4 records</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>区间起始 %1 → 结束 %2</source>
        <translation>Range start %1 → end %2</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>所选范围内暂无监控数据</source>
        <translation>No monitoring data in the selected range</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>约 %1 天后写满</source>
        <translation>Will be full in about %1 days</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>按当前增速 · 预计 %1</source>
        <translation>At the current growth rate · estimated %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>暂无风险</source>
        <translation>No risk</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>增长已趋稳，短期不会写满</source>
        <translation>Growth has stabilized; it will not fill up in the short term</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>暂无数据，无法预测</source>
        <translation>No data; unable to forecast</translation>
    </message>
    <message>
        <location line="+170"/>
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
        <location line="-238"/>
        <location line="+239"/>
        <source>未知错误</source>
        <translation>unknown error</translation>
    </message>
    <message>
        <location line="-239"/>
        <source>启动失败：%1</source>
        <translation>Failed to start: %1</translation>
    </message>
    <message>
        <location line="-60"/>
        <source>无法连接 systemd 用户会话</source>
        <translation>Cannot connect to the systemd user session</translation>
    </message>
    <message>
        <location line="+457"/>
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
        <location filename="../src/mainwindow.cpp" line="+43"/>
        <location line="+5"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <source>概览</source>
        <translation type="vanished">Overview</translation>
    </message>
    <message>
        <source>系统维护</source>
        <translation type="vanished">System maintenance</translation>
    </message>
    <message>
        <source>设置</source>
        <translation type="vanished">Settings</translation>
    </message>
    <message>
        <location line="+102"/>
        <source>总览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>磁盘空间</source>
        <translation>Disk usage</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>系统保护</source>
        <translation>System protection</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>备份与版本</source>
        <translation>Backups</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>系统更新</source>
        <translation>System update</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>策略与设置</source>
        <translation>Policies &amp; settings</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>语言</source>
        <translation>Language</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>跟随系统</source>
        <translation>Follow system</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>简体中文</source>
        <translation>Simplified Chinese</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>English</source>
        <translation>English</translation>
    </message>
</context>
<context>
    <name>OverviewPage</name>
    <message>
        <location filename="../src/pages/overviewpage.cpp" line="+202"/>
        <source>总览</source>
        <translation>Overview</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统运行状态一览</source>
        <translation>At a glance: Panshi system status</translation>
    </message>
    <message>
        <location line="+46"/>
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
        <source>系统快照</source>
        <translation>System snapshots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统盘剩余</source>
        <translation>System disk free</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>可用空间</source>
        <translation>Available Space</translation>
    </message>
    <message>
        <location line="+7"/>
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
        <location line="+172"/>
        <location line="+38"/>
        <source>%1 个</source>
        <translation>%1</translation>
    </message>
    <message>
        <location line="-236"/>
        <source>存储空间</source>
        <translation>Storage</translation>
    </message>
    <message>
        <location line="-63"/>
        <source>刷新状态</source>
        <translation>Refresh Status</translation>
    </message>
    <message>
        <location line="+63"/>
        <source>磐石系统各仓库占用（物理）</source>
        <translation>Physical usage of each Panshi repository</translation>
    </message>
    <message>
        <location line="+9"/>
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
        <location line="+25"/>
        <source>快速操作</source>
        <translation>Quick actions</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>常用功能快捷入口</source>
        <translation>Shortcuts to common features</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>立即创建备份</source>
        <translation>Create backup now</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>检查系统更新</source>
        <translation>Check for updates</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>保护设置</source>
        <translation>Protection settings</translation>
    </message>
    <message>
        <location line="+81"/>
        <source>正在读取磐石系统状态…</source>
        <translation>Reading Panshi system status…</translation>
    </message>
    <message>
        <location line="+55"/>
        <location line="+4"/>
        <location line="+1"/>
        <location line="+1"/>
        <location line="+1"/>
        <source>统计中…</source>
        <translation>Calculating…</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>正在统计存储空间…</source>
        <translation>Calculating storage usage…</translation>
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
        <location filename="../src/disk/scanner.cpp" line="+323"/>
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
        <location filename="../src/widgets/countdowndialog.cpp" line="+84"/>
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
        <location line="+32"/>
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
        <source>只读保护</source>
        <translation>Read-only protection</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>可写层</source>
        <translation>Writable layer</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>重启后状态</source>
        <translation>State after reboot</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>已开启</source>
        <translation>Enabled</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>已关闭</source>
        <translation>Disabled</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>核心目录只读</source>
        <translation>Core directories read-only</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>核心目录可写</source>
        <translation>Core directories writable</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>已解锁</source>
        <translation>Unlocked</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>已锁定</source>
        <translation>Locked</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>开发调试模式</source>
        <translation>Debug mode</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>未启用</source>
        <translation>Not enabled</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>恢复只读</source>
        <translation>Restores read-only</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>仍保持可写</source>
        <translation>Stays writable</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>临时关闭</source>
        <translation>Temporarily off</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>永久关闭</source>
        <translation>Permanently off</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>仍为只读</source>
        <translation>Read-only</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>未做变更</source>
        <translation>No change</translation>
    </message>
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
        <location filename="../src/ui/aboutdialog.cpp" line="+278"/>
        <source>关于磐石设置</source>
        <translation>About Panshi Manager</translation>
    </message>
    <message>
        <location line="+121"/>
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
        <location line="+6"/>
        <source>磐石系统技术参考</source>
        <translation>Panshi system technical reference</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>不可变系统的六条核心机制</source>
        <translation>Six core mechanisms of the immutable system</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>核心目录只读保护</source>
        <translation>Read-only protection for core directories</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>/usr 只读 overlay，/etc /opt 可写 overlay</source>
        <translation>/usr is a read-only overlay; /etc and /opt are writable overlays</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>原子更新</source>
        <translation>Atomic updates</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>升级失败自动回滚（auto-rollback-on-failure）</source>
        <translation>Rolls back automatically if an upgrade fails (auto-rollback-on-failure)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>快照备份</source>
        <translation>Snapshot backups</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>基于 OSTree 秒级增量快照，支持导出/导入</source>
        <translation>OSTree-based incremental snapshots in seconds, with export and import</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>初始备份保护</source>
        <translation>Initial backup protection</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>初始备份受系统保护，不可删除/修改</source>
        <translation>The initial backup is protected by the system and cannot be deleted or modified</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原需重启</source>
        <translation>Rollback needs a reboot</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>快照还原需重启生效，可写层开启时禁止还原</source>
        <translation>A rollback takes effect after a reboot and is blocked while the writable layer is on</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>分层存储</source>
        <translation>Layered storage</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>系统与数据分区分层，/home 等数据分区不受还原影响</source>
        <translation>System and data are layered; data partitions such as /home are unaffected by a rollback</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>点击命令右侧图标复制，可直接粘贴到终端执行</source>
        <translation>Click the icon at the right of a command to copy it, then paste it into a terminal</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>列出快照</source>
        <translation>List snapshots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>创建快照</source>
        <translation>Create a snapshot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原快照</source>
        <translation>Roll back to a snapshot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>保护状态</source>
        <translation>Protection status</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>开/关可写层</source>
        <translation>Enable or disable the writable layer</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>部署状态</source>
        <translation>Deployment status</translation>
    </message>
    <message>
        <source>· 核心目录只读保护: /usr 只读 overlay, /etc /opt 可写 overlay
· 原子更新: 升级失败自动回滚 (auto-rollback-on-failure)
· 快照备份: 基于 OSTree 秒级增量快照, 支持导出/导入
· 初始备份受系统保护, 不可删除/修改
· 快照还原需重启生效, 可写层开启时禁止还原
· 系统与数据分区分层: /home 等数据分区不受还原影响</source>
        <translation type="vanished">· Read-only protection: /usr is a read-only overlay, /etc and /opt are writable overlays
· Atomic updates: failed upgrades roll back automatically (auto-rollback-on-failure)
· Snapshot backup: second-level incremental snapshots based on OSTree, export/import supported
· The initial backup is system-protected and cannot be deleted or modified
· Restoring a snapshot takes effect after reboot; not allowed while the writable layer is enabled
· System and data are layered: data partitions such as /home are unaffected by restore</translation>
    </message>
    <message>
        <location line="-8"/>
        <source>常用命令参考</source>
        <translation>Common commands reference</translation>
    </message>
</context>
<context>
    <name>PolicyPage</name>
    <message>
        <location filename="../src/pages/policypage.cpp" line="+54"/>
        <source>策略与设置</source>
        <translation>Policies &amp; settings</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>备份保留策略与磐石系统参数</source>
        <translation>Backup retention policies and immutable system parameters</translation>
    </message>
    <message>
        <location line="+123"/>
        <location line="+56"/>
        <source>磐石系统参数</source>
        <translation>Immutable system parameters</translation>
    </message>
    <message>
        <location line="-153"/>
        <source>备份数量上限</source>
        <translation>Backup count limit</translation>
    </message>
    <message>
        <location line="+103"/>
        <source>保存系统参数</source>
        <translation>Save system parameters</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>升级失败自动回滚</source>
        <translation>Auto-rollback on failed upgrade</translation>
    </message>
    <message>
        <location line="-47"/>
        <location line="+80"/>
        <source> 个</source>
        <translation> items</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>自动刷新阈值</source>
        <translation>Auto-refresh threshold</translation>
    </message>
    <message>
        <location line="-119"/>
        <location line="+160"/>
        <source>备份保留</source>
        <translation>Backup Retention</translation>
    </message>
    <message>
        <location line="-159"/>
        <source>系统升级会自动创建备份（系统内置，不可关闭）。以下策略控制备份保留与清理，避免无限累积。</source>
        <translation>System upgrades automatically create backups (built-in, cannot be disabled). The policies below control backup retention and cleanup to prevent unlimited accumulation.</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+51"/>
        <source>即时生效</source>
        <translation>Takes Effect Immediately</translation>
    </message>
    <message>
        <location line="-45"/>
        <source>0 表示不限；超出后按清理策略处理</source>
        <translation>0 means unlimited; when exceeded, backups are handled per the cleanup policy</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>超限自动清理</source>
        <translation>Auto-clean Excess Backups</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>仅删除最旧普通备份；初始备份永不删除</source>
        <translation>Only the oldest normal backup is removed; the initial backup is never deleted</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>上限为不限时无需自动清理</source>
        <translation>No auto-cleanup needed while the limit is unlimited</translation>
    </message>
    <message>
        <location line="+18"/>
        <location line="+109"/>
        <source>更新与回滚</source>
        <translation>Update &amp; Rollback</translation>
    </message>
    <message>
        <location line="-108"/>
        <source>以下策略控制系统升级失败时的回滚与清理流程。</source>
        <translation>These policies control rollback and cleanup when a system upgrade fails.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>失败时回滚到升级前状态，避免系统无法启动</source>
        <translation>Rolls back to the pre-upgrade state on failure to prevent an unbootable system</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>回滚前备份清理</source>
        <translation>Pre-Rollback Backup Cleanup</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>还原前自动创建回滚前备份；回滚成功后按所选方式清理</source>
        <translation>A pre-rollback backup is created before restoring, and cleaned up per the chosen policy after a successful rollback</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>手动清理（默认）</source>
        <translation>Manual cleanup (default)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>重启 3 次正常后自动清理</source>
        <translation>Auto-cleanup after 3 successful reboots</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>保留旧部署数</source>
        <translation>Retained Deployments</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>升级/回滚时保留的旧系统部署数量（默认 1，新备份自动覆盖旧备份）</source>
        <translation>Number of old system deployments kept during upgrade/rollback (default 1; a new backup automatically overwrites the old one)</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>以下参数写入系统配置文件，需管理员权限；修改后点击「保存系统参数」生效。</source>
        <translation>These parameters are written to the system configuration file and require administrator privileges; changes take effect after clicking Save System Parameters.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>有未保存修改</source>
        <translation>Unsaved Changes</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>修改层超过该阈值时自动刷新合并到系统层；建议 200–1000 MiB</source>
        <translation>When the modification layer exceeds this threshold, it is automatically flushed and merged into the system layer; recommended 200-1000 MiB</translation>
    </message>
    <message>
        <location line="+3"/>
        <source> MiB</source>
        <translation> MiB</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>配置文件</source>
        <translation>Configuration File</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>保存系统参数的目标文件</source>
        <translation>Target file where system parameters are saved</translation>
    </message>
    <message>
        <location line="+81"/>
        <source>正在读取配置...</source>
        <translation>Reading configuration…</translation>
    </message>
    <message>
        <location line="+51"/>
        <source>正在保存系统参数...</source>
        <translation>Saving system parameters…</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>系统参数已保存</source>
        <translation>System parameters saved</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>保存失败: %1</source>
        <translation>Save failed: %1</translation>
    </message>
</context>
<context>
    <name>ProtectionPage</name>
    <message>
        <location filename="../src/pages/protectionpage.cpp" line="+35"/>
        <source>系统保护</source>
        <translation>System protection</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统只读保护(可写层)管理</source>
        <translation>Manage Panshi read-only protection (writable layer)</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>总开关：开启或关闭磐石系统保护</source>
        <translation>Master switch: enables or disables Panshi system protection</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>说明</source>
        <translation>Description</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>磐石系统将核心目录(/usr 等)以只读方式挂载, 防止误操作或恶意软件破坏系统。
开启“可写层”将解除只读保护(适合开发调试), 关闭后恢复保护。</source>
        <translation>The Panshi system mounts core directories (such as /usr) read-only to prevent accidental operations or malware from damaging the system.
Enabling the &quot;writable layer&quot; removes the read-only protection (suitable for development and debugging); turning it off restores the protection.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>注意: 开启可写层后系统将拒绝执行快照还原。</source>
        <translation>Note: while the writable layer is enabled, snapshot rollback is refused.</translation>
    </message>
    <message>
        <location line="-19"/>
        <location line="+39"/>
        <source>保护开关</source>
        <translation>Protection switch</translation>
    </message>
    <message>
        <location line="-11"/>
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
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/main.cpp" line="+169"/>
        <location line="+1"/>
        <source>磐石设置</source>
        <translation>Panshi Manager</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Deepin 25 磐石系统管理工具</source>
        <translation>Deepin 25 immutable system management tool</translation>
    </message>
    <message>
        <location filename="../src/pages/common.h" line="+359"/>
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
        <location filename="../src/pages/aboutpage.cpp" line="+4"/>
        <location line="+61"/>
        <location filename="../src/pages/common.h" line="+16"/>
        <source>已复制</source>
        <translation>Copied</translation>
    </message>
    <message>
        <location line="-59"/>
        <source>复制版本信息</source>
        <translation>Copy version info</translation>
    </message>
    <message>
        <location line="+61"/>
        <source>复制全部</source>
        <translation>Copy all</translation>
    </message>
    <message>
        <location line="-64"/>
        <source>磐石设置  </source>
        <translation>Panshi Manager  </translation>
    </message>
    <message>
        <location filename="../src/pages/common.h" line="-276"/>
        <location filename="../src/ui/confirmdialog.cpp" line="+106"/>
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
        <location line="+32"/>
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
        <location filename="../src/ui/sidebar.cpp" line="+201"/>
        <source>主导航</source>
        <translation>Main navigation</translation>
    </message>
    <message>
        <location filename="../src/backend.cpp" line="+79"/>
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
        <location line="+828"/>
        <source>命令执行失败</source>
        <translation>Command execution failed</translation>
    </message>
    <message>
        <location line="-312"/>
        <source>无法写入临时配置文件</source>
        <translation>Cannot write temporary config file</translation>
    </message>
    <message>
        <location line="+162"/>
        <source>无法写入临时状态文件</source>
        <translation>Cannot write temporary state file</translation>
    </message>
    <message>
        <location line="+82"/>
        <source>安装 systemd 服务失败</source>
        <translation>Failed to install systemd service</translation>
    </message>
    <message>
        <location line="+73"/>
        <source>已取消授权，未执行操作</source>
        <translation>Authorization cancelled, no action taken</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>没有权限执行此操作</source>
        <translation>Not authorized to perform this operation</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>身份验证失败，请检查密码后重试</source>
        <translation>Authentication failed, please check your password and try again</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>密码错误，请重试</source>
        <translation>Incorrect password, please try again</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>权限不足，无法执行此操作</source>
        <translation>Insufficient permissions to perform this operation</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>命令执行超时</source>
        <translation>Command execution timed out</translation>
    </message>
    <message>
        <location filename="../src/disk/scanner.cpp" line="-246"/>
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
        <location filename="../src/pages/diskusagepage.cpp" line="-2338"/>
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
        <location filename="../src/pages/updatepage.cpp" line="+78"/>
        <location line="+52"/>
        <source>内核与驱动</source>
        <translation>Kernel &amp; drivers</translation>
    </message>
    <message>
        <location line="-46"/>
        <location line="+46"/>
        <source>桌面环境</source>
        <translation>Desktop environment</translation>
    </message>
    <message>
        <location line="-41"/>
        <location line="+41"/>
        <source>输入法与字体</source>
        <translation>Input methods &amp; fonts</translation>
    </message>
    <message>
        <location line="-37"/>
        <location line="+38"/>
        <source>系统库文件</source>
        <translation>System libraries</translation>
    </message>
    <message>
        <location line="-34"/>
        <location line="+34"/>
        <source>网络应用</source>
        <translation>Network applications</translation>
    </message>
    <message>
        <location line="-30"/>
        <location line="+30"/>
        <source>影音与图形</source>
        <translation>Multimedia &amp; graphics</translation>
    </message>
    <message>
        <location line="-22"/>
        <location line="+23"/>
        <source>开发环境</source>
        <translation>Development tools</translation>
    </message>
    <message>
        <location line="-20"/>
        <location line="+20"/>
        <source>游戏</source>
        <translation>Games</translation>
    </message>
    <message>
        <location line="-16"/>
        <location line="+16"/>
        <source>系统组件</source>
        <translation>System components</translation>
    </message>
    <message>
        <location line="-10"/>
        <location line="+11"/>
        <source>工具与应用</source>
        <translation>Tools &amp; applications</translation>
    </message>
    <message>
        <location line="-10"/>
        <location line="+10"/>
        <source>其他</source>
        <translation>Other</translation>
    </message>
    <message>
        <location line="+342"/>
        <source>系统升级</source>
        <translation>System upgrade</translation>
    </message>
    <message>
        <location filename="../src/ui/components.cpp" line="+881"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关闭提示</source>
        <translation>Close notice</translation>
    </message>
    <message>
        <location line="+529"/>
        <source>展开或收起</source>
        <translation>Expand or collapse</translation>
    </message>
    <message>
        <location line="+194"/>
        <source>收起</source>
        <translation>Collapse</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>展开</source>
        <translation>Expand</translation>
    </message>
    <message>
        <source>概览</source>
        <translation type="vanished">Overview</translation>
    </message>
    <message>
        <source>系统维护</source>
        <translation type="vanished">Maintenance</translation>
    </message>
    <message>
        <source>设置</source>
        <translation type="vanished">Settings</translation>
    </message>
    <message>
        <location filename="../src/ui/aboutdialog.cpp" line="-372"/>
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
    <message>
        <source>条记录</source>
        <translation type="vanished"> records</translation>
    </message>
    <message>
        <source>增长</source>
        <translation type="vanished">Growth</translation>
    </message>
    <message>
        <source>排行</source>
        <translation type="vanished">Ranking</translation>
    </message>
    <message>
        <source>磁盘分析表</source>
        <translation type="vanished">Disk analysis table</translation>
    </message>
    <message>
        <source>较前一日变化</source>
        <translation type="vanished">Change vs. previous day</translation>
    </message>
    <message>
        <source>前一日暂无数据</source>
        <translation type="vanished">No data for the previous day</translation>
    </message>
    <message>
        <source>净增长</source>
        <translation type="vanished">Net growth</translation>
    </message>
    <message>
        <source>容量预测</source>
        <translation type="vanished">Capacity Forecast</translation>
    </message>
    <message>
        <source>预计</source>
        <translation type="vanished">Estimated</translation>
    </message>
    <message>
        <source>系统盘</source>
        <translation type="vanished">System Disk</translation>
    </message>
    <message>
        <source>构成</source>
        <translation type="vanished">Composition</translation>
    </message>
    <message>
        <source>返回系统盘</source>
        <translation type="vanished">Back to system disk</translation>
    </message>
    <message>
        <source>返回上级</source>
        <translation type="vanished">Back</translation>
    </message>
    <message>
        <source>增长：</source>
        <translation type="vanished">Growth:</translation>
    </message>
    <message>
        <source>检查更新</source>
        <translation type="vanished">Check for updates</translation>
    </message>
    <message>
        <source>有可用更新</source>
        <translation type="vanished">Updates available</translation>
    </message>
    <message>
        <source>已是最新</source>
        <translation type="vanished">Up to date</translation>
    </message>
    <message>
        <source>待更新软件包</source>
        <translation type="vanished">Packages to update</translation>
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
        <location filename="../src/pages/snapshotspage.cpp" line="+102"/>
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
        <location line="-10"/>
        <location line="+117"/>
        <source>正在读取快照列表...</source>
        <translation>Reading snapshot list…</translation>
    </message>
    <message>
        <location line="-98"/>
        <source>系统备份列表</source>
        <translation>System Backup List</translation>
    </message>
    <message>
        <location line="+150"/>
        <source>未启用</source>
        <translation>Not enabled</translation>
    </message>
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
        <location line="-13"/>
        <source>不限制</source>
        <translation>Unlimited</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 个</source>
        <translation>%1 backup(s)</translation>
    </message>
    <message>
        <location line="+62"/>
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
        <location line="+108"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="-106"/>
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
        <location line="+17"/>
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
        <location line="-403"/>
        <source>备份数量上限</source>
        <translation>Backup count limit</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>当前数量</source>
        <translation>Current count</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>回滚前安全备份</source>
        <translation>Pre-rollback safe backup</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>提示：悬停名称列可查看完整描述与用途；系统初始备份受磐石保护，不可删除。</source>
        <translation>Tip: hover the name column to see the full description; the initial system backup is protected and cannot be deleted.</translation>
    </message>
    <message>
        <location line="+304"/>
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
        <location line="-491"/>
        <location line="+234"/>
        <source>创建备份</source>
        <translation>Create backup</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>手动备份-</source>
        <translation>Manual backup-</translation>
    </message>
    <message>
        <location line="-247"/>
        <source>暂无备份</source>
        <translation>No backups yet</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>创建备份后可在系统出问题时还原到当前状态。系统初始备份由磐石自动创建，受保护、不可删除。</source>
        <translation>After creating a backup, you can restore the system to its current state if problems occur. The initial system backup is created automatically by Panshi; it is protected and cannot be deleted.</translation>
    </message>
</context>
<context>
    <name>UpdatePage</name>
    <message>
        <location filename="../src/pages/updatepage.cpp" line="-318"/>
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
        <location line="+8"/>
        <source>更新检查</source>
        <translation>Update check</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>检查系统是否有可用的更新</source>
        <translation>Check whether updates are available</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>尚未检查更新，点击下方「检查更新」获取最新状态。</source>
        <translation>Not checked yet. Click &quot;Check for updates&quot; below to get the latest status.</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+99"/>
        <source>未检查</source>
        <translation>Not checked</translation>
    </message>
    <message>
        <location line="-80"/>
        <source>更新说明</source>
        <translation>Update notes</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>磐石系统升级特性</source>
        <translation>Panshi system upgrade features</translation>
    </message>
    <message>
        <location line="+16"/>
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
        <location line="-22"/>
        <source>自动备份与回滚</source>
        <translation>Automatic backup and rollback</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>待更新软件包</source>
        <translation>Packages to update</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>按用途分组，便于了解更新内容</source>
        <translation>Grouped by purpose, so you can see what will be installed</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>升级前系统自动创建备份，升级失败自动回滚到之前状态</source>
        <translation>The system automatically creates a backup before upgrading and rolls back to the previous state if the upgrade fails</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>原子更新</source>
        <translation>Atomic updates</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>更新过程要么全部成功，要么整体回退，无需手动干预</source>
        <translation>Updates either fully succeed or fully roll back, with no manual intervention needed</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>部署保留策略</source>
        <translation>Deployment retention policy</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>升级会创建新的系统部署，旧部署按「最大备份部署数」策略保留</source>
        <translation>Upgrades create a new system deployment; old deployments are kept per the &quot;max backup deployments&quot; policy</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>重启生效</source>
        <translation>Takes effect after reboot</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>升级完成后需重启进入新系统</source>
        <translation>A reboot is required to enter the new system after the upgrade</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>更新详情</source>
        <translation>Update details</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>已是最新</source>
        <translation>Up to date</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>有可用更新</source>
        <translation>Updates available</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>检查失败</source>
        <translation>Check failed</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>可更新项：0（系统已是最新）</source>
        <translation>Updates: 0 (system is up to date)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>可更新项：%1</source>
        <translation>Updates: %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>可更新项：%1&#x3000;需下载：%2</source>
        <translation>Updates: %1&#x3000;to download: %2</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>%1 项</source>
        <translation>%1 items</translation>
    </message>
    <message>
        <location line="+36"/>
        <location line="+1"/>
        <source>正在检查更新...</source>
        <translation>Checking for updates…</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>系统已是最新版本，无需更新。</source>
        <translation>The system is already up to date. No updates needed.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>发现 %1 个可更新项。</source>
        <translation>Found %1 available update(s).</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>发现 %1 个可更新项，需下载 %2 个。</source>
        <translation>Found %1 available update(s), %2 to download.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>检查完成，未获取到更新摘要。</source>
        <translation>Check complete, but no update summary was returned.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>检查完成</source>
        <translation>Check complete</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>检查更新失败</source>
        <translation>Check for updates failed</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>检查更新失败: %1</source>
        <translation>Check for updates failed: %1</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>未知错误</source>
        <translation>Unknown error</translation>
    </message>
    <message>
        <location line="+12"/>
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
        <location line="+13"/>
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
        <location filename="../src/pages/versionpage.cpp" line="+49"/>
        <source>备份与版本</source>
        <translation>Backups &amp; versions</translation>
    </message>
    <message>
        <location line="-14"/>
        <location line="+148"/>
        <source>系统快照</source>
        <translation>System snapshots</translation>
    </message>
    <message>
        <location line="-147"/>
        <location line="+149"/>
        <source>系统部署实例</source>
        <translation>System deployments</translation>
    </message>
    <message>
        <location line="-135"/>
        <source>系统快照是可还原的备份点，系统部署实例是可启动的系统</source>
        <translation>Restorable backup points; deployments are bootable systems</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>刷新</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>创建备份</source>
        <translation>Create backup</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>「系统快照」和「系统部署实例」有什么区别？</source>
        <translation>How do &quot;System snapshots&quot; and &quot;System deployments&quot; differ?</translation>
    </message>
    <message>
        <location line="+76"/>
        <source>这里记录的是「可还原的备份点」：可以创建、删除，也可以把系统还原回它记录的那一刻。</source>
        <translation>This tab lists restorable backup points: you can create and delete them, and roll the system back to the moment a snapshot records.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>这里列出的是「一整套可启动的系统」：只能删除以释放磁盘空间，删除后该系统无法再启动。</source>
        <translation>This tab lists complete bootable systems: they can only be deleted to free disk space, and once deleted that system can no longer boot.</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>对比项</source>
        <translation>Aspect</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>是什么</source>
        <translation>What it is</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>怎么来的</source>
        <translation>Where it comes from</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>能做什么</source>
        <translation>What you can do</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>删除后</source>
        <translation>After deletion</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>可还原的备份点，只记录系统层的差异</source>
        <translation>A restorable backup point; stores only the differences of the system layer</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>你手动创建，或系统初始化时自动生成</source>
        <translation>Created by you manually, or automatically at system initialization</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>可以创建、删除，也可以还原回它记录的时刻</source>
        <translation>Can be created, deleted, and rolled back to the moment it records</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>只是少一个还原点，系统照常运行</source>
        <translation>You just lose one restore point; the system keeps running</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>一整套可启动的系统，占用完整磁盘空间</source>
        <translation>A complete bootable system; occupies its full disk space</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>系统升级、回滚时自动生成</source>
        <translation>Generated automatically on system upgrade or rollback</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>只能删除，用来释放磁盘空间</source>
        <translation>Can only be deleted, to free disk space</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>该系统将无法再启动</source>
        <translation>That system will no longer be bootable</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>两者关系：还原一个快照后，系统会自动生成一个新的部署实例（带「回滚前」标记），可在「系统部署实例」页看到它。</source>
        <translation>How they relate: after you roll back a snapshot, the system automatically creates a new deployment (marked &quot;Pre-rollback&quot;), which you can see in the &quot;System deployments&quot; tab.</translation>
    </message>
</context>
</TS>
