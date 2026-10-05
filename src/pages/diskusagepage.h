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

#include <QDate>
#include <QList>
#include <QDateTime>
#include <QPair>

class MainWindow;
class UsageChart;
class QTableWidget;
class QFileSystemWatcher;
class QTimer;
class QScrollArea;
#include <DTabBar>
#include <DDialog>
class QStackedWidget;

/**
 * 磁盘占用页（阶段 3-7 重写）：概览 / 时间线 / 三标签分析 / 独立设置窗口。
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
    void onRankCellClicked(int row, int column);
    void onRankCellDoubleClicked(int row, int column);
    void onRankBack();
    void onDetailCellClicked(int row, int column);
    void onBackToSystemTimeline();
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

private:
    void buildUi();
    void connectSettingsWidgets();
    void buildOverviewCard(class QVBoxLayout *lay);
    void buildTimelineCard(class QVBoxLayout *lay);
    void buildAnalysisTabs(class QVBoxLayout *lay);
    void buildSummaryTab(class QVBoxLayout *lay);
    void buildDetailTab(class QVBoxLayout *lay);
    void buildRankTab(class QVBoxLayout *lay);
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
    // ② 时间线
    UsageChart *m_chart = nullptr;
    DLabel *m_loadStatus = nullptr;
    DLabel *m_timelineHint = nullptr;   // 时间线当前显示范围提示
    DPushButton *m_backTimelineBtn = nullptr; // “返回系统盘”按钮（目录模式显示）
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
    // ③ 区间汇总
    PanshiUi::PanshiStateCard *m_sumNet = nullptr;
    PanshiUi::PanshiStateCard *m_sumDaily = nullptr;
    PanshiUi::PanshiStateCard *m_sumMax = nullptr;
    PanshiUi::PanshiStateCard *m_sumTrend = nullptr;
    PanshiUi::PanshiResponsiveCardRow *m_sumRow = nullptr;   // 汇总卡容器（窄窗折行）
    PanshiUi::PanshiInlineBanner *m_sumEtaBar = nullptr;  // 容量预测（原 panshiTipBar）
    DLabel *m_sumTop = nullptr;
    // ④ 增长详情
    QTableWidget *m_detailTable = nullptr;
    // ⑤ 占用排行
    QTableWidget *m_rankTable = nullptr;
    DLabel *m_rankPath = nullptr;
    DPushButton *m_rankBackBtn = nullptr;
    // ③ 分析标签页
    DTabBar *m_tabBar = nullptr;
    QStackedWidget *m_tabs = nullptr;
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
    QList<Panshi::DiskItem> m_detailItems; // 增长详情当前行对应的磁盘项（供点击切换时间线）
};
