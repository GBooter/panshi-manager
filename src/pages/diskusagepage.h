// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"
#include "disk/history.h"
#include "disk/scanner.h"
#include "disk/monitorconfig.h"

#include <DLabel>
#include <DSwitchButton>
#include <DComboBox>
#include <DSpinBox>
#include <DDoubleSpinBox>
#include <DLineEdit>
#include <DListView>
#include <DProgressBar>
#include <DDialog>
#include <DButtonBox>

#include <QDate>
#include <QJsonArray>
#include <QList>
#include <QDateTime>
#include <QPair>

class MainWindow;
class UsageChart;
class QTableWidget;
class QFileSystemWatcher;
class QTimer;
class QScrollArea;
class QVBoxLayout;

/**
 * 磁盘占用页：概览 / 时间线 / 区间汇总 / 磁盘分析（整合区） / 独立设置窗口。
 *
 * 本轮（2026-10-10）信息架构整合 —— 起因是三块内容**数据源重复**：
 *  - 「磁盘构成」= 最新一天 topLevel 前 5 + 其他（区间汇总卡内）；
 *  - 「占用排行」= 最新一天 topLevel 全部 ≤200（标签页内）；
 *   两者取同一份 `latest.topLevel`，却在两处各渲染一遍，口径/排序/占比分母
 *   各写一份必然分叉；「增长详情」又是第三份列表与第三套交互。
 *
 * 整合为**一个「磁盘分析」区 + 视角分段切换**（构成 / 增长 / 排行）：
 *  - 一张表、一套列（名称 | 大小 | 占比 | 操作），三个视角只换数据源与说明；
 *  - 占比列内嵌细条，把原「磁盘构成」的占比条并入表格 —— 三视角都有占比可视化；
 *  - 交互统一：单击行 = 选中该目录并让上方时间线跟随；双击目录行 = 下钻进入；
 *    「增长」视角不下钻（下钻会脱离「当日增量」口径）。
 *
 * 「返回系统盘」随之重新设计：
 *  - 原按钮埋在时间线卡底部（离"选目录"的操作区太远，用户反馈不方便）；
 *  - 现改为分析区顶部的**面包屑**：「系统盘 › var › log」每一级可点击回退，
 *    最左「系统盘」即返回系统盘；旁边保留「← 上级」逐级回退；
 *  - 时间线卡底部只留一行状态文字（不再放按钮），避免两处按钮语义打架。
 *
 * 告警改为**可关闭 + 可回顾**：
 *  - 横幅右上角「×」→ 发 `dismissed()` → 记进 dismissed 列表（持久化到本地
 *    UI 状态文件），本次及之后不再自动弹出；
 *  - 关闭**不从数据源删除**，概览卡新增「告警历史（N）」入口，打开后仍可查看全部
 *    历史告警（含被关闭的），并提供「清空历史」。
 *
 * 保留的数据逻辑（未改）：时间线按范围懒加载、QFileSystemWatcher 自动刷新、
 * pkexec 提权扫描与实时进度、排行下钻与打开位置、容量预测。
 *
 * 历史（方案 C，阶段 3-9）：
 *  - 「立即扫描」从时间线卡移入概览卡，与「监控设置」并排 —— 操作入口集中到一处；
 *  - 概览卡状态行重构：服务徽章承载运行状态，采样参数 + 最近记录合并为单行说明；
 *  - 时间线卡瘦身：图例 + DButtonBox 分段范围选择同行；「当前显示/已加载」合并为
 *    图表下方单行；
 *  - 「区间汇总」从标签页上移为常驻卡（结论零切换可见）。
 *
 * 本轮对齐组件层（与 3-1～3-6 一致）：
 *  - Ui::card() → PanshiPanel / PanshiSection（层次靠底色差 + 圆角，不描边）；
 *  - 服务状态徽章、告警、容量预测提示条 → PanshiBadge / PanshiInlineBanner
 *    （不再用 objectName 切 QSS 的 panshiTagXxx / panshiWarnBox / panshiTipBar）；
 *  - 区间汇总 4 个数值卡 → PanshiStateCard（语义色由组件承担，不用 stat 属性）；
 *  - 表格列宽按默认视口倒推 + ElidedLabel 承载长路径 + 窄窗横向滚动兜底
 *    （与 SnapshotsPage / DeploysPage 同一方案，见 common.h 注释）；
 *  - 设置窗口分组 → PanshiSection + PanshiRow。
 *
 * 阶段 3-7b（Qt 原生控件收尾，本轮）：
 *  - 概览容量条 `QProgressBar` → `DProgressBar`（DTK 原生外观，颜色职责归样式表）；
 *  - 设置窗口「基线倍数」`QDoubleSpinBox` → `DDoubleSpinBox`（DTK 版双精度框）；
 *  - 设置窗口「排除目录」`QListWidget` → `DListView`（QListView 系，addItem/addItems/
 *    removeItem 语义与 QListWidget 对齐，取当前行改 `currentIndex().row()`）；
 *  - 三处按钮成员声明 `QPushButton*` → `DPushButton*`（`Ui::button()` 本就返回
 *    `DPushButton*`，原声明靠继承关系才编过，属声明与实现不一致）；
 *  - 自定义范围对话框 `QCalendarWidget`×2（纵排占高、两日历互相比大小导致
 *    「高亮 vs 摘要」状态分叉）→ **单日历两次点击定区间**：
 *    第 1 次点击落起点，第 2 次点击落终点（早于起点则改为新起点）。
 *    摘要文字以 from/to 状态为唯一数据源，从结构上消除不一致；
 *  - `QTableWidget`×2 按设计方案 §5.2 保留（DTK 未提供等价表格控件）。
 *
 * 保留的数据逻辑（未改）：时间线按范围懒加载、QFileSystemWatcher 自动刷新、
 * pkexec 提权扫描与实时进度、排行下钻与打开位置。
 */
class DiskUsagePage : public BasePage
{
    Q_OBJECT
public:
    /**
     * 磁盘分析视角（三视角共用一张表）。
     *
     * 三个视角是**同一份顶层占用数据的三种问法**，不是一个页面的三个子页面：
     *   构成 = 现在系统盘被谁占着（Top5 + 其他，看结构）
     *   增长 = 某一天是谁在涨（环比增量，看变化）
     *   排行 = 完整名单从大到小（看全量）
     * 故用枚举 + 单一渲染入口，而不是三个控件各自 setState。
     */
    enum class ViewMode
    {
        Composition,
        Growth,
        Ranking,
    };

    explicit DiskUsagePage(MainWindow *win);
    ~DiskUsagePage() override;

    void refresh() override;
    void shutdown() override;

    /** 通知 action 定位：跳转并选中指定日期 */
    void focusDate(const QDate &date);

private slots:
    void onPreset(int days);
    void onOpenCustomRange();
    void onChartDateSelected(const QDate &d);
    void onRangeProgress(quint64 id, int loaded, int total);
    void onRangeLoaded(quint64 id, const QList<Panshi::DiskDayRecord> &records);
    void onScanNow();
    void onScanProgress(int done, const QString &cur);
    void onScanFinished(bool ok, const QString &err);
    void onDrillFinished(const QString &dir, const QList<Panshi::DiskItem> &items);
    /** 切换分析视角（构成 / 增长 / 排行） */
    void onViewModeChanged(int index);
    void onAnalysisCellClicked(int row, int column);
    void onAnalysisCellDoubleClicked(int row, int column);
    /** 面包屑回退到指定层级（0 = 系统盘根） */
    void onBreadcrumbJump(int level);
    void onBackToSystemTimeline();
    /** 其它分区行 → 下钻该分区内部构成（切到占用排行标签） */
    void onDrillPartition(const QString &mountPoint);
    void onMonitorToggled(bool on);
    void onAlertModeChanged(int index);
    void onAddExclude();
    void onRemoveExclude();
    void onClearHistory();
    void onHistoryDirChanged(const QString &path);
    void onThemeChanged();
    /**
     * 调色板替换**就位后**的重绘（挂 `applicationPaletteChanged`）。
     *
     * 与 onThemeChanged 的分工：后者挂 `themeTypeChanged`，此刻 DTK 调色板尚未
     * 替换完成，重绘会得到「半新半旧」；本方法在调色板就位后才重绘。
     * 见 .cpp 内注释。
     */
    void onPaletteChanged();
    void onOpenSettings();

protected:
    void showEvent(QShowEvent *e) override;
    /** 监听内容视口 Resize → 英雄区折行（必须调基类：它自己也要管卡片行） */
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    void buildUi();
    void connectSettingsWidgets();
    void buildOverviewCard(class QVBoxLayout *lay);
    void buildTimelineCard(class QVBoxLayout *lay);
    void buildSummaryCard(class QVBoxLayout *lay);
    /**
     * 英雄区响应式重排：宽窗「净增长 │ 容量预测」左右并置，窄窗折成上下。
     *
     * 为什么必须自己做：容量预测块有最小宽度（否则「按当前增速约 N 天后写满」
     * 折行成 4 行，右栏比左栏高一倍）。而横向排布在窄窗下会把这个最小宽度
     * 直接顶到净增长的大数值上，把 T2 挤换行 —— 本项目「显式最小宽度是
     * 挤压唯一防线」的反面：这里是要**主动折行**而不是让它挤。
     */
    void updateHeroLayout();
    /** 按 m_fcSemantic 重设容量预测图标（QLabel 图标是快照，切主题必重查） */
    void updateForecastIcon();
    /**
     * 整合后的「磁盘分析」区：面包屑上下文条 + 视角分段 + 单表 + 视角说明 +
     * 「其它分区」小节（全盘视角）。
     *
     * 替代原先分散的三处：区间汇总卡内的磁盘构成行列表、增长详情标签、
     * 占用排行标签 —— 三者取同一份顶层数据，合并后只剩一个数据源、一个渲染函数。
     */
    void buildAnalysisSection(class QVBoxLayout *lay);
    /** 按当前视角重建表格内容（三个视角共用的唯一渲染入口） */
    void updateAnalysis();
    /** 重建面包屑（依据当前浏览路径 m_rankCurrentPath） */
    void rebuildBreadcrumb();
    /** 刷新上下文条：两个返回按钮的可用状态 + 视角说明 + 面包屑 */
    void updateContextBar();
    /** 回到指定层级：弹出帧栈直到只剩 level 层（0 = 系统盘根） */
    void popBrowseTo(int level);
    /** 视角说明文案（同时用作分段按钮的 tooltip） */
    static QString viewHintFor(ViewMode v);
    /** 把视角分段控件的选中态同步到 m_view（外部改了 m_view 后调用） */
    void syncViewBox();
    /** 重建「其它分区」行列表（df 数据，全盘视角） */
    void rebuildOtherPartitions();
    void buildSettingsDialog();
    void requestRange(const QDate &from, const QDate &to);
    void updateOverview();
    void updateSummary();
    void updateTimelineSeries();
    void updateServiceStatus();
    void applyServiceStatus(const QString &state);
    void onStartMonitorService();
    void beginCheck();                       // 启动检查：显示检查面板
    void checkStep(const QString &name);     // 记录一个检查步骤（含耗时）
    void updateDayDetail(const QDate &d);
    void loadRanking();
    void updateMonitorSettingsUi();
    void applyConfigToStore();
    void openInFileManager(const QString &path);
    /**
     * 启停 systemd 用户 timer。
     * @param errorOut 失败时填入 systemctl 的真实输出（stdout + stderr）。
     *
     * 「服务打不开」的三类成因（单元没被 systemd 扫描到 / 会话总线不可用 /
     * 单元语法错误）修法完全不同，所以失败原因必须带出来，不能只回一个 bool。
     */
    bool setMonitorService(bool on, QString *errorOut = nullptr);
    void rebuildSpikeSet();
    void showStatus(const QString &text, bool ok = true);

    // ------------------------------------------------------------ 告警
    /** 一条历史告警（展示用，来自 alert.json 的 spikes） */
    struct AlertEntry
    {
        QString date;
        QString dir;
        QString growth;
    };

    /**
     * 读取 alert.json 并重算横幅与历史入口。
     *
     * 展示哪一条 = **最后一条未被用户关闭的**告警；已被关闭的不再自动弹出，
     * 但仍完整保留在历史里（关闭是"视图状态"，不是"删数据"）。
     */
    void refreshAlert();
    /** 历史告警条目（新→旧），直接来自 alert.json 的 spikes 数组 */
    QList<AlertEntry> alertHistory() const;
    /** 告警唯一键：日期|目录（同一天同一目录只算一条） */
    static QString alertKey(const QString &date, const QString &dir);
    /** 按当前 dismissed / seen 状态刷新「告警历史」按钮文案与可用状态 */
    void updateAlertHistoryBtn();
    /** 用户点「×」关闭当前横幅 → 记入 dismissed 并持久化 */
    void onDismissAlert();
    /** 打开告警历史弹窗（含清空历史） */
    void onOpenAlertHistory();
    /** 读取 / 写入本地 UI 状态（dismissed 列表 + 已看条数） */
    void loadAlertUiState();
    void saveAlertUiState() const;

    MainWindow *m_win;
    QScrollArea *m_contentScroll = nullptr;   // 内容滚动区（供基类取视口宽度）
    Panshi::DiskHistoryStore m_store;
    Panshi::DiskScanner m_scanner;
    Panshi::MonitorConfig m_config;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    bool m_initialized = false;

    // ① 概览
    DProgressBar *m_usageBar = nullptr;
    /**
     * 容量条的百分比标签（**外置于条右侧**，不是条内文字）。
     *
     * ⚠️ 缺陷 O2-1 二次修正（2026-10-02 复测 V-16 像素级实测）：
     *   我原先判断「14px 装不下文字行高」是**错的** —— 提到 18px 后文字
     *   仍不显示。实测证明 **DTK6 的 DProgressBar 样式不绘制内嵌 format 文字**
     *   （`CE_ProgressBarLabel` 在 ChameleonStyle 里未实现）。
     *   → 方案改为「百分比外置」：条只负责形状，百分比由独立 DLabel 承载。
     *   这也是更稳的做法：不依赖控件的内部绘制行为，未来 DTK 变化也不受影响。
     */
    DLabel *m_usagePct = nullptr;
    DLabel *m_usedBig = nullptr;
    DLabel *m_overviewInfo = nullptr;
    DLabel *m_monitorStatus = nullptr;
    PanshiUi::PanshiInlineBanner *m_alertBanner = nullptr;   // 告警（原 panshiWarnBox）
    DPushButton *m_alertHistoryBtn = nullptr;   // 告警历史入口（按钮文案带条数）
    QStringList m_dismissedAlerts;   // 用户已关闭的告警键（持久化，仅影响是否自动弹出）
    int m_alertSeenCount = 0;        // 已查看的告警条数（用于历史入口的"新增"提示）
    // ② 时间线
    UsageChart *m_chart = nullptr;
    DLabel *m_loadStatus = nullptr;
    DLabel *m_timelineHint = nullptr;   // 时间线当前显示对象提示（纯文字，无按钮）
    class DButtonBox *m_rangeBox = nullptr;   // 范围分段选择（15/30/90/180/365/自定义）
    QString m_timelineDir;              // 当前时间线目录（空 = 系统盘占用）

    // 启动检查流程
    bool m_checkedOnce = false;
    bool m_checking = false;
    int m_checkSteps = 0;
    int m_checkTotal = 4;
    qint64 m_lastCheckMs = 0;
    class QElapsedTimer m_checkTimer;
    PanshiUi::PanshiBadge *m_serviceBadge = nullptr;  // 监控服务运行状态徽章
    DPushButton *m_serviceStartBtn = nullptr; // “立即启动”修复按钮
    class QProcess *m_svcProc = nullptr; // 服务状态检测进程
    // ② 区间汇总（2026-10-10 重设计：主角 + 次要指标网格，取代四张等权卡片）
    DLabel *m_sumRangeLabel = nullptr;  // 区间陈述（起止日期 · 天数 · 记录数）
    DLabel *m_sumNet = nullptr;         // 主角：净增长（T2 大数值）
    DLabel *m_sumBeginEnd = nullptr;    // 主角副说明：区间起始 → 结束
    DLabel *m_sumDaily = nullptr;       // 日均增长
    DLabel *m_sumMax = nullptr;         // 最大单日
    DLabel *m_sumMaxDay = nullptr;      // 最大单日备注（发生日期，空则隐藏）
    DLabel *m_sumTrend = nullptr;       // 增长起点
    /**
     * 英雄区两栏容器（2026-10-10 第三轮）。
     *
     * 宽窗 = 横向「净增长 │ 容量预测」；窄窗自动折成上下 —— 见 updateHeroLayout()。
     * 容量预测从底部整宽 Banner 搬上来后不再是「页面级通知」，而是主角的
     * 对偶：左边回答"已经涨了多少"，右边回答"按这个速度还能撑多久"。
     */
    class QBoxLayout *m_heroRow = nullptr;
    QWidget *m_heroMain = nullptr;      // 左栏：净增长 + 起始→结束
    QWidget *m_heroSep = nullptr;       // 两栏之间的竖分隔线（仅宽窗可见）
    QWidget *m_forecastBox = nullptr;   // 右栏：容量预测块
    DLabel *m_fcIcon = nullptr;         // 容量预测语义图标（颜色之外的第二通道）
    DLabel *m_fcLabel = nullptr;        // 容量预测标签（图标旁）
    DLabel *m_fcValue = nullptr;        // 容量预测主体（还剩多少天 / 趋稳）
    DLabel *m_fcNote = nullptr;         // 容量预测备注（预计写满日期）
    /** 容量预测当前语义档（图标是快照，切主题时据此重查） */
    PanshiUi::Semantic m_fcSemantic = PanshiUi::Semantic::Muted;
    // ④ 磁盘分析（整合区）
    ViewMode m_view = ViewMode::Composition;
    QTableWidget *m_analysisTable = nullptr;
    class DButtonBox *m_viewBox = nullptr;      // 视角分段（构成 / 增长 / 排行）
    DPushButton *m_backUpBtn = nullptr;         // 返回上级（下钻栈）
    DPushButton *m_backRootBtn = nullptr;       // 返回系统盘（回到根）
    QWidget *m_crumbBox = nullptr;              // 面包屑宿主
    class QHBoxLayout *m_crumbLay = nullptr;
    DLabel *m_viewHint = nullptr;               // 视角说明（随视角切换）
    class QVBoxLayout *m_otherLay = nullptr;    // 其它分区行列表容器
    DDialog *m_settingsDlg = nullptr;
    // ⑥ 设置（独立设置窗口）
    DSwitchButton *m_monitorSwitch = nullptr;
    DComboBox *m_alertModeBox = nullptr;
    DSpinBox *m_fixedThresholdSpin = nullptr;
    DDoubleSpinBox *m_multiplierSpin = nullptr;
    DSpinBox *m_cooldownSpin = nullptr;
    DLineEdit *m_excludeEdit = nullptr;
    DListView *m_excludeList = nullptr;
    /** DListView 走标准 model/view（不同于 QListWidget 自带模型），模型随页面存活 */
    class QStringListModel *m_excludeModel = nullptr;
    DSpinBox *m_retentionSpin = nullptr;
    DComboBox *m_budgetBox = nullptr;
    DLabel *m_storageUsed = nullptr;

    quint64 m_rangeId = 0;
    QList<Panshi::DiskDayRecord> m_records;
    QDate m_from, m_to;
    QDate m_selected;
    QSet<QDate> m_spikeSet;
    QList<Panshi::DiskItem> m_rankItems;
    QString m_rankCurrentPath;            // 当前列表路径（"" = 一级目录）
    QList<QPair<QString, QList<Panshi::DiskItem>>> m_rankFrames; // 下钻帧栈
    /** 当前表格正在展示的条目（供「单击行 → 时间线跟随」定位，三视角共用） */
    QList<Panshi::DiskItem> m_viewItems;
    /** 当前横幅所展示的告警键（空 = 横幅未展示任何告警） */
    QString m_currentAlertKey;
    /** 表格「占比」列：数值 + 细条（把原磁盘构成的占比条并入表格） */
    void setShareCell(class QTableWidget *t, int row, int col, double pct, bool valid);
    /** 读取 alert.json 的 spikes 数组（无文件/非对象时返回空数组） */
    static QJsonArray loadAlertSpikes();
};
