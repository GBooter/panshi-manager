// SPDX-License-Identifier: GPL-3.0-or-later
#include "diskusagepage.h"
#include "common.h"
#include "../mainwindow.h"
#include "../backend.h"
#include "../widgets/usagechart.h"

#include <DSwitchButton>
#include <DComboBox>
#include <DSpinBox>
#include <DDoubleSpinBox>
#include <DListView>
#include <DProgressBar>
#include <QIcon>
#include <QSignalBlocker>
#include <algorithm>
#include <memory>
#include <DLineEdit>
#include <DDialog>
#include <DPalette>
#include <DGuiApplicationHelper>
#include <DTabBar>
#include <DIconTheme>
#include <DIconButton>
#include <DButtonBox>
#include <QStackedWidget>
#include <QCalendarWidget>
#include <QDir>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QStringListModel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QProcess>
#include <QStyle>
#include <QFrame>
#include <QSizePolicy>
#include <QStringListModel>
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace Panshi;

namespace {
constexpr qint64 kMB = 1024LL * 1024;

/**
 * 紧凑容量文本：GB/MB 取整、无小数（用于分区行「已用 / 容量」短标签）。
 * 分区容量可能到数百 GB，formatSize 的三位小数会把行标签撑爆。
 */
QString formatSizeCompact(qint64 bytes)
{
    constexpr qint64 kG = 1024LL * 1024 * 1024;
    constexpr qint64 kM = 1024LL * 1024;
    if (bytes >= kG)
        return QString::number(bytes / kG) + QStringLiteral(" GB");
    if (bytes >= kM)
        return QString::number(bytes / kM) + QStringLiteral(" MB");
    if (bytes >= 1024)
        return QString::number(bytes / 1024) + QStringLiteral(" KB");
    return QString::number(bytes) + QStringLiteral(" B");
}

/** 在已加载记录里查某一天的序号；找不到返回 -1 */
int dateIndexInRecords(const QList<DiskDayRecord> &recs, const QDate &d)
{
    if (!d.isValid())
        return -1;
    for (int i = 0; i < recs.size(); ++i)
        if (recs.at(i).date == d)
            return i;
    return -1;
}

/**
 * 作用域退出时补一次表格无障碍收尾。
 *
 * 为什么不直接在每个 return 前手写 finalizeTableAccessible()：
 * updateDayDetail() 有 4 条 return 路径（目录模式缺记录 / 目录模式正常 /
 * 系统盘缺记录 / 当日无显著增长），手写必然漏掉一条，而漏掉的代价是
 * 「那一态下单元格读屏读不出列名」—— 静默失效，没有编译告警。
 * 与其靠人肉记住每条出口，不如让出口在语法上无法被忘记。
 */
struct A11yOnScopeExit {
    QTableWidget *t;
    ~A11yOnScopeExit() { PanshiUi::finalizeTableAccessible(t); }
};

/**
 * 从某日记录中取出指定目录的大小（一级目录查 topLevel，深层目录查 drillCache）。
 * 无记录返回 -1。
 */
qint64 dirSizeInRecord(const Panshi::DiskDayRecord &r, const QString &dir)
{
    for (const Panshi::DiskItem &it : r.topLevel) {
        if (it.path == dir)
            return it.size;
    }
    const int slash = dir.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        const QString parent = dir.left(slash);
        const auto items = r.drillCache.value(parent);
        for (const Panshi::DiskItem &it : items) {
            if (it.path == dir)
                return it.size;
        }
    }
    return -1;
}

/** 监控 systemd 用户单元是否已部署（安装应用时写入各 systemd 用户单元目录） */
bool monitorUnitInstalled()
{
    // 与 systemd.unit(5) 的用户单元查找目录保持一致；
    // /usr/local/ 不可漏 —— cmake --install 默认 prefix 即 /usr/local
    // （2026-10-03 B6 实测：单元实际在 /usr/local/lib/systemd/user/，
    //  缺这一路径会把已安装的系统级实例误判为「监控组件未安装」）。
    const QStringList paths = {
        QStringLiteral("/usr/local/lib/systemd/user/panshi-monitor.timer"),
        QStringLiteral("/usr/lib/systemd/user/panshi-monitor.timer"),
        QStringLiteral("/etc/systemd/user/panshi-monitor.timer"),
        QDir::homePath() + QStringLiteral("/.config/systemd/user/panshi-monitor.timer"),
        // $XDG_DATA_HOME 默认落点（用户级 install / 手工放置也会到这儿）
        QDir::homePath() + QStringLiteral("/.local/share/systemd/user/panshi-monitor.timer"),
    };
    for (const QString &p : paths) {
        if (QFile::exists(p))
            return true;
    }
    return false;
}

/**
 * 执行一条 `systemctl --user` 命令（阻塞，带超时）。
 *
 * 为什么连 stderr 一起读回来：
 *   「服务打不开」的真正原因（单元没被扫描到 / 会话总线不可用 / 单元语法错误）
 *   **只写在 stderr**。原实现只读 stdout 并丢弃一切，失败后没有任何线索，
 *   只能给「请重启系统后重试」这种猜出来的文案 —— 而重启未必能解决。
 */
struct SystemctlResult
{
    bool ok = false;
    QString output;   // stdout + stderr（已 trim）
};

SystemctlResult runSystemctlUser(const QStringList &args, int timeoutMs = 15000)
{
    SystemctlResult r;
    QProcess p;
    p.start(QStringLiteral("systemctl"), QStringList{QStringLiteral("--user")} + args);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        r.output = QObject::tr("systemctl --user %1 执行超时").arg(args.join(QStringLiteral(" ")));
        return r;
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (out.isEmpty())
        r.output = err;
    else if (err.isEmpty())
        r.output = out;
    else
        r.output = out + QStringLiteral("\n") + err;
    r.ok = (p.exitStatus() == QProcess::NormalExit) && (p.exitCode() == 0);
    return r;
}

/**
 * 把 systemctl 的原始输出压成一行短摘要。
 *
 * 徽章和状态栏都是单行窄控件，而 systemd 的错误可能带多行上下文
 * （单元路径、建议命令等）→ 直接塞进去会把整行撑破、把右侧按钮挤出可视区。
 * 取首行 + 截断，既能定位问题，又不破坏布局。
 */
QString briefError(const QString &err)
{
    QString s = err.trimmed();
    const int nl = s.indexOf(QLatin1Char('\n'));
    if (nl >= 0)
        s = s.left(nl).trimmed();
    if (s.size() > 120)
        s = s.left(120) + QStringLiteral("…");
    return s;
}

/**
 * 让 systemd 用户实例认识 panshi-monitor.timer（缺失时先重载再验证）。
 *
 * ⚠️ 这是「磁盘监控服务打不开」的根因（2026-10-04 实测）：
 *   单元文件落盘（deb 装到 /usr/lib/systemd/user/）**不等于** systemd 认识它 ——
 *   用户实例只在自身启动或 daemon-reload 时扫描单元目录。而包里的 postinst
 *   写的是 `systemctl --global daemon-reload`，systemd 255 实测直接报
 *     "--global is not supported for this operation."
 *   （退出码 1，又被 `|| true` 吞掉）→ 装完后**从来没有重载过**。
 *   就算那条命令有效也没用：dpkg 以 root 运行，连不上用户的会话总线。
 *   → 唯一可靠的重载时机是**应用进程内**（它跑在用户会话里，有总线地址）。
 *
 * 返回 false = systemd 至今不认识这个单元，此时 enable/start 必然失败。
 */
bool ensureMonitorUnitLoaded()
{
    // daemon-reload 幂等，可安全重复执行
    runSystemctlUser({QStringLiteral("daemon-reload")}, 20000);
    // 用 cat 验证：能打出内容 ⇒ 单元已被扫描进管理器（不存在时退出码非 0）
    const SystemctlResult r =
        runSystemctlUser({QStringLiteral("cat"), QStringLiteral("panshi-monitor.timer")});
    return r.ok;
}

/**
 * 紧凑的"打开位置"图标按钮（避免整列按钮过长）。
 *
 * 无障碍：纯图标按钮的可访问名不能只写"打开位置"——读屏用户在一张表里逐格移动时，
 * 听不出这是哪一行的按钮，必须带上目标路径（tooltip 同理，也带路径）。
 * 参数 path 可为空（不传时退化为通用名）。
 */
DIconButton *makeOpenButton(const QString &path = QString())
{
    auto *btn = new DIconButton();
    btn->setIcon(DIconTheme::findQIcon(QStringLiteral("folder")));
    btn->setFixedSize(30, 30);
    const QString label = path.isEmpty()
                              ? QObject::tr("打开位置")
                              : QObject::tr("打开位置：%1").arg(path);
    btn->setToolTip(label);
    btn->setAccessibleName(label);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

/**
 * 名称/路径列单元格：用 ElidedLabel 中部省略，完整路径走 tooltip。
 * 路径是自由长度文本，绝不能让它决定列宽（否则长路径会把表撑爆）；
 * 省略后由 tooltip 兜底可读性（与 SnapshotsPage 名称列同策略）。
 *
 * 无障碍：这一列是 cellWidget（不是 QTableWidgetItem），finalizeTableAccessible 遍历不到，
 * 所以可访问名必须在这里单独挂 —— 省略显示不等于省略语义。
 */
void setPathCell(QTableWidget *t, int row, int col, const QString &path)
{
    auto *holder = new QWidget(t);
    auto *hl = new QHBoxLayout(holder);
    hl->setContentsMargins(6, 0, 6, 0);
    hl->setSpacing(0);
    auto *lab = new Ui::ElidedLabel(path, holder);
    lab->setToolTip(path);
    lab->setAccessibleName(path);
    hl->addWidget(lab, 1);
    t->setCellWidget(row, col, holder);
}

/**
 * 建表（阶段 3-7 统一列宽策略）。
 *
 * 与 SnapshotsPage / DeploysPage 同方案：
 *  - 列宽全部 Interactive（原 ResizeToContents 会让长路径把表撑爆）；
 *  - 第 0 列（名称/路径，自由长度文本）用 ElidedLabel 承载 → 可被压缩；
 *  - 其余列为固定宽度（内容长度可控）；
 *  - 窄窗兜底：横向 SizePolicy=Ignored + 自身横向滚动条 AsNeeded，
 *    避免被外层 makeScrollArea（ScrollBarAlwaysOff）静默裁剪。
 *
 * 预算：两表都是「名称弹性 + 大小 110 + [占比 90] + 操作 72」。
 */
QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
    auto *t = new QTableWidget(parent);
    t->setObjectName(QStringLiteral("panshiTable"));
    // ⚠️ 必须显式 setColumnCount：Qt6 的 setHorizontalHeaderLabels() **不会**顺带
    // 把列数撑起来（实测传 3 个 label，columnCount() 仍是 0），而后续 setItem()
    // 在 0 列表上是**静默失败** —— 不报错、不崩，item() 直接返回 nullptr。
    // 单列表（备份页/部署页）用的是 QTableWidget(0, 6)，同样自带列数。
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(40);
    t->horizontalHeader()->setFixedHeight(42);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);

    auto *hh = t->horizontalHeader();
    hh->setSectionsMovable(false);
    for (int i = 0; i < headers.size(); ++i)
        hh->setSectionResizeMode(i, QHeaderView::Interactive);
    hh->setSectionResizeMode(0, QHeaderView::Stretch);   // 名称/路径列吸收剩余宽度
    hh->setMinimumSectionSize(72);
    hh->setStretchLastSection(false);

    // 窄窗兜底（同 3-6）：让布局忽略表格 sizeHint，横向溢出由表格内部滚动条承担
    {
        QSizePolicy sp = t->sizePolicy();
        sp.setHorizontalPolicy(QSizePolicy::Ignored);
        t->setSizePolicy(sp);
    }
    t->setMinimumWidth(320);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    t->setMinimumHeight(180);
    return t;
}

/**
 * 固定列宽（建表后调用一次）。
 * headers.size()==3：名称弹性 + 大小 120 + 操作 72
 * headers.size()==4：名称弹性 + 大小 120 + 占比 90 + 操作 72
 * 右侧「操作」列只放一个 30px 图标按钮，72 足够；由此最小总宽
 * = 名称下限 72 + 120 (+90) + 72，远小于默认视口，默认态不出滚动条。
 */
void applyTableWidths(QTableWidget *t, int columns)
{
    t->setColumnWidth(1, 120);
    if (columns == 4)
        t->setColumnWidth(2, 90);
    t->setColumnWidth(columns - 1, 72);
}
} // namespace

DiskUsagePage::DiskUsagePage(MainWindow *win)
    : BasePage(win)
    , m_win(win)
{
    // 只在这里 buildUi()：设置控件的信号连接由 buildSettingsDialog() 末尾
    // 自己调用 connectSettingsWidgets()。
    //
    // ⚠️ 此前构造函数里还多调了一次 connectSettingsWidgets()，而那时
    //    m_monitorSwitch / m_alertModeBox 等**全是 nullptr**（它们要等
    //    buildUi() → buildSettingsDialog() 才被创建）→ 那次调用里每条
    //    connect 都只得到一句 "QObject::connect: Cannot connect (nullptr)"
    //    警告然后被丢弃。功能上没坏（真正生效的是 buildSettingsDialog 里那次），
    //    但代价很坏：启动时刷一串噪音警告，且**真正漏连时的警告会淹没在里面**
    //    ——下次谁少建一个控件，看日志根本分不出是哪一条。已移除该次无效调用。
    //
    // 告警的"已关闭"状态要先于 UI 建好读入 —— 否则本次启动会先把用户
    // 上次关掉的告警再弹一次（看起来像"关不掉"）。
    loadAlertUiState();
    buildUi();

    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(DiskHistoryStore::historyDir());
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DiskUsagePage::onHistoryDirChanged);
    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(400);
    connect(m_refreshDebounce, &QTimer::timeout, this, [this] {
        applyConfigToStore();
        updateOverview();
        if (m_from.isValid())
            requestRange(m_from, m_to);   // 数据文件变化后静默刷新当前范围
    });

    // 手动扫描：顶层结果保存为今日完整记录
    connect(&m_scanner, &DiskScanner::topLevelFinished, this,
            [this](const QList<DiskItem> &items, bool complete) {
                const QDate today = QDate::currentDate();
                DiskDayRecord rec;
                if (!m_store.loadDay(today, &rec)) {
                    rec = DiskDayRecord();
                    rec.date = today;
                }
                rec.topLevel = items;
                rec.complete = complete;
                m_store.saveDayRecord(rec);
            });
    connect(&m_scanner, &DiskScanner::progress, this, &DiskUsagePage::onScanProgress);
    connect(&m_scanner, &DiskScanner::drillFinished, this, &DiskUsagePage::onDrillFinished);
    connect(&m_scanner, &DiskScanner::scanFinished, this, &DiskUsagePage::onScanFinished);

    // 懒加载信号
    connect(&m_store, &DiskHistoryStore::rangeProgress,
            this, &DiskUsagePage::onRangeProgress);
    connect(&m_store, &DiskHistoryStore::rangeLoaded,
            this, &DiskUsagePage::onRangeLoaded);

    // 主题切换：两个信号分工（见 onPaletteChanged 的注释）
    //   themeTypeChanged          → 立刻知道要准备（可能过早，调色板未就位）
    //   applicationPaletteChanged → 调色板就位，此刻重绘才安全（真正起作用的那个）
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &DiskUsagePage::onThemeChanged);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::applicationPaletteChanged,
            this, &DiskUsagePage::onPaletteChanged);
    m_initialized = true;
}

DiskUsagePage::~DiskUsagePage()
{
    m_scanner.cancel();
}

void DiskUsagePage::showEvent(QShowEvent *e)
{
    BasePage::showEvent(e);
    // 隐藏页在 QStackedWidget 里首次 show 时才拿到真实视口宽度（此时布局已就绪），
    // 与基类对 registerResponsiveCardRow 的处理同理。
    updateHeroLayout();
}

bool DiskUsagePage::eventFilter(QObject *watched, QEvent *e)
{
    // 必须在基类处理之前/之后都无害 —— 两者监听的宽度来源不同：
    // 基类管卡片行折行（表320/Row200），本页管英雄区折行（520 阈值）。
    if (m_contentScroll && watched == m_contentScroll->viewport()
        && e->type() == QEvent::Resize) {
        updateHeroLayout();
    }
    return BasePage::eventFilter(watched, e);
}

void DiskUsagePage::buildUi()
{
    auto *scroll = Ui::makeScrollArea(this);
    m_contentScroll = scroll;
    auto *content = new QWidget;
    scroll->setWidget(content);
    // 英雄区折行判定取的是 viewport 宽度（已扣侧边栏与滚动条），
    // 必须跟基类一样监听 viewport 的 Resize —— 只挂基类那一个 eventFilter
    // 不够，页面自身的 resizeEvent 在 ScrollArea 里不随视口变化而触发。
    if (scroll->viewport())
        scroll->viewport()->installEventFilter(this);
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(scroll);

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(14);

    // 页头（设置入口位于下方概览卡“监控设置”）
    lay->addWidget(Ui::pageHeader(tr("磁盘空间"),
                                  tr("持续监控系统盘空间，定位容量增长来源"),
                                  QStringLiteral("drive-harddisk")));

    buildOverviewCard(lay);
    // ③ 区间汇总在 ④ 时间线**之前**（2026-10-10 用户要求）：阅读顺序改为
    //   「先给结论 → 再看明细」。读者第一眼拿到「这段时间涨了多少」，
    //   需要溯源时才往下看折线；反过来（先图后数）会让人先费力读图、
    //   再到下面找结论，等于把答案埋在过程后面。
    // 连带影响：范围选择器（15/30/90/…/自定义）是「区间」的定义者，
    //   必须随区间汇总一起上移，否则变成「控件在它所控制的结论下方」。
    buildSummaryCard(lay);
    buildTimelineCard(lay);
    buildAnalysisSection(lay);   // ④ 磁盘分析：构成 / 增长 / 排行 三视角整合区
    buildSettingsDialog();
    lay->addStretch();
}

// ---------- ① 概览：hero 状态条 ----------
void DiskUsagePage::buildOverviewCard(QVBoxLayout *lay)
{
    auto *c1 = new PanshiUi::PanshiPanel(nullptr, PanshiUi::PanshiPanel::Level::Surface);
    auto *cl1 = c1->body();
    cl1->setContentsMargins(20, 18, 20, 18);
    cl1->setSpacing(10);

    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(18);
    heroRow->addWidget(Ui::iconBadge(QStringLiteral("drive-harddisk"), 56));
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(2);
    m_usedBig = new DLabel(c1);
    m_usedBig->setForegroundRole(DPalette::TextTitle);
    m_usedBig->setWordWrap(true);
    DFontSizeManager::instance()->bind(m_usedBig, DFontSizeManager::T2);
    heroText->addWidget(m_usedBig);
    m_overviewInfo = Ui::rowDesc(QString());
    heroText->addWidget(m_overviewInfo);
    heroRow->addLayout(heroText, 1);
    // 概览容量条：DProgressBar（DTK 原生外观）。
    //
    // ⚠️ 缺陷 O2-1 二次修正（2026-10-02 复测 V-16 像素级实测）：
    //   **DTK6 的 DProgressBar 样式不绘制内嵌 format 文字** ——
    //   `setTextVisible(true)` + `setFormat()` 都正确调用、槽高从 14px 提到 18px，
    //   像素级检查仍找不到任何文字像素。故我此前「槽太矮放不下行高」的判断是错的。
    //   （`setTextVisible` 在继承链上有效，但样式层没有实现 CE_ProgressBarLabel。）
    //
    //   方案改为**百分比外置**：条只负责形状与填充，百分比由右侧独立 DLabel 承载。
    //   好处不止于绕开缺陷 —— 不依赖控件内部绘制实现，DTK 升级也不受影响。
    //
    // 颜色职责：不再走 QSS 的 chunk 切色（原 #panshiUsageBar 规则已随之删除），
    // 语义完全落在左侧「已用」大数值的文字色上（见 updateOverview）。
    m_usageBar = new DProgressBar(c1);
    m_usageBar->setRange(0, 1000);
    m_usageBar->setFixedWidth(240);
    // 纯形状条：不再需要为文字预留高度，回到紧凑高度（token: ThinProgress 的语义兄弟档）
    m_usageBar->setFixedHeight(PanshiUi::Size::UsageBarHeight);
    heroRow->addWidget(m_usageBar, 0, Qt::AlignVCenter);

    // 百分比外置标签：与条同排、垂直居中。宽度固定以免数字变化时布局抖动
    // （1 位与 2 位小数宽度不同，固定宽度可避免条被推着左右移动）。
    m_usagePct = new DLabel(c1);
    m_usagePct->setFixedWidth(PanshiUi::Size::UsagePctWidth);
    m_usagePct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_usagePct->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(m_usagePct, DFontSizeManager::T4);
    heroRow->addWidget(m_usagePct, 0, Qt::AlignVCenter);
    heroRow->addStretch();
    cl1->addLayout(heroRow);

    // 状态行：服务徽章 + 监控参数/最近记录（单行，主次分层）
    // 徽章语义由 PanshiBadge 承担（原按 objectName 切 panshiTagXxx 的 QSS）
    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(10);
    m_serviceBadge = new PanshiUi::PanshiBadge(tr("正在检测服务状态…"),
                                               PanshiUi::Semantic::Muted, c1);
    statusRow->addWidget(m_serviceBadge);
    m_serviceStartBtn = Ui::button(tr("立即启动"));
    m_serviceStartBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("system-run")));
    m_serviceStartBtn->hide();
    connect(m_serviceStartBtn, &DPushButton::clicked, this, &DiskUsagePage::onStartMonitorService);
    statusRow->addWidget(m_serviceStartBtn);
    m_monitorStatus = Ui::rowDesc(QString());
    statusRow->addWidget(m_monitorStatus, 1);
    cl1->addLayout(statusRow);

    // 操作行：立即扫描 + 监控设置（右对齐，把散在两处的操作集中到概览卡）
    // 2026-10-10 新增「告警历史」——告警可手动关闭，关闭后不消失，只是收进这里。
    auto *opRow = new QHBoxLayout;
    opRow->setSpacing(10);
    opRow->addStretch();
    auto *scanBtn = Ui::button(tr("立即扫描"));
    scanBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("view-refresh")));
    connect(scanBtn, &DPushButton::clicked, this, &DiskUsagePage::onScanNow);
    opRow->addWidget(scanBtn);
    auto *svcBtn = Ui::button(tr("监控设置"));
    svcBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("preferences-system")));
    connect(svcBtn, &DPushButton::clicked, this, &DiskUsagePage::onOpenSettings);
    opRow->addWidget(svcBtn);
    m_alertHistoryBtn = Ui::button(tr("告警历史"));
    m_alertHistoryBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("appointment-soon")));
    m_alertHistoryBtn->setToolTip(tr("查看全部历史告警，包括已手动关闭的"));
    connect(m_alertHistoryBtn, &DPushButton::clicked,
            this, &DiskUsagePage::onOpenAlertHistory);
    opRow->addWidget(m_alertHistoryBtn);
    cl1->addLayout(opRow);

    // 告警：语义 Banner（图标 + 柔和底 + 中性正文），替代原 panshiWarnBox 自绘框。
    // 2026-10-10：改为可关闭；关闭动作落进 dismissed 列表并持久化，
    // 下一次 refresh 不会再把同一条弹回来，但历史里仍可查看。
    m_alertBanner = new PanshiUi::PanshiInlineBanner(c1);
    m_alertBanner->setClosable(true);
    m_alertBanner->hide();
    connect(m_alertBanner, &PanshiUi::PanshiInlineBanner::dismissed,
            this, &DiskUsagePage::onDismissAlert);
    cl1->addWidget(m_alertBanner);
    lay->addWidget(c1);
}

// ---------- ③ 时间线（明细：结论在上方「区间汇总」，此处只负责过程） ----------
void DiskUsagePage::buildTimelineCard(QVBoxLayout *lay)
{
    auto *c2 = new PanshiUi::PanshiSection(tr("时间线"),
                                           tr("折线展示每日磁盘占用；点击数据点查看该日增长详情"));
    auto *tl = c2->content();

    // 工具行：图例（左）+ 范围分段选择（右）。
    // 「立即扫描」已上移到概览卡，标题行保持干净；图例色取自 DPalette，自动跟随主题。
    // 图例文字取短形态（正常/异常/选中），完整语义走 tooltip —— 与范围控件
    // 同行后不再有整行放不下、被外层 ScrollBarAlwaysOff 静默裁剪的风险。
    auto *toolRow = new QHBoxLayout;
    toolRow->setSpacing(10);
    auto *lgNormal = Ui::legendDot(PanshiUi::Semantic::Accent, tr("正常"));
    lgNormal->setToolTip(tr("正常增长"));
    toolRow->addWidget(lgNormal);
    auto *lgAbnormal = Ui::legendDot(PanshiUi::Semantic::Danger, tr("异常"));
    lgAbnormal->setToolTip(tr("异常增长"));
    toolRow->addWidget(lgAbnormal);
    auto *lgSelected = Ui::legendDot(PanshiUi::Semantic::Warning, tr("选中"));
    lgSelected->setToolTip(tr("选中日"));
    toolRow->addWidget(lgSelected);
    auto *lgTrend = Ui::legendLine(tr("趋势"));
    lgTrend->setToolTip(tr("趋势线"));
    toolRow->addWidget(lgTrend);
    toolRow->addStretch();
    // 范围选择器已上移到「区间汇总」卡（2026-10-10）——它定义的是区间，
    // 而区间汇总现在排在时间线前面；放在这里会变成「控件在它所控制的结论下方」。
    tl->addLayout(toolRow);

    m_chart = new UsageChart(c2);
    tl->addWidget(m_chart);
    connect(m_chart, &UsageChart::dateSelected, this, &DiskUsagePage::onChartDateSelected);

    // 图表下方单行：当前显示对象（左）+ 加载状态（右）。
    //
    // ⚠️ 「返回系统盘」已从这里移走（2026-10-10 用户反馈：放在时间线区域里不方便）。
    //    时间线只负责**陈述当前在看什么**，返回入口跟着「选目录」的动作走 ——
    //    即下方「磁盘分析」区顶部的上下文条（面包屑）。两处不再重复摆按钮，
    //    避免同一个动作两个入口、状态各自为政。
    auto *footerRow = new QHBoxLayout;
    footerRow->setSpacing(10);
    m_timelineHint = Ui::rowDesc(QString());
    footerRow->addWidget(m_timelineHint, 1);
    m_loadStatus = Ui::rowDesc(QString());
    footerRow->addWidget(m_loadStatus);
    tl->addLayout(footerRow);
    lay->addWidget(c2);
}

// ---------- ② 区间汇总（结论先行：先给答案，再看下方折线明细） ----------
void DiskUsagePage::buildSummaryCard(QVBoxLayout *lay)
{
    // ⚠️ 本卡原为「四张等权 PanshiStateCard 一字排开」，用户反馈「有点乱」。
    //    拆开看，乱来自四个叠在一起的问题：
    //      1. **没有焦点** —— 净增长/日均/最大单日/起始日同样大小、同样权重，
    //         眼睛不知道先落哪儿；而「这段区间涨了多少」才是唯一结论。
    //      2. **行高参差** —— 只有「最大单日」多一枚日期徽章，同排其余三张没有，
    //         一排卡片高低不齐（这是最直观的"乱"）。
    //      3. **类型混杂** —— 三个是容量、一个是日期，却共用同一套大数值样式；
    //         「趋势起始日」被迫降级成小字号，看起来像一张"没数据的卡"。
    //      4. **区间没被陈述** —— 卡叫「区间汇总」却通篇没说是哪一段，
    //         范围控件还远在另一张卡里，数字悬空没有主语。
    //    重设计：主角（净增长，T2 大数值）+ 次要指标（统一单行的键值网格，
    //    日期退为同行备注）+ 区间陈述 + 容量预测，层级一次拉开。
    auto *c3 = new PanshiUi::PanshiSection(tr("区间汇总"),
                                           tr("所选范围内的增长结论与容量预测"));
    auto *sl = c3->content();

    // ---- 工具行：范围选择（左）+ 区间陈述（右） ----
    // 范围控件放在这里而非时间线：本卡已排到时间线前面，而控件定义的是「区间」
    // —— 让「选范围」和「看这段范围的结论」挨在一起，避免控件在结论下方。
    auto *toolRow = new QHBoxLayout;
    toolRow->setSpacing(PanshiUi::Space::S);

    // 范围选择：DButtonBox 分段控件（紧凑 + 可高亮当前所选范围），替代平铺按钮排。
    // 互斥高亮在 onPreset / onOpenCustomRange 里手动维护（见对应实现）。
    m_rangeBox = new DButtonBox(c3);
    QList<DButtonBoxButton *> rangeBtns;
    const int presets[] = {15, 30, 90, 180, 365};
    for (int days : presets) {
        auto *b = new DButtonBoxButton(tr("%1 天").arg(days), m_rangeBox);
        connect(b, &DButtonBoxButton::clicked, this, [this, days] { onPreset(days); });
        rangeBtns.append(b);
    }
    auto *customBtn = new DButtonBoxButton(tr("自定义"), m_rangeBox);
    connect(customBtn, &DButtonBoxButton::clicked, this, &DiskUsagePage::onOpenCustomRange);
    rangeBtns.append(customBtn);
    m_rangeBox->setButtonList(rangeBtns, true);
    toolRow->addWidget(m_rangeBox, 0, Qt::AlignVCenter);

    // 区间陈述：补上数字的主语（哪一段 · 多少天 · 几条记录）
    m_sumRangeLabel = Ui::rowDesc(QString());
    m_sumRangeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    toolRow->addWidget(m_sumRangeLabel, 1);
    sl->addLayout(toolRow);

    // ---- 英雄区：净增长（左） │ 容量预测（右） ----
    // 结构对偶：左边回答「已经涨了多少」（事实），右边回答「按这个速度还能撑多久」
    // （推论）。原设计把容量预测做成页面最底部的整宽 Banner，与净增长上下相隔
    // 一整个次要指标网格 —— 两者本是同一条因果链，却被拆到两个视区，读者要
    // 上下扫才能把「涨得多」和「撑不久」连起来。并置后这条因果链一眼可见。
    // 两栏都不套卡片底：本卡已在 PanshiSection 里，再套一层就是「卡中卡」；
    // 层级交给字号（T2 vs T4）、语义色与一条竖分隔线。
    m_heroRow = new QBoxLayout(QBoxLayout::LeftToRight);
    m_heroRow->setSpacing(PanshiUi::Space::XL);

    // ---- 左栏：净增长 ----
    // 顺序即层级（同 PanshiStateCard 的约定）：标签（这是什么）→ 值（结论）
    // → 副说明（来路）。
    m_heroMain = new QWidget(c3);
    auto *heroLay = new QVBoxLayout(m_heroMain);
    heroLay->setContentsMargins(0, 0, 0, 0);
    heroLay->setSpacing(PanshiUi::Space::XS);
    heroLay->addWidget(PanshiUi::makeLabel(tr("净增长"), PanshiUi::TextLevel::Desc,
                                           PanshiUi::Semantic::Muted));
    m_sumNet = PanshiUi::makeLabel(QStringLiteral("-"), PanshiUi::TextLevel::Value,
                                   PanshiUi::Semantic::Accent);
    m_sumNet->setWordWrap(true);
    heroLay->addWidget(m_sumNet);
    // 副说明：给出「涨了多少」的参照系，否则孤立的大数字没有落点
    m_sumBeginEnd = Ui::rowDesc(QString());
    heroLay->addWidget(m_sumBeginEnd);
    m_heroRow->addWidget(m_heroMain, 1);

    // 竖分隔线：把「事实」与「推论」两栏在视觉上分开。
    // 不用 QFrame::VLine —— 它走 QSS 边框色，不受 DPalette 主题切换管辖；
    // 这里用 palette 填色 + setFixedWidth(1)，颜色随 appPalette() 走。
    // 高度取整栏（Expanding 撑满），不跟内容高度走：半截短线读起来像
    // "这里少了一块"，整高竖线才读得出"这是两栏的分界"。
    m_heroSep = new QWidget(c3);
    m_heroSep->setFixedWidth(1);
    m_heroSep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    {
        auto *bar = new QWidget(m_heroSep);
        bar->setAutoFillBackground(true);
        QPalette pal = bar->palette();
        pal.setColor(QPalette::Window, DPalette::FrameBorder);
        bar->setPalette(pal);
        auto *sepLay = new QVBoxLayout(m_heroSep);
        sepLay->setContentsMargins(0, 0, 0, 0);
        sepLay->setSpacing(0);
        sepLay->addWidget(bar);   // 无 stretch → 随容器高度整高铺满
    }
    m_heroRow->addWidget(m_heroSep, 0);

    // ---- 右栏：容量预测 ----
    // 三段式与左栏同构（标签 → 值 → 备注），只是整体降一档字号：
    // 预测是「推论」不该和「事实」抢焦点，所以用 T4 而非 T2。
    m_forecastBox = new QWidget(c3);
    auto *fcLay = new QVBoxLayout(m_forecastBox);
    fcLay->setContentsMargins(0, 0, 0, 0);
    fcLay->setSpacing(PanshiUi::Space::XS);
    {
        // 标签行：图标 + 文字。图标随语义切换，是颜色之外的第二条信息通道
        // （色弱用户与深浅主题下都还能分辨）——沿用 Banner 的做法。
        auto *fcHead = new QWidget(m_forecastBox);
        auto *headLay = new QHBoxLayout(fcHead);
        headLay->setContentsMargins(0, 0, 0, 0);
        headLay->setSpacing(PanshiUi::Space::S);
        m_fcIcon = new DLabel(fcHead);
        m_fcIcon->setFixedSize(PanshiUi::Size::IconSmall, PanshiUi::Size::IconSmall);
        m_fcIcon->setAlignment(Qt::AlignCenter);
        headLay->addWidget(m_fcIcon, 0, Qt::AlignVCenter);
        m_fcLabel = PanshiUi::makeLabel(tr("容量预测"), PanshiUi::TextLevel::Desc,
                                        PanshiUi::Semantic::Muted);
        headLay->addWidget(m_fcLabel, 0, Qt::AlignVCenter);
        headLay->addStretch(1);
        fcLay->addWidget(fcHead);
    }
    m_fcValue = PanshiUi::makeLabel(QStringLiteral("-"), PanshiUi::TextLevel::ValueSmall,
                                    PanshiUi::Semantic::Warning);
    m_fcValue->setWordWrap(true);
    fcLay->addWidget(m_fcValue);
    m_fcNote = Ui::rowDesc(QString());
    fcLay->addWidget(m_fcNote);
    // 最小宽度：低于它「约 N 天后写满」会折成 4 行，右栏比左栏高出一倍。
    // 这是本项目「显式最小宽度是挤压唯一防线」的用法 —— 但方向相反：
    // 不是防挤压，而是**触发主动折行**（见 updateHeroLayout）。
    m_forecastBox->setMinimumWidth(220);
    m_heroRow->addWidget(m_forecastBox, 0);
    sl->addLayout(m_heroRow);
    updateHeroLayout();

    sl->addWidget(new PanshiUi::PanshiSeparator(c3));

    // ---- 次要指标：统一单行的键值网格 ----
    // 用 QGridLayout 而不是「每行一个 QHBoxLayout」—— 网格共享列宽，
    // 三行才会成为整齐的两列；分开写各自按内容伸缩，值参差不齐。
    // 每行的「值 + 备注」装进同一个宿主再右对齐，保证所有行的右边缘一致
    // （否则有备注的行和没备注的行会差出备注那一列的宽度）。
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setVerticalSpacing(PanshiUi::Space::XS);
    grid->setHorizontalSpacing(PanshiUi::Space::M);
    grid->setColumnStretch(0, 1);   // 标签列吃掉剩余空间 → 值靠右成列

    // 返回 {值, 备注}；备注为空时自动隐藏（不占位）
    auto addMetric = [grid](int row, const QString &title,
                            PanshiUi::Semantic s) -> QPair<DLabel *, DLabel *> {
        grid->addWidget(Ui::rowDesc(title), row, 0);
        auto *host = new QWidget;
        auto *hl = new QHBoxLayout(host);
        hl->setContentsMargins(0, 0, 0, 0);
        // 值与备注之间留 L(16)：默认 S(8) 时「1.42 GB  2026-10-10」读起来是
        // 一个整体，日期被误认成数值的一部分；拉开一档才看得出「值 / 出处」。
        hl->setSpacing(PanshiUi::Space::L);
        auto *v = PanshiUi::makeLabel(QStringLiteral("-"), PanshiUi::TextLevel::Body, s);
        hl->addWidget(v, 0, Qt::AlignVCenter);
        auto *note = PanshiUi::makeLabel(QString(), PanshiUi::TextLevel::Aux,
                                         PanshiUi::Semantic::Muted);
        note->hide();
        hl->addWidget(note, 0, Qt::AlignVCenter);
        grid->addWidget(host, row, 1, Qt::AlignRight | Qt::AlignVCenter);
        return qMakePair(v, note);
    };

    const auto dailyPair = addMetric(0, tr("日均增长"), PanshiUi::Semantic::Normal);
    m_sumDaily = dailyPair.first;
    // 最大单日：需留意的峰值 → Warning；日期退为同行小字备注，不再单独成卡
    const auto maxPair = addMetric(1, tr("最大单日"), PanshiUi::Semantic::Warning);
    m_sumMax = maxPair.first;
    m_sumMaxDay = maxPair.second;
    const auto trendPair = addMetric(2, tr("增长起点"), PanshiUi::Semantic::Normal);
    m_sumTrend = trendPair.first;
    sl->addLayout(grid);

    // 注：原挂在卡片底部的整宽容量预测 Banner 已移除，搬进上方英雄区右栏
    // （m_forecastBox）—— 见 buildSummaryCard 里的结构对偶说明。

    lay->addWidget(c3);
}

/**
 * 英雄区响应式重排。
 *
 * ⚠️ 当前 MainWindow 有 setMinimumSize(920, 620)，viewport 最窄约 720px ——
 *    **高于下面这个阈值，所以折行分支在现行窗口约束下走不到**。
 *    保留它是为了「窗口约束一旦放开（或页面被嵌入更窄容器）不必回头重做」，
 *    而不是现在就在生效。若哪天把最小宽度降到 600 以下，这里是真会被触发的。
 *
 * 阈值 520 的来历：左栏 T2 大数值「1.45 GB」+ 副说明约需 260px，
 * 右栏最小 220px，加间距 32 + 分隔线 1 = 513px。低于此值再横向摆，
 * 顶到最小宽度的挤压只会更糟 —— 不如主动折成上下，让两栏各自拿满整宽。
 *
 * 只在方向真正变化时改 setDirection：QBoxLayout::setDirection 每次调用都会
 * 触发布局重算，而 resizeEvent 在拖窗口时会连续触发几十次。
 */
void DiskUsagePage::updateHeroLayout()
{
    if (!m_heroRow)
        return;
    const bool wide = (m_contentScroll && m_contentScroll->viewport()
                           ? m_contentScroll->viewport()->width()
                           : width())
                      >= 520;
    const QBoxLayout::Direction want =
        wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
    if (m_heroRow->direction() != want) {
        m_heroRow->setDirection(want);
        // 竖分隔线只在并置时有意义；折成上下后留着就是一根莫名其妙的短竖线
        m_heroSep->setVisible(wide);
    }
}

// ---------- ③ 时间线 ----------
void DiskUsagePage::buildAnalysisSection(QVBoxLayout *lay)
{
    auto *sec = new PanshiUi::PanshiSection(
        tr("磁盘分析"),
        tr("同一份目录占用数据的三种看法：看结构（构成）、看变化（增长）、看全量（排行）"));
    auto *sl = sec->content();

    // ---- 上下文条：返回入口 + 面包屑 + 视角分段 ----
    //
    // 设计说明（2026-10-10）：「返回系统盘」原本挂在时间线卡底部，
    // 离用户"点目录"的位置太远 —— 于是导航**贴着操作对象**放：
    //   · 返回系统盘 = 回到根（时间线恢复系统盘 + 列表回到顶层）
    //   · 返回上级   = 逐级退出下钻
    //   · 面包屑     = 标出当前浏览位置，祖先层可直接点回去
    // 两个按钮**常驻只切可用状态**，不做显示/隐藏 —— 位置固定的控件比
    // 时有时无的控件好找（不用每次进目录模式都重新搜索"返回"在哪）。
    auto *ctxRow = new QHBoxLayout;
    ctxRow->setSpacing(PanshiUi::Space::S);

    m_backRootBtn = Ui::button(tr("返回系统盘"));
    m_backRootBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("go-home")));
    m_backRootBtn->setAccessibleName(tr("返回系统盘"));
    m_backRootBtn->setToolTip(tr("时间线恢复为系统盘占用，列表回到顶层目录"));
    connect(m_backRootBtn, &DPushButton::clicked,
            this, &DiskUsagePage::onBackToSystemTimeline);
    ctxRow->addWidget(m_backRootBtn);

    m_backUpBtn = Ui::button(tr("返回上级"));
    m_backUpBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("go-previous")));
    m_backUpBtn->setAccessibleName(tr("返回上级"));
    m_backUpBtn->setToolTip(tr("回到上一层目录"));
    connect(m_backUpBtn, &DPushButton::clicked,
            this, [this] { popBrowseTo(qMax(0, m_rankFrames.size() - 1)); });
    ctxRow->addWidget(m_backUpBtn);

    m_crumbBox = new QWidget(sec);
    m_crumbLay = new QHBoxLayout(m_crumbBox);
    m_crumbLay->setContentsMargins(0, 0, 0, 0);
    m_crumbLay->setSpacing(PanshiUi::Space::XS);
    ctxRow->addWidget(m_crumbBox, 1);

    m_viewBox = new DButtonBox(sec);
    QList<DButtonBoxButton *> viewBtns;
    const QString viewLabels[] = {tr("构成"), tr("增长"), tr("排行")};
    for (int i = 0; i < 3; ++i) {
        auto *b = new DButtonBoxButton(viewLabels[i], m_viewBox);
        b->setToolTip(viewHintFor(static_cast<ViewMode>(i)));
        connect(b, &DButtonBoxButton::clicked, this,
                [this, i] { onViewModeChanged(i); });
        viewBtns.append(b);
    }
    m_viewBox->setButtonList(viewBtns, true);
    syncViewBox();
    ctxRow->addWidget(m_viewBox, 0, Qt::AlignVCenter);
    sl->addLayout(ctxRow);

    m_viewHint = Ui::rowDesc(QString());
    sl->addWidget(m_viewHint);

    // ---- 表格：三视角共用一张 ----
    m_analysisTable = makeTable({tr("名称"), tr("大小"), tr("占比"), tr("操作")}, sec);
    applyTableWidths(m_analysisTable, 4);
    PanshiUi::setTableAccessibleName(m_analysisTable, tr("磁盘分析表"));
    connect(m_analysisTable, &QTableWidget::cellClicked,
            this, &DiskUsagePage::onAnalysisCellClicked);
    connect(m_analysisTable, &QTableWidget::cellDoubleClicked,
            this, &DiskUsagePage::onAnalysisCellDoubleClicked);
    sl->addWidget(m_analysisTable);

    // ---- 其它分区（全盘视角，与上表互补） ----
    sl->addWidget(new PanshiUi::PanshiSeparator(sec));
    auto *otherTitle = Ui::rowLabel(tr("其它分区"));
    otherTitle->setToolTip(tr("系统盘之外的其它分区（按设备去重，容量/已用取自 df）；"
                              "点击行右侧按钮可查看该分区内部构成"));
    sl->addWidget(otherTitle);
    auto *otherBox = new QWidget(sec);
    m_otherLay = new QVBoxLayout(otherBox);
    m_otherLay->setContentsMargins(0, 0, 0, 0);
    m_otherLay->setSpacing(PanshiUi::Space::XS);
    sl->addWidget(otherBox);
    rebuildOtherPartitions();

    lay->addWidget(sec);
}

// ---------- ④b 视角说明（分段按钮 tooltip 与提示行共用同一份文案） ----------
QString DiskUsagePage::viewHintFor(ViewMode v)
{
    switch (v) {
    case ViewMode::Composition:
        return tr("构成：系统盘顶层目录的占用结构（前 5 项 + 其他），占比 = 占系统盘已用");
    case ViewMode::Growth:
        return tr("增长：所选日期相对前一日有增长的目录/文件（最多 8 项），"
                  "占比 = 该目录增量占当日总增量");
    case ViewMode::Ranking:
        return tr("排行：当前目录下所有子项从大到小；单击行让上方时间线跟随，双击进入子目录");
    }
    return QString();
}

/** 「占比」单元格：右侧百分比数字 + 左侧细占比条（原磁盘构成的条并入表格） */
void DiskUsagePage::setShareCell(QTableWidget *t, int row, int col, double pct, bool valid)
{
    auto *holder = new QWidget(t);
    auto *hl = new QHBoxLayout(holder);
    hl->setContentsMargins(6, 0, 6, 0);
    hl->setSpacing(6);

    auto *val = new DLabel(valid ? QString::number(pct, 'f', 1) + QStringLiteral("%")
                                 : QStringLiteral("—"),
                           holder);
    DFontSizeManager::instance()->bind(val, DFontSizeManager::T6);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    val->setFixedWidth(46);

    auto *bar = new PanshiUi::PanshiFlatProgress(holder);
    bar->setRange(0, 1000);
    bar->setFixedWidth(56);
    bar->setFixedHeight(PanshiUi::Size::ThinProgress + 4);
    bar->setValue(valid ? qBound(0, int(pct * 10.0), 1000) : 0);

    hl->addWidget(bar, 0, Qt::AlignVCenter);
    hl->addWidget(val, 0, Qt::AlignVCenter);
    t->setCellWidget(row, col, holder);
}

// ---------- ④c 表格渲染：三个视角共用的唯一出口 ----------
void DiskUsagePage::updateAnalysis()
{
    if (!m_analysisTable)
        return;
    // 同 updateDayDetail：空数据占位那条 early return 也得补无障碍，用守卫兜住
    const A11yOnScopeExit a11y{m_analysisTable};

    struct Row
    {
        QString path;         // 空 = 信息行（不可打开 / 不可下钻）
        QString name;
        QString sizeText;
        double pct = 0.0;
        bool pctValid = false;
        bool isDir = true;
        bool drillable = false;
    };
    QList<Row> rows;
    QString emptyHint;

    if (m_view == ViewMode::Growth) {
        // ---------- 视角：增长（口径 = 某一天的环比增量） ----------
        if (!m_timelineDir.isEmpty()) {
            const int idx = dateIndexInRecords(m_records, m_selected);
            if (idx < 0) {
                emptyHint = tr("所选日期不在当前加载范围内");
            } else {
                const qint64 cur = dirSizeInRecord(m_records.at(idx), m_timelineDir);
                const qint64 prev =
                    idx > 0 ? dirSizeInRecord(m_records.at(idx - 1), m_timelineDir) : -1;
                Row r1;
                r1.path = m_timelineDir;
                r1.name = tr("目录 %1").arg(m_timelineDir);
                r1.sizeText = cur >= 0 ? formatSize(cur) : tr("无数据");
                rows.append(r1);
                Row r2;
                if (prev >= 0 && cur >= 0) {
                    const qint64 diff = cur - prev;
                    r2.name = tr("较前一日变化");
                    r2.sizeText =
                        (diff >= 0 ? QStringLiteral("+") : QString()) + formatSize(diff);
                } else {
                    r2.name = tr("前一日暂无数据");
                }
                rows.append(r2);
            }
        } else {
            const int idx = dateIndexInRecords(m_records, m_selected);
            if (idx < 0) {
                emptyHint = tr("点击时间线上任意数据点，查看该日的增长来源");
            } else {
                DiskDayRecord prev;
                if (idx > 0)
                    prev = m_records.at(idx - 1);
                const qint64 totalGrowth =
                    idx > 0 ? m_records.at(idx).total - m_records.at(idx - 1).total : 0;
                const QList<DiskItem> items =
                    DiskHistoryStore::topGrowthItems(prev, m_records.at(idx), 8);
                if (items.isEmpty()) {
                    emptyHint = tr("该日无显著增长");
                } else {
                    for (const DiskItem &it : items) {
                        Row r;
                        r.path = it.path;
                        r.name = it.path;
                        r.sizeText = formatSize(it.size);
                        r.isDir = it.isDir;
                        r.pctValid = totalGrowth > 0;
                        r.pct = totalGrowth > 0 ? double(it.size) * 100.0 / totalGrowth : 0.0;
                        rows.append(r);
                    }
                }
            }
        }
    } else {
        // ---------- 视角：构成 / 排行（同一份子项，只差截取长度） ----------
        QList<DiskItem> items;
        qint64 total = 0;
        if (m_rankCurrentPath.isEmpty()) {
            if (!m_records.isEmpty()) {
                items = m_records.last().topLevel;
                total = m_records.last().total;
            }
        } else {
            items = m_rankItems;          // 下钻结果（异步，为空即尚未就绪）
            for (const DiskItem &it : items)
                total += it.size;
        }
        std::sort(items.begin(), items.end(),
                  [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });

        if (items.isEmpty()) {
            emptyHint = m_scanner.isBusy()
                            ? tr("正在读取子目录…")
                            : tr("暂无数据：请开启监控或点击“立即扫描”");
        } else {
            const bool compo = (m_view == ViewMode::Composition);
            const int n = qMin(items.size(), compo ? 5 : 200);
            qint64 sum = 0;
            for (int i = 0; i < n; ++i)
                sum += items.at(i).size;
            // 占比分母：子项之和与区间总占用取较大者。
            // 顶层扫描已加 -x（只统计根分区自身），新记录下两者同口径；
            // 历史旧记录（-x 之前）仍可能含跨分区数据，硬用 df / 已用会算出
            // >100% 的假占比 —— 取 max 后任何数据下都 ≤100%。
            const qint64 denom = qMax(total, sum);
            for (int i = 0; i < n; ++i) {
                Row r;
                r.path = items.at(i).path;
                r.name = items.at(i).path;
                r.sizeText = formatSize(items.at(i).size);
                r.isDir = items.at(i).isDir;
                r.drillable = items.at(i).isDir;
                r.pctValid = denom > 0;
                r.pct = denom > 0 ? double(items.at(i).size) * 100.0 / denom : 0.0;
                rows.append(r);
            }
            if (compo) {
                const qint64 other = total - sum;
                if (other > 0) {
                    Row r;
                    r.name = tr("其他");
                    r.sizeText = formatSize(other);
                    r.pctValid = denom > 0;
                    r.pct = denom > 0 ? double(other) * 100.0 / denom : 0.0;
                    rows.append(r);
                }
            }
        }
    }

    m_viewItems.clear();
    // 🚨 重绘前必须 clearContents()（2026-10-10 修复「该日无显著增长」糊字 Bug）。
    //
    // 缺陷机制（已实测确认，见 tools/diskshot.cpp 的 A6 双向验证）：
    // 本表对不同列用了**不同的填充方式** —— 名称/占比/操作列走 setCellWidget，
    // 大小列与空态提示行走 setItem。而 Qt 允许 QTableWidgetItem 与 cellWidget
    // 在同一格**共存，两者都会绘制**，且：
    //   · setRowCount() 扩容不清旧格；
    //   · setRowCount() 缩小只删多余行，**存活行的 cellWidget 保留**；
    //   · setCellWidget() 不清同格已有的 item；
    //   · clearContents() **不清 span**（空态合并过的格扩容后会一直合并）。
    // 于是「增长视角空数据 → 切回排行」这类切换必然留下 item/widget 并存，
    // 上一视角的文案（"该日无显著增长"）就叠在新一行的 ElidedLabel 上糊成一团。
    // 清空责任只能落在渲染循环之前 —— 任何一列的填充动作都不会顺手清掉它。
    m_analysisTable->clearContents();
    m_analysisTable->setRowCount(rows.size());
    m_analysisTable->clearSpans();
    for (int i = 0; i < rows.size(); ++i) {
        const Row &r = rows.at(i);
        setPathCell(m_analysisTable, i, 0, r.path.isEmpty() ? r.name : r.path);
        m_analysisTable->setItem(i, 1, new QTableWidgetItem(r.sizeText));
        setShareCell(m_analysisTable, i, 2, r.pct, r.pctValid);
        if (!r.path.isEmpty()) {
            auto *btn = makeOpenButton(r.path);
            const QString path = r.path;
            connect(btn, &DIconButton::clicked, this,
                    [this, path] { openInFileManager(path); });
            m_analysisTable->setCellWidget(i, 3, btn);
            m_viewItems.append(DiskItem{r.path, 0, r.isDir});
        } else {
            m_analysisTable->setItem(i, 3, new QTableWidgetItem());
        }
    }

    if (rows.isEmpty()) {
        // 空态提示合并整行：提示语是「一句话说明」，散在四格里会让人
        // 以为第 1 列是空数据、第 3 列是空操作。
        m_analysisTable->setSpan(0, 0, 1, 4);
        m_analysisTable->setItem(0, 0, new QTableWidgetItem(emptyHint));
    }

    // 🚨 上下文条（返回按钮状态 + 面包屑 + 视角说明）必须跟着渲染一起刷新。
    //
    // 这里是三个视角**唯一**的渲染出口，所以把 updateContextBar() 挂在这里，
    // 而不是散在各个调用点逐个补：
    //   初始化（updateSummary）/ 下钻（onDrillFinished）/ 返回（onBackToSystemTimeline）
    //   / 切视角（onViewModeChanged）/ 单击行（onAnalysisCellClicked）全部自动跟上。
    // 漏在调用点的代价是**静默失效**：按钮灰着但也不报错，用户只会觉得"返回键坏了"。
    updateContextBar();
}

// ---------- ④d 上下文条：返回按钮状态 + 面包屑 ----------
void DiskUsagePage::updateContextBar()
{
    // 任一状态非"根+系统盘"即可返回：下钻层非空，或时间线正跟在某个目录上
    const bool canBack = !m_rankFrames.isEmpty() || !m_timelineDir.isEmpty();
    if (m_backRootBtn)
        m_backRootBtn->setEnabled(canBack);
    if (m_backUpBtn) {
        const bool hasFrames = !m_rankFrames.isEmpty();
        m_backUpBtn->setVisible(hasFrames);
        m_backUpBtn->setEnabled(hasFrames);
    }
    if (m_viewHint)
        m_viewHint->setText(viewHintFor(m_view));
    rebuildBreadcrumb();
}

void DiskUsagePage::rebuildBreadcrumb()
{
    if (!m_crumbLay)
        return;
    while (QLayoutItem *item = m_crumbLay->takeAt(0)) {
        if (QWidget *w = item->widget())
            delete w;
        delete item;
    }

    // 第 0 级「系统盘」：点击 = 返回系统盘（与左侧按钮同义，就近可达）
    auto *rootBtn = new DPushButton(tr("系统盘"), m_crumbBox);
    rootBtn->setFlat(true);
    rootBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("go-home")));
    rootBtn->setIconSize(QSize(PanshiUi::Size::IconSmall, PanshiUi::Size::IconSmall));
    rootBtn->setCursor(Qt::PointingHandCursor);
    rootBtn->setAccessibleName(tr("返回系统盘"));
    rootBtn->setToolTip(tr("返回系统盘时间线与顶层目录"));
    connect(rootBtn, &DPushButton::clicked, this, &DiskUsagePage::onBackToSystemTimeline);
    m_crumbLay->addWidget(rootBtn);

    // 逐段展开当前浏览路径（/var/log → 系统盘 › var › log）
    //
    // 折叠规则：路径很深时（如 /media/Another/Date/…/build/.tmp/…）全展开会把
    // 右侧「视角切换」挤出可视区 —— 每行控件都有最小宽度，横向挤不动就只能牺牲别人
    // （项目教训：显式最小宽度是挤压唯一防线，但这里不能给面包屑留无限宽）。
    // 所以只保留末 3 段，前面用「…」代替；祖先层仍可通过剩下的按钮回退。
    const QStringList segs =
        m_rankCurrentPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const int keepTail = 3;
    const int first = qMax(0, segs.size() - keepTail);
    if (first > 0) {
        auto *ellip = new DLabel(QStringLiteral("…"), m_crumbBox);
        ellip->setForegroundRole(DPalette::TextTips);
        ellip->setToolTip(m_rankCurrentPath);
        m_crumbLay->addWidget(ellip);
    }
    for (int i = first; i < segs.size(); ++i) {
        auto *sep = new DLabel(QStringLiteral("›"), m_crumbBox);
        sep->setForegroundRole(DPalette::TextTips);
        m_crumbLay->addWidget(sep);

        auto *b = new DPushButton(segs.at(i), m_crumbBox);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        const bool last = (i == segs.size() - 1);
        b->setEnabled(!last);   // 当前层本身不可点
        b->setAccessibleName(last ? segs.at(i) : tr("回到 %1").arg(segs.at(i)));
        if (!last)
            b->setToolTip(tr("回到 %1").arg(segs.at(i)));
        const int lvl = i + 1;
        connect(b, &DPushButton::clicked, this, [this, lvl] { popBrowseTo(lvl); });
        m_crumbLay->addWidget(b);
    }
    m_crumbLay->addStretch();
}

void DiskUsagePage::popBrowseTo(int level)
{
    while (m_rankFrames.size() > level) {
        const auto frame = m_rankFrames.takeLast();
        m_rankCurrentPath = frame.first;
        m_rankItems = frame.second;
    }
    if (m_rankFrames.isEmpty()) {
        m_rankCurrentPath.clear();
        m_timelineDir.clear();   // 回到根 = 时间线恢复系统盘
        m_rankItems = m_records.isEmpty() ? QList<DiskItem>() : m_records.last().topLevel;
    } else {
        m_timelineDir = m_rankCurrentPath;
    }
    updateTimelineSeries();
    updateAnalysis();
}

// ---------- ④e 表格交互 ----------
void DiskUsagePage::onViewModeChanged(int index)
{
    m_view = static_cast<ViewMode>(index);
    syncViewBox();
    updateAnalysis();
}

void DiskUsagePage::syncViewBox()
{
    if (!m_viewBox)
        return;
    const QList<QAbstractButton *> btns = m_viewBox->buttonList();
    const int cur = static_cast<int>(m_view);
    for (int i = 0; i < btns.size(); ++i)
        btns.at(i)->setChecked(i == cur);
}

void DiskUsagePage::onAnalysisCellClicked(int row, int column)
{
    Q_UNUSED(column);
    if (m_view == ViewMode::Growth && !m_timelineDir.isEmpty())
        return;                  // 目录模式的两行是信息行，不参与切换
    if (row < 0 || row >= m_viewItems.size())
        return;
    const DiskItem &it = m_viewItems.at(row);
    if (!it.isDir)
        return;
    // 单击目录行 = 选中它，上方时间线随之切为该目录的占用变化
    m_timelineDir = it.path;
    updateTimelineSeries();
    updateContextBar();
}

void DiskUsagePage::onAnalysisCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (m_view == ViewMode::Growth)
        return;                  // 增长口径不按下钻：进去就脱离「当日增量」了
    if (row < 0 || row >= m_viewItems.size())
        return;
    const DiskItem &it = m_viewItems.at(row);
    if (!it.isDir)
        return;
    if (m_scanner.isBusy())
        return;
    m_scanner.setAsRoot(true);
    m_scanner.setRootPolicy(DiskScanner::RootPolicy::AllowPrompt);
    showStatus(tr("正在读取 %1 的子目录…").arg(it.path));
    m_scanner.drillInto(it.path);
}

void DiskUsagePage::onBreadcrumbJump(int level)
{
    popBrowseTo(level);
}

// ---------- ④a2 其它分区行列表（全盘视角） ----------
void DiskUsagePage::rebuildOtherPartitions()
{
    if (!m_otherLay)
        return;
    // 清空旧行（无信号连接的行直接 delete；下钻按钮连接挂在页面 this 上，安全）
    while (QLayoutItem *item = m_otherLay->takeAt(0)) {
        if (QWidget *w = item->widget())
            delete w;
        delete item;
    }

    const QList<DiskMountInfo> parts = DiskScanner::listOtherPartitions();
    if (parts.isEmpty()) {
        auto *empty = Ui::rowDesc(tr("未检测到其它分区（单分区布局）"));
        m_otherLay->addWidget(empty);
        return;
    }

    for (const DiskMountInfo &p : parts) {
        auto *row = new QWidget(m_otherLay->parentWidget());
        auto *hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);
        auto *name = new Ui::ElidedLabel(p.mountPoint, row);
        name->setToolTip(tr("%1 · %2 分区\n容量 %3 · 已用 %4")
                             .arg(p.device, p.fsType, formatSize(p.capacity), formatSize(p.used)));
        name->setAccessibleName(p.mountPoint);
        hl->addWidget(name, 1);
        auto *bar = new PanshiUi::PanshiFlatProgress(row);
        bar->setRange(0, 1000);
        bar->setFixedWidth(120);
        bar->setFixedHeight(6);
        bar->setValue(p.capacity > 0 ? int(qint64(p.used) * 1000 / p.capacity) : 0);
        bar->setAccessibleName(p.mountPoint);
        hl->addWidget(bar, 0, Qt::AlignVCenter);
        const double pct = p.capacity > 0 ? double(p.used) * 100.0 / p.capacity : 0.0;
        auto *val = new DLabel(QStringLiteral("%1 / %2 · %3%")
                                   .arg(formatSizeCompact(p.used), formatSizeCompact(p.capacity),
                                        QString::number(pct, 'f', 0)));
        val->setForegroundRole(DPalette::TextTitle);
        DFontSizeManager::instance()->bind(val, DFontSizeManager::T6);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        // 固定宽避免数字位数变化时整行抖动
        val->setFixedWidth(150);
        hl->addWidget(val, 0, Qt::AlignVCenter);
        // 下钻按钮：查看该分区内部构成（复用占用排行的 drill 流程）
        auto *btn = new DIconButton(row);
        btn->setIcon(DIconTheme::findQIcon(QStringLiteral("go-next")));
        btn->setFixedSize(28, 28);
        const QString label = tr("查看 %1 内部构成").arg(p.mountPoint);
        btn->setToolTip(label);
        btn->setAccessibleName(label);
        btn->setCursor(Qt::PointingHandCursor);
        const QString mp = p.mountPoint;
        connect(btn, &DIconButton::clicked, this, [this, mp] { onDrillPartition(mp); });
        hl->addWidget(btn, 0, Qt::AlignVCenter);
        m_otherLay->addWidget(row);
    }
}

// ---------- ④a3 其它分区下钻 ----------
void DiskUsagePage::onDrillPartition(const QString &mountPoint)
{
    if (m_scanner.isBusy())
        return;
    // 分区内部构成用「排行」视角展示（原为切到占用排行标签）
    m_view = ViewMode::Ranking;
    syncViewBox();
    m_scanner.setAsRoot(true);
    m_scanner.setRootPolicy(DiskScanner::RootPolicy::AllowPrompt);
    showStatus(tr("正在读取 %1 的子目录…").arg(mountPoint));
    m_scanner.drillInto(mountPoint);
}

// ---------- ④ 独立设置窗口 ----------
void DiskUsagePage::buildSettingsDialog()
{
    m_settingsDlg = new DDialog(this);
    PanshiUi::disableDialogTransparency(m_settingsDlg);   // 关闭 DTK 弹窗默认的模糊/透明效果
    // 降为 WindowModal：设置对话框开着时主窗口仍可拖动
    // （根因见 components.h::makeDialogNonBlockingWindowModal）
    PanshiUi::makeDialogNonBlockingWindowModal(m_settingsDlg);
    m_settingsDlg->setTitle(tr("磁盘空间设置"));
    m_settingsDlg->setIcon(DIconTheme::findQIcon(QStringLiteral("preferences-system")));

    auto *scroll = new QScrollArea(m_settingsDlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedWidth(560);
    scroll->setFixedHeight(580);
    auto *content = new QWidget(scroll);
    scroll->setWidget(content);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(2, 2, 8, 2);
    cl->setSpacing(PanshiUi::Space::M);

    // 分段：每个分组 = 一个 PanshiSection，行 = PanshiRow（左标题+说明 / 右控件），
    // 与 policypage 等设置页同构（原实现是「分隔线 + panshiGroupTitle + 手拼 QHBoxLayout」）
    auto makeSection = [cl](const QString &title) {
        auto *sec = new PanshiUi::PanshiSection(title);
        cl->addWidget(sec);
        return sec->content();
    };
    auto addRow = [](QVBoxLayout *body, const QString &label, QWidget *w,
                     const QString &desc = QString()) {
        auto *row = new PanshiUi::PanshiRow;
        row->setTitle(label);
        if (!desc.isEmpty())
            row->setDescription(desc);
        row->addControl(w);
        body->addWidget(row);
    };

    // 总开关（独立一组，开关行内不塞说明，说明随行）
    {
        auto *sec = new PanshiUi::PanshiSection(tr("监控"));
        cl->addWidget(sec);
        m_monitorSwitch = new DSwitchButton;
        auto *row = new PanshiUi::PanshiRow;
        row->setTitle(tr("启用磁盘监控"));
        row->setDescription(tr("开启后立即建立基线，由后台服务每 15 分钟采样一次并自动告警"));
        row->addControl(m_monitorSwitch);
        sec->content()->addWidget(row);
        connect(m_monitorSwitch, &DSwitchButton::toggled, this, &DiskUsagePage::onMonitorToggled);
    }

    // 告警规则
    {
        auto *body = makeSection(tr("告警规则"));
        m_alertModeBox = new DComboBox;
        m_alertModeBox->addItems({tr("智能基线"), tr("固定阈值")});
        connect(m_alertModeBox, QOverload<int>::of(&DComboBox::currentIndexChanged),
                this, &DiskUsagePage::onAlertModeChanged);
        m_fixedThresholdSpin = new DSpinBox;
        m_fixedThresholdSpin->setRange(64, 102400);
        m_fixedThresholdSpin->setSuffix(QStringLiteral(" MB"));
        m_multiplierSpin = new DDoubleSpinBox;
        m_multiplierSpin->setRange(1.0, 10.0);
        m_multiplierSpin->setDecimals(1);
        // 步进必须显式给 0.1（缺陷 O6-1，2026-10-02 实测）：
        // QDoubleSpinBox 的默认 singleStep 是 **1.0**，不随 decimals 变化 ——
        // 于是 setDecimals(1) 只影响显示（3.0），键盘 ↑↓ 却一次跳 1.0（3.0→4.0），
        // 出现「显示一位小数但步进整数」的错配。
        // 基线倍数的语义本就是细粒度系数（默认 3.0 = 近 7 天日均 × 3），
        // 0.1 步进才符合预期粒度。
        // ⚠️ API 名是 QDoubleSpinBox 的 setSingleStep（DDoubleSpinBox 无 setStep 成员），
        // 编译代修 2026-10-02。
        m_multiplierSpin->setSingleStep(0.1);
        m_multiplierSpin->setValue(3.0);
        m_cooldownSpin = new DSpinBox;
        m_cooldownSpin->setRange(1, 72);
        m_cooldownSpin->setSuffix(tr(" 小时"));
        // 数值控件显式最小宽度：与 PanshiRow 的控件区下限配合，
        // 确保「当前值 + 后缀」在任何窗口宽度下都可读（不改各自的 sizeHint 逻辑）
        for (QWidget *w : {static_cast<QWidget *>(m_alertModeBox),
                           static_cast<QWidget *>(m_fixedThresholdSpin),
                           static_cast<QWidget *>(m_multiplierSpin),
                           static_cast<QWidget *>(m_cooldownSpin)})
            w->setMinimumWidth(140);
        addRow(body, tr("告警模式"), m_alertModeBox);
        addRow(body, tr("固定阈值"), m_fixedThresholdSpin, tr("增长超过该值触发告警（固定阈值模式）"));
        addRow(body, tr("基线倍数"), m_multiplierSpin,
               tr("智能基线 = 近 7 天日均增长 × 该倍数，且不小于最小告警值"));
        addRow(body, tr("告警冷却"), m_cooldownSpin, tr("同一目录冷却期内只告警一次，避免刷屏"));
    }

    // 排除目录
    {
        auto *body = makeSection(tr("排除目录"));
        auto *exRow = new QWidget;
        auto *exLay = new QHBoxLayout(exRow);
        exLay->setContentsMargins(0, 0, 0, 0);
        exLay->setSpacing(PanshiUi::Space::S);
        m_excludeEdit = new DLineEdit;
        m_excludeEdit->setPlaceholderText(tr("输入目录，如 /home/Test/Downloads"));
        auto *addExBtn = Ui::button(tr("添加"));
        connect(addExBtn, &DPushButton::clicked, this, &DiskUsagePage::onAddExclude);
        exLay->addWidget(m_excludeEdit, 1);
        exLay->addWidget(addExBtn);
        body->addWidget(exRow);
        // 排除目录列表：DListView（QListView 系，配 QStringListModel）。
        // 原 QListWidget 是「自带模型」的便利类；DListView 走标准 model/view，
        // 故显式持有一个 QStringListModel（成员 m_excludeModel，随页面生命周期存活）。
        // 读当前行：QListWidget::currentRow() → DListView currentIndex().row()
        // （无选中项时 currentIndex() 为无效索引，row() 返回 -1，语义与原实现一致）。
        m_excludeList = new DListView;
        m_excludeList->setMaximumHeight(110);
        m_excludeList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_excludeModel = new QStringListModel(m_excludeList);
        m_excludeList->setModel(m_excludeModel);
        body->addWidget(m_excludeList);
        auto *delExBtn = Ui::button(tr("移除选中"), false, true);
        connect(delExBtn, &DPushButton::clicked, this, &DiskUsagePage::onRemoveExclude);
        body->addWidget(delExBtn, 0, Qt::AlignLeft);
    }

    // 数据存储
    {
        auto *body = makeSection(tr("数据存储"));
        m_retentionSpin = new DSpinBox;
        m_retentionSpin->setRange(90, 730);
        m_retentionSpin->setSuffix(tr(" 天"));
        m_budgetBox = new DComboBox;
        m_budgetBox->addItems({QStringLiteral("256 MB"), QStringLiteral("512 MB"),
                               QStringLiteral("1024 MB")});
        // 同上：本组两个控件也加显式下限（保留天数带「天」后缀，易被切）
        m_retentionSpin->setMinimumWidth(140);
        m_budgetBox->setMinimumWidth(140);
        m_storageUsed = Ui::rowDesc(QString());
        addRow(body, tr("保留天数"), m_retentionSpin);
        addRow(body, tr("存储预算"), m_budgetBox,
               tr("历史数据本地占用上限，超出预算自动降级保存"));
        addRow(body, tr("当前占用"), m_storageUsed);
        auto *clearBtn = Ui::button(tr("清除全部监控数据"), false, true);
        connect(clearBtn, &DPushButton::clicked, this, &DiskUsagePage::onClearHistory);
        addRow(body, tr("数据管理"), clearBtn);
    }

    m_settingsDlg->addContent(scroll);
    m_settingsDlg->addButton(tr("关闭"), false, DDialog::ButtonNormal);
    connectSettingsWidgets();
}

void DiskUsagePage::onOpenSettings()
{
    updateMonitorSettingsUi();   // 打开前同步最新配置
    m_settingsDlg->exec();
}

// ----------------------------------------------------------------------
// 数据加载
// ----------------------------------------------------------------------
void DiskUsagePage::refresh()
{
    if (!m_initialized)
        return;
    if (!m_checkedOnce) {
        m_checkedOnce = true;
        beginCheck();
    }
    applyConfigToStore();
    updateOverview();
    updateServiceStatus();
    updateMonitorSettingsUi();
    if (m_records.isEmpty())
        onPreset(15);
}

void DiskUsagePage::shutdown()
{
    m_scanner.cancel();
    if (m_rangeId)
        m_store.cancelRange(m_rangeId);
}

void DiskUsagePage::focusDate(const QDate &date)
{
    if (!date.isValid())
        return;
    m_selected = date;
    requestRange(date.addDays(-3), date.addDays(3));
}

void DiskUsagePage::requestRange(const QDate &from, const QDate &to)
{
    if (!from.isValid() || !to.isValid() || from > to)
        return;
    m_from = from;
    m_to = to;
    if (m_rangeId)
        m_store.cancelRange(m_rangeId);
    m_chart->setLoading(true, tr("正在加载 %1 ~ %2 的数据…")
                                 .arg(from.toString(QStringLiteral("yyyy-MM-dd")),
                                      to.toString(QStringLiteral("yyyy-MM-dd"))));
    m_loadStatus->setText(QString());
    m_rangeId = m_store.requestRange(from, to);
}

void DiskUsagePage::onPreset(int days)
{
    const QDate today = QDate::currentDate();
    requestRange(today.addDays(-(days - 1)), today);
    // 高亮对应的范围分段（DButtonBox 手动互斥：选中当前项、取消其余）
    if (m_rangeBox) {
        const QList<QAbstractButton *> btns = m_rangeBox->buttonList();
        int idx = -1;
        if (days == 15) idx = 0;
        else if (days == 30) idx = 1;
        else if (days == 90) idx = 2;
        else if (days == 180) idx = 3;
        else if (days == 365) idx = 4;
        for (int i = 0; i < btns.size(); ++i)
            btns.at(i)->setChecked(i == idx);
    }
}

void DiskUsagePage::onOpenCustomRange()
{
    // 单日历区间选择（阶段 3-7b）。
    //
    // 原实现是**纵排两个 QCalendarWidget**（开始日期 + 结束日期），问题有三：
    //   1. 两个整月网格纵向堆叠 ≈ 700px 高，对话框被迫很高，观感与其它页不一致；
    //   2. 起止相互约束全靠「点击回调里比大小后决定要不要刷新摘要」，
    //      无效点击（终点早于起点）时**摘要不更新但选中日已变**，出现
    //      「界面高亮与摘要文字不一致」的状态分叉；
    //   3. 确定时还要 std::swap 兜底，说明上面的不一致确实会发生。
    //
    // 现改为**一个日历 + 两次点击定区间**（Qt 官方推荐的 range-pick 范式）：
    //   第 1 次点击 → 设为起点，清空终点（等第二次点击）；
    //   第 2 次点击 → 若早于起点则自动成为新的起点（终点清空待再点），
    //                否则设为终点。结束时统一 std::swap 保证 from <= to。
    // 这样「摘要」永远由同一份 from/to 状态派生，不会再出现分叉。
    DDialog dlg(this);
    PanshiUi::disableDialogTransparency(&dlg);   // 关闭 DTK 弹窗默认的模糊/透明效果
    // 降为 WindowModal：对话框开着时主窗口仍可拖动
    // （根因见 components.h::makeDialogNonBlockingWindowModal）
    PanshiUi::makeDialogNonBlockingWindowModal(&dlg);
    dlg.setTitle(tr("选择日期范围"));
    dlg.setIcon(DIconTheme::findQIcon(QStringLiteral("appointment-soon")));

    // 区间状态：以成员当前范围（m_from/m_to）为初值；无效则回落到最近 7 天
    QDate from = m_from.isValid() ? m_from : QDate::currentDate().addDays(-6);
    QDate to = m_to.isValid() ? m_to : QDate::currentDate();
    if (from > to)
        std::swap(from, to);

    auto *content = new QWidget(&dlg);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 4, 0, 4);
    cl->setSpacing(8);

    auto *hint = Ui::rowDesc(tr("第 1 次点击选择开始日期，第 2 次点击选择结束日期"
                                "（第二次早于首次时会自动改为新的开始日期）"));
    cl->addWidget(hint);

    auto *cal = new QCalendarWidget(content);
    cal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    cal->setMaximumDate(QDate::currentDate());   // 未来日期无监控数据，禁选
    cal->setSelectedDate(from);
    cl->addWidget(cal);

    // 区间状态：用 std::shared_ptr 承载，由 lambda 按值捕获。
    // 这样避免了「按引用捕获函数局部变量，而连接挂在比它更长寿的对象上」的隐患。
    struct RangeState {
        QDate from;
        QDate to;
        bool complete = true;   // 初值视为「已选定完整区间」，首次点击即开始重选
    };
    auto st = std::make_shared<RangeState>(RangeState{from, to, true});

    // 已选范围摘要：**唯一数据源是 st 里的 from/to**。
    //
    // ⚠️ 缺陷 O1-1 修复（2026-10-02 实测「摘要文案与高亮分叉」）：
    //   原实现只让**摘要**读 st，日历的高亮仍由 QCalendarWidget **自行维护**
    //   （点击时 Qt 内部自动改 selectedDate，另外切换月份时某些主题/风格
    //   也会改动选中日）。于是「高亮」与「摘要」是两个独立状态源 ——
    //   原先声称的「单一数据源」只覆盖了摘要一侧，日历高亮从未被 st 同步过。
    //
    // 修法：让**摘要与高亮都从 st 派生** —— refreshSummary 里同时回写
    // cal->setSelectedDate()。这样任一路径（点击 / 月份切换 / setMaximumDate
    // 夹取）造成的日历内部漂移，都会在下一次 refresh 时被拉回 st，
    // 两者在结构上不再可能分叉。
    //
    // 同时补上「未选完」的显式提示：complete == false 时 from == to，
    // 若仍显示「已选范围：X ~ X」，用户会误以为已选定单日区间而直接确定。
    auto *summary = Ui::rowLabel(QString());
    auto refreshSummary = [summary, cal, st] {
        if (!st->complete) {
            summary->setText(tr("已选开始日期：%1，请再点击选择结束日期")
                                 .arg(st->from.toString(QStringLiteral("yyyy-MM-dd"))));
        } else {
            summary->setText(tr("已选范围：%1 ~ %2")
                                 .arg(st->from.toString(QStringLiteral("yyyy-MM-dd")),
                                      st->to.toString(QStringLiteral("yyyy-MM-dd"))));
        }
        // 高亮同样由 st 驱动（见上方说明）：区间未选完时高亮起点，
        // 选完时高亮终点 —— 与摘要表达的是同一份状态。
        cal->setSelectedDate(st->complete ? st->to : st->from);
    };
    refreshSummary();
    cl->addWidget(summary);

    connect(cal, &QCalendarWidget::clicked, &dlg,
            [st, refreshSummary](const QDate &d) {
                if (!st->complete) {
                    // 第二次点击：早于起点则改起点（仍等终点），否则定终点
                    if (d < st->from) {
                        st->from = d;
                        st->complete = false;
                    } else {
                        st->to = d;
                        st->complete = true;
                    }
                } else {
                    // 第一次点击：起点落定，终点暂同起点，等待第二次
                    st->from = d;
                    st->to = d;
                    st->complete = false;
                }
                refreshSummary();
            });

    content->setFixedWidth(420);
    dlg.addContent(content);
    dlg.addButton(tr("取消"), false, DDialog::ButtonNormal);
    dlg.addButton(tr("确定"), true, DDialog::ButtonRecommend);

    const int ret = dlg.exec();
    // 自定义区间不归属任何预设：无论确认还是取消，都取消所有分段高亮
    // （点在「自定义」段上时它会被 DButtonBox 勾选，取消对话框必须一并撤销）
    if (m_rangeBox) {
        const QList<QAbstractButton *> btns = m_rangeBox->buttonList();
        for (QAbstractButton *b : btns)
            b->setChecked(false);
    }
    if (ret != 1)
        return;
    // complete == false 表示用户只点了起点就确定 → from/to 已同值，视为单日区间
    QDate outFrom = st->from;
    QDate outTo = st->to;
    if (outFrom > outTo)
        std::swap(outFrom, outTo);
    requestRange(outFrom, outTo);
}

void DiskUsagePage::onChartDateSelected(const QDate &d)
{
    // 点击数据点 = 想看这天涨在哪 → 自动切到「增长」视角
    //（原为切到「增长详情」标签。视角切换在这里是"意图直译"，
    //  否则用户点了图表只有下方表格变化，容易觉得"没反应"）
    m_view = ViewMode::Growth;
    syncViewBox();
    updateDayDetail(d);
}

void DiskUsagePage::onRangeProgress(quint64 id, int loaded, int total)
{
    if (id != m_rangeId)
        return;
    m_loadStatus->setText(tr("加载中 %1/%2 …").arg(loaded).arg(total));
    m_chart->setLoadProgress(loaded, total);
}

void DiskUsagePage::onRangeLoaded(quint64 id, const QList<DiskDayRecord> &records)
{
    if (id != m_rangeId)
        return;
    m_rangeId = 0;
    m_records = records;
    m_chart->setLoading(false);
    // 加载完成的收尾：把进度文案换成结果摘要。
    // 原实现在这里不清 m_loadStatus，导致「加载中 N/N …」永久残留
    // （只有开关监控/扫描完成/下钻才会覆盖它）——用户实测 4 次均复现。
    // 这里给出终态：有数据报天数，无数据明确说明，不留"进行中"的错觉。
    m_loadStatus->setText(records.isEmpty()
                              ? tr("该范围内暂无监控数据")
                              : tr("已加载 %1 天记录").arg(records.size()));
    loadRanking();     // 内部重置目录时间线并按当前范围重建图表序列
    updateSummary();
    if (!m_records.isEmpty()) {
        if (m_selected.isValid())
            updateDayDetail(m_selected);
        else
            updateDayDetail(m_records.last().date);
    } else {
        m_chart->setSelectedDate(QDate());
        updateAnalysis();
    }
}

void DiskUsagePage::rebuildSpikeSet()
{
    m_spikeSet.clear();
    const DiskHistoryIndex idx = m_store.loadIndex();
    for (const DiskDaySummary &s : idx.days) {
        if (s.spike)
            m_spikeSet.insert(s.date);
    }
    const qint64 minSpike = static_cast<qint64>(m_config.minAlertMB()) * kMB;
    for (int i = 1; i < m_records.size(); ++i) {
        const qint64 g = m_records.at(i).total - m_records.at(i - 1).total;
        if (g >= minSpike)
            m_spikeSet.insert(m_records.at(i).date);
    }
}

// ----------------------------------------------------------------------
// 概览
// ----------------------------------------------------------------------
void DiskUsagePage::updateOverview()
{
    qint64 capacity = 0, used = 0;
    DiskScanner::dfRoot(&capacity, &used);
    if (capacity > 0) {
        const double pct = double(used) * 100.0 / capacity;
        m_usageBar->setValue(static_cast<int>(used * 1000 / capacity));
        // 百分比**外置标签**而非 setFormat（缺陷 O2-1，V-16 像素级实测：
        // DTK6 的 DProgressBar 样式不绘制内嵌 format 文字，14→18px 均无效）。
        // 顺带好处：这里已是唯一写百分比的地方，语义色也一并落在它身上。
        const QString pctText = QString::number(pct, 'f', pct >= 100 ? 0 : 1)
                                + QStringLiteral("%");
        m_usagePct->setText(pctText);
        m_usedBig->setText(tr("已用 %1").arg(formatSize(used)));
        m_overviewInfo->setText(tr("总容量 %1 · 可用 %2")
                                    .arg(formatSize(capacity), formatSize(capacity - used)));
        // 使用率语义：不再用 QSS 的 level 属性切进度条 chunk 色，
        // 改为把语义落在「大数值」与「百分比」的文字色上（DPalette 角色，跟随主题）：
        //   <75% 正常 → TextTitle；≥75% 警示 → TextWarning（两档同色，见下）
        // 进度条本体是 DProgressBar（DTK 原生外观，阶段 3-7b 由 QProgressBar 换入），
        // chunk 色由 DTK 样式从调色板派生，本页不再有任何 QSS 接触。
        //
        // WCAG 1.4.1：颜色不得作为唯一载体 —— 百分比数字本身就写着数值，
        // 语义不会因主题色异常而失效。**两个标签同步染色**，
        // 避免「大数值变红、百分比还是黑的」这种自相矛盾。
        //
        // ⚠️ 危险档改用 `TextWarning`（V-23 修正，2026-10-03 调色板探针）：
        //   原来是 `TextLively`，但它在 DTK 默认主题下**就是中性黑**
        //   （不是降级环境的灰）→ ≥90% 的「已用」与百分比渲染成普通正文黑，
        //   危险语义完全失效。DTK 里 `TextLively` 的语义是「活跃式文本」，
        //   本就不承担危险含义。与 components.cpp 的 Semantic::Danger 取色一致。
        //
        // 现 Warning 与 Danger 同色，**75~90% 与 ≥90% 在视觉上不再区分** ——
        // 这是「复用 TextWarning」方案的已知代价（用户 2026-10-03 决策）。
        // 两档的语义差异目前由文案与图标承载，不再依赖颜色。
        // 若日后要恢复区分，应引入明确的危险色角色而非回头用 TextLively。
        //
        // 判据已抽到 PanshiUi::usageSemanticForPct()（阈值常量 kUsageWarnPct 唯一定义）：
        // 配色探针 `panshi-colorprobe` 复用同一判据，C7 的颜色断言因此自动跟随这里。
        const PanshiUi::Semantic usageSem = PanshiUi::usageSemanticForPct(pct);
        PanshiUi::applySemantic(m_usedBig, usageSem);
        // 百分比标签**始终走正常色（TextTitle）**，不用警示色：
        // 它是 18px 小字，DTK 的 TextWarning 在其上的对比度（亮 3.08 / 暗 3.42）
        // 低于 WCAG AA 正文 4.5:1 门槛；而 18px bold 仍不足 18.66px，连「大文本」
        // 3:1 都不够。把警示色留给 28px 的大数值（大文本档达标）与进度条，
        // 百分比靠数值本身 + 相邻橙色条/数值承载语义（WCAG 1.4.1 仍满足）。
        // C7b 配色探针已按此契约改为 pct 恒 normal。
        PanshiUi::applySemantic(m_usagePct, PanshiUi::Semantic::Normal);
    } else {
        m_usageBar->setValue(0);
        // 无盘符/读取失败：百分比标签回落为占位符，颜色一并归中性
        // （否则会残留上一次刷新的高亮语义色，与「已用 --」自相矛盾）。
        m_usagePct->setText(QStringLiteral("--"));
        m_usagePct->setForegroundRole(DPalette::TextTips);
        m_usedBig->setText(tr("已用 --"));
        m_usedBig->setForegroundRole(DPalette::TextTitle);
        m_overviewInfo->setText(tr("总容量 -- · 可用 --"));
    }

    // 监控状态行：运行状态已由徽章（m_serviceBadge）承载，
    // 这里只放采样参数 + 最近记录，不再用 ●/○ 字符重复表达状态
    QString status;
    if (m_config.enabled())
        status = tr("每 %1 分钟采样一次 · 历史保留 %2 天")
                     .arg(m_config.pollMinutes()).arg(m_config.retentionDays());
    else
        status = tr("开启后自动建立基线并持续监控");
    const DiskHistoryIndex idx = m_store.loadIndex();
    checkStep(tr("读取历史数据索引"));
    if (!idx.days.isEmpty()) {
        const DiskDaySummary &last = idx.days.last();
        status += tr(" · 最近记录 %1 · 该日增长 %2")
                      .arg(last.date.toString(QStringLiteral("yyyy-MM-dd")),
                           formatSize(last.growth));
    }
    m_monitorStatus->setText(status);

    refreshAlert();
}

// ----------------------------------------------------------------------
// 告警：横幅展示 + 手动关闭 + 历史回看
// ----------------------------------------------------------------------
QString DiskUsagePage::alertKey(const QString &date, const QString &dir)
{
    return date + QStringLiteral("|") + dir;
}

QJsonArray DiskUsagePage::loadAlertSpikes()
{
    QFile f(DiskHistoryStore::historyDir() + QStringLiteral("/alert.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return {};
    return doc.object().value(QStringLiteral("spikes")).toArray();
}

QList<DiskUsagePage::AlertEntry> DiskUsagePage::alertHistory() const
{
    QList<AlertEntry> out;
    const QJsonArray spikes = loadAlertSpikes();
    for (int i = spikes.size() - 1; i >= 0; --i) {   // 新 → 旧
        const QJsonObject o = spikes.at(i).toObject();
        AlertEntry e;
        e.date = o.value(QStringLiteral("date")).toString();
        e.dir = o.value(QStringLiteral("causeDir")).toString();
        e.growth = formatSize(
            static_cast<qint64>(o.value(QStringLiteral("growth")).toDouble(0)));
        out.append(e);
    }
    return out;
}

void DiskUsagePage::refreshAlert()
{
    if (!m_alertBanner)
        return;
    const QJsonArray spikes = loadAlertSpikes();
    for (int i = spikes.size() - 1; i >= 0; --i) {
        const QJsonObject o = spikes.at(i).toObject();
        const QString date = o.value(QStringLiteral("date")).toString();
        const QString dir = o.value(QStringLiteral("causeDir")).toString();
        if (m_dismissedAlerts.contains(alertKey(date, dir)))
            continue;    // 用户关过 → 不再自动弹出（历史里仍可见）
        // 文案不带「⚠」符号 —— 语义由 Banner 图标承载（WCAG 1.4.1）
        m_alertBanner->setSemantic(PanshiUi::Semantic::Danger);
        m_alertBanner->setText(tr("%1 · %2 增长 %3")
                                   .arg(date, dir,
                                        formatSize(static_cast<qint64>(
                                            o.value(QStringLiteral("growth")).toDouble(0)))));
        m_alertBanner->setClosable(true);
        m_alertBanner->show();
        m_currentAlertKey = alertKey(date, dir);
        updateAlertHistoryBtn();
        return;
    }
    // 没有"未关闭"的告警 → 收起。
    // ⚠️ 走 setText(空) 的自动隐藏路径，不发 dismissed —— 否则自动收起
    //    会被误记成"用户关掉的"，历史里的"新增"计数就乱了。
    m_currentAlertKey.clear();
    m_alertBanner->setText(QString());
    updateAlertHistoryBtn();
}

void DiskUsagePage::updateAlertHistoryBtn()
{
    if (!m_alertHistoryBtn)
        return;
    const int total = loadAlertSpikes().size();
    const int fresh = qMax(0, total - m_alertSeenCount);
    m_alertHistoryBtn->setText(fresh > 0 ? tr("告警历史（%1 条新增）").arg(fresh)
                                          : tr("告警历史（%1）").arg(total));
    m_alertHistoryBtn->setEnabled(total > 0);
}

void DiskUsagePage::onDismissAlert()
{
    if (m_currentAlertKey.isEmpty())
        return;
    if (!m_dismissedAlerts.contains(m_currentAlertKey))
        m_dismissedAlerts.append(m_currentAlertKey);
    // 上限保护：与 spikes（≤200）同量级即可，超出丢弃最旧的
    while (m_dismissedAlerts.size() > 200)
        m_dismissedAlerts.removeFirst();
    saveAlertUiState();
    m_currentAlertKey.clear();
    showStatus(tr("告警已关闭，可随时在「告警历史」中查看"));
    updateAlertHistoryBtn();
}

void DiskUsagePage::loadAlertUiState()
{
    QFile f(DiskHistoryStore::historyDir() + QStringLiteral("/alert-ui-state.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject o = doc.object();
    m_dismissedAlerts.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("dismissed")).toArray())
        m_dismissedAlerts.append(v.toString());
    m_alertSeenCount = static_cast<int>(o.value(QStringLiteral("seen")).toDouble(0));
}

void DiskUsagePage::saveAlertUiState() const
{
    QJsonArray arr;
    for (const QString &k : m_dismissedAlerts)
        arr.append(k);
    QJsonObject o;
    o.insert(QStringLiteral("dismissed"), arr);
    o.insert(QStringLiteral("seen"), m_alertSeenCount);
    QFile f(DiskHistoryStore::historyDir() + QStringLiteral("/alert-ui-state.json"));
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void DiskUsagePage::onOpenAlertHistory()
{
    const QList<AlertEntry> entries = alertHistory();

    DDialog dlg(this);
    PanshiUi::disableDialogTransparency(&dlg);
    // 降为 WindowModal：历史弹窗开着时主窗口仍可拖动（同设置窗口口径）
    PanshiUi::makeDialogNonBlockingWindowModal(&dlg);
    dlg.setTitle(tr("告警历史"));
    dlg.setIcon(DIconTheme::findQIcon(QStringLiteral("dialog-warning")));

    auto *sec = new PanshiUi::PanshiSection(
        tr("历史告警"),
        entries.isEmpty() ? tr("暂无告警记录")
                          : tr("按时间倒序；手动关闭过的告警仍保留在此"));

    QStringList lines;
    lines.reserve(entries.size());
    for (const AlertEntry &e : entries)
        lines.append(tr("%1 · %2 · 增长 %3").arg(e.date, e.dir, e.growth));

    auto *model = new QStringListModel(sec);
    model->setStringList(lines);
    auto *view = new DListView(sec);
    view->setModel(model);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setMinimumHeight(240);
    sec->content()->addWidget(view);

    dlg.addContent(sec);
    dlg.addButton(tr("关闭"), false, DDialog::ButtonNormal);
    dlg.addButton(tr("清空历史"), false, DDialog::ButtonWarning);

    // 打开即视为已读：清掉按钮上的"新增"角标
    m_alertSeenCount = entries.size();
    saveAlertUiState();
    updateAlertHistoryBtn();

    if (dlg.exec() == 1) {   // 1 = 「清空历史」
        if (Ui::confirmDialog(this, tr("清空告警历史"),
                              tr("将删除全部告警记录，且不可恢复。确定继续？"),
                              tr("清空"), true)) {
            const QString dir = DiskHistoryStore::historyDir();
            QFile::remove(dir + QStringLiteral("/alert.json"));
            QFile::remove(dir + QStringLiteral("/alert-ui-state.json"));
            m_dismissedAlerts.clear();
            m_alertSeenCount = 0;
            m_currentAlertKey.clear();
            refreshAlert();
        }
    }
}

// ----------------------------------------------------------------------
// 启动检查流程（雷达面板 + 步骤耗时）
// ----------------------------------------------------------------------
void DiskUsagePage::beginCheck()
{
    m_checking = true;
    m_checkSteps = 0;
    m_checkTotal = 4;
    m_lastCheckMs = 0;
    m_checkTimer.start();
    m_chart->setChecking(true, tr("正在检查系统信息…"));
    // 兜底：3 秒内未全部完成则强制结束面板
    QTimer::singleShot(3000, this, [this] {
        if (m_checking) {
            m_checking = false;
            m_chart->finishCheck(m_checkTimer.elapsed());
        }
    });
}

void DiskUsagePage::checkStep(const QString &name)
{
    if (!m_checking)
        return;
    const qint64 now = m_checkTimer.elapsed();
    const qint64 stepMs = now - m_lastCheckMs;
    m_lastCheckMs = now;
    ++m_checkSteps;
    m_chart->addCheckStep(name, true, stepMs);
    if (m_checkSteps >= m_checkTotal) {
        m_checking = false;
        m_chart->finishCheck(now);
    }
}

// ----------------------------------------------------------------------
// 监控服务状态
// ----------------------------------------------------------------------
void DiskUsagePage::updateServiceStatus()
{
    if (m_svcProc)
        return;   // 已有检测进行中
    if (!m_serviceBadge)
        return;
    m_serviceBadge->setText(tr("正在检测服务状态…"));
    m_serviceBadge->setSemantic(PanshiUi::Semantic::Muted);
    m_svcProc = new QProcess(this);
    connect(m_svcProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                const QString out = QString::fromUtf8(m_svcProc->readAllStandardOutput()).trimmed();
                m_svcProc->deleteLater();
                m_svcProc = nullptr;
                applyServiceStatus(out);
            });
    m_svcProc->start(QStringLiteral("systemctl"),
                     {QStringLiteral("--user"), QStringLiteral("is-active"),
                      QStringLiteral("panshi-monitor.timer")});
}

void DiskUsagePage::applyServiceStatus(const QString &state)
{
    if (!m_serviceBadge)
        return;
    // 空输出 = systemctl 压根没跑起来。典型成因是**会话总线不可用**
    // （例如用 sudo/pkexec 启动本应用，或不在桌面会话里）→ 此时所有
    // enable/start 都会失败，必须先说清楚，否则用户只会看到「未开启」而找不到原因。
    if (state.isEmpty()) {
        m_serviceBadge->setText(tr("无法连接 systemd 用户会话"));
        m_serviceBadge->setSemantic(PanshiUi::Semantic::Danger);
        if (m_serviceStartBtn)
            m_serviceStartBtn->hide();
        return;
    }
    const bool enabled = m_config.enabled();
    if (state == QLatin1String("active")) {
        m_serviceBadge->setText(tr("磁盘监控运行中"));
        m_serviceBadge->setSemantic(PanshiUi::Semantic::Success);
        if (m_serviceStartBtn)
            m_serviceStartBtn->hide();
    } else if (enabled) {
        // 配置已开启但后台服务没跑起来：给出人话提示 + 一键修复入口
        m_serviceBadge->setText(tr("磁盘监控未启动，无法自动记录"));
        m_serviceBadge->setSemantic(PanshiUi::Semantic::Danger);
        if (m_serviceStartBtn)
            m_serviceStartBtn->show();
    } else {
        m_serviceBadge->setText(tr("磁盘监控未开启"));
        m_serviceBadge->setSemantic(PanshiUi::Semantic::Muted);
        if (m_serviceStartBtn)
            m_serviceStartBtn->hide();
    }
}

void DiskUsagePage::onStartMonitorService()
{
    if (m_serviceStartBtn)
        m_serviceStartBtn->hide();
    m_serviceBadge->setText(tr("正在启动磁盘监控…"));
    m_serviceBadge->setSemantic(PanshiUi::Semantic::Muted);

    // 监控组件（systemd 单元）未安装时给出明确指引，避免“启动失败”让人困惑
    if (!monitorUnitInstalled()) {
        m_serviceBadge->setText(tr("监控组件未安装：请先安装应用（sudo ./install.sh）"));
        m_serviceBadge->setSemantic(PanshiUi::Semantic::Danger);
        if (m_serviceStartBtn)
            m_serviceStartBtn->show();
        return;
    }

    // 失败原因直接取自 systemctl 的 stderr：单元没被识别 / 总线不可用 / 语法错误
    // 三类成因的修法完全不同，不能再统一说成「重启系统后重试」。
    QString err;
    if (setMonitorService(true, &err)) {
        m_config.setEnabled(true);
        if (m_monitorSwitch) {
            QSignalBlocker b(m_monitorSwitch);
            m_monitorSwitch->setChecked(true);
        }
        // 不等 timer 的下一个整点：立刻跑一轮，把今日基线建起来
        runSystemctlUser({QStringLiteral("start"), QStringLiteral("--no-block"),
                          QStringLiteral("panshi-monitor.service")}, 10000);
        showStatus(tr("监控已开启，正在建立基线（由后台服务采样）"));
        QTimer::singleShot(1500, this, [this] { updateServiceStatus(); });
        return;
    }

    const QString reason = briefError(err);
    const QString text = tr("启动失败：%1").arg(reason.isEmpty() ? tr("未知错误") : reason);
    m_serviceBadge->setText(text);
    m_serviceBadge->setSemantic(PanshiUi::Semantic::Danger);
    showStatus(text);
    if (m_serviceStartBtn)
        m_serviceStartBtn->show();
}

// ----------------------------------------------------------------------
// 汇总 / 详情 / 排行
// ----------------------------------------------------------------------
void DiskUsagePage::updateSummary()
{
    const DiskRangeStats st = DiskHistoryStore::computeRangeStats(m_records);
    const bool hasData = !m_records.isEmpty();

    // ---- 区间陈述：给下面的数字补上主语（哪一段 · 多长 · 多少条）----
    m_sumRangeLabel->setText(
        (m_from.isValid() && m_to.isValid())
            ? tr("%1 ~ %2 · %3 天 · %4 条记录")
                  .arg(m_from.toString(QStringLiteral("yyyy-MM-dd")),
                       m_to.toString(QStringLiteral("yyyy-MM-dd")))
                  .arg(m_from.daysTo(m_to) + 1)
                  .arg(m_records.size())
            : QString());

    // ---- 主角：净增长 ----
    m_sumNet->setText(hasData ? formatSize(st.netGrowth) : QStringLiteral("-"));
    // 无数据时不用强调色——Accent 会把「没有数据」渲染成一个结论
    PanshiUi::applySemantic(m_sumNet,
                            hasData ? PanshiUi::Semantic::Accent : PanshiUi::Semantic::Muted);
    // 副说明：起始 → 结束。让「涨了多少」有参照系，否则孤立的大数字没有落点。
    m_sumBeginEnd->setText(hasData ? tr("区间起始 %1 → 结束 %2")
                                        .arg(formatSize(st.beginUsed), formatSize(st.endUsed))
                                   : tr("所选范围内暂无监控数据"));

    // ---- 次要指标：统一单行，日期退为备注 ----
    m_sumDaily->setText(hasData ? formatSize(st.dailyAvg) : QStringLiteral("-"));
    const bool hasMax = hasData && st.maxDay.isValid();
    m_sumMax->setText(hasMax ? formatSize(st.maxDayGrowth) : QStringLiteral("-"));
    PanshiUi::applySemantic(m_sumMax, hasMax ? PanshiUi::Semantic::Warning
                                             : PanshiUi::Semantic::Muted);
    // 备注为空时必须隐藏（hide 而非留空文本）—— 否则该行仍占一列宽度，
    // 与上方没有备注的行右边缘对不齐。
    m_sumMaxDay->setVisible(hasMax);
    m_sumMaxDay->setText(hasMax ? st.maxDay.toString(QStringLiteral("yyyy-MM-dd")) : QString());
    m_sumTrend->setText(st.trendStart.isValid()
                            ? st.trendStart.toString(QStringLiteral("yyyy-MM-dd"))
                            : tr("无连续增长"));

    // ---- 容量预测（英雄区右栏）----
    // 三段式与左栏同构：标签 → 主体 → 备注。原整句文案「按当前增速约 N 天后写满
    // （约 YYYY-MM-DD）」塞进单行 Banner，日期被降级进括号里；拆成
    // 「约 107 天」+「按当前增速写满 · 2027-01-25」后，落点是同一个具体日期。
    if (hasData && st.etaDays > 0) {
        m_fcValue->setText(tr("约 %1 天后写满").arg(st.etaDays));
        // 主体是「剩多少时间」，越少越该被看见 → Warning
        m_fcSemantic = PanshiUi::Semantic::Warning;
        m_fcNote->setText(tr("按当前增速 · 预计 %1")
                              .arg(QDate::currentDate().addDays(st.etaDays)
                                       .toString(QStringLiteral("yyyy-MM-dd"))));
    } else if (hasData) {
        m_fcValue->setText(tr("暂无风险"));
        m_fcSemantic = PanshiUi::Semantic::Accent;
        m_fcNote->setText(tr("增长已趋稳，短期不会写满"));
    } else {
        m_fcValue->setText(QStringLiteral("-"));
        m_fcSemantic = PanshiUi::Semantic::Muted;
        m_fcNote->setText(tr("暂无数据，无法预测"));
    }
    // 颜色走 applyExactColor 登记簿（切主题自动刷新），图标是快照需重查
    PanshiUi::applySemantic(m_fcValue, m_fcSemantic);
    updateForecastIcon();
    updateHeroLayout();
    // 磁盘分析区：构成 / 增长 / 排行 三视角共用一张表（整合后唯一渲染入口）
    updateAnalysis();
    // 其它分区：df 数据即时刷新（全盘视角，不依赖扫描记录）
    rebuildOtherPartitions();
}

void DiskUsagePage::updateDayDetail(const QDate &d)
{
    // 只负责"选中哪一天"，渲染统一交给 updateAnalysis()：
    // 三个视角共用一张表，日期只会影响「增长」视角的取数。
    m_selected = d;
    m_chart->setSelectedDate(d);
    updateAnalysis();
}

void DiskUsagePage::loadRanking()
{
    // 背景刷新（历史目录变化 / 监控采样 / 扫描完成）都会走到这里。
    //
    // 🚨 旧实现无条件"回到根"：清空下钻栈 + 清空时间线目录。后果是用户正看着
    //    /var/log 时后台一刷新就被弹回系统盘 —— 浏览位置被静默丢弃，且没有任何
    //    提示，用户只会觉得"这个软件老自己跳"。
    //
    //    但实际上**历史数据变化并不会让下钻视图失效**：分析表在下钻态显示的
    //    是当前目录的子项，来自**按需 du**（m_rankItems），不是历史记录。
    //    所以正确的语义是"根态下刷新顶层子项；下钻态原样保留"，
    //    真正的回根只由「返回系统盘 / 面包屑根 / 切换时间范围」显式触发。
    if (m_rankCurrentPath.isEmpty()) {
        DiskDayRecord latest;
        if (!m_records.isEmpty()) {
            latest = m_records.last();
        } else {
            const DiskHistoryIndex idx = m_store.loadIndex();
            if (!idx.days.isEmpty()) {
                DiskDayRecord r;
                if (m_store.loadDay(idx.days.last().date, &r))
                    latest = r;
            }
        }
        m_rankItems = latest.topLevel;
        std::sort(m_rankItems.begin(), m_rankItems.end(),
                  [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    }
    updateTimelineSeries();
    updateAnalysis();
}
// ----------------------------------------------------------------------
// 时间线序列：系统盘占用 / 指定目录占用
// ----------------------------------------------------------------------
void DiskUsagePage::updateTimelineSeries()
{
    QList<DiskDayRecord> series;
    if (m_timelineDir.isEmpty()) {
        series = m_records;   // 系统盘占用
    } else {
        // 目录模式：提取该目录逐日大小（有记录才收录）
        for (const DiskDayRecord &r : m_records) {
            const qint64 sz = dirSizeInRecord(r, m_timelineDir);
            if (sz >= 0) {
                DiskDayRecord dr = r;
                dr.total = sz;
                series.append(dr);
            }
        }
    }

    m_chart->setLoading(false);
    if (series.isEmpty()) {
        m_chart->setRecords({});
        m_chart->setSpikeDays({});
        // 补齐花括号：原实现缺花括号，使 `m_backTimelineBtn->hide()` 成为
        // if 语句体的一部分（缩进误导读者以为它在 if 之外）。行为虽恰好正确，
        // 但「看起来在 if 内、实际也在 if 内」这种巧合一旦后续有人加语句就会出错。
        m_timelineHint->setText(
            m_timelineDir.isEmpty()
                ? tr("当前显示：系统盘占用")
                : tr("目录“%1”暂无逐日数据，请先开启监控或在该目录下钻").arg(m_timelineDir));
        return;
    }

    if (m_timelineDir.isEmpty()) {
        m_timelineHint->setText(tr("当前显示：系统盘占用变化"));
        rebuildSpikeSet();
        m_chart->setSpikeDays(m_spikeSet);
    } else {
        m_timelineHint->setText(tr("当前显示：目录 %1 的占用变化").arg(m_timelineDir));
        m_chart->setSpikeDays({});   // 目录模式不标系统盘飙升日
    }
    m_chart->setRecords(series);
}

// ----------------------------------------------------------------------
// 设置控件连接
// ----------------------------------------------------------------------
void DiskUsagePage::connectSettingsWidgets()
{
    connect(m_fixedThresholdSpin, QOverload<int>::of(&DSpinBox::valueChanged),
            this, [this](int v) { m_config.setRawValue(QStringLiteral("monitor.fixedThresholdMB"), v); });
    connect(m_multiplierSpin, QOverload<double>::of(&DDoubleSpinBox::valueChanged),
            this, [this](double v) { m_config.setRawValue(QStringLiteral("monitor.baselineMultiplier"), v); });
    connect(m_cooldownSpin, QOverload<int>::of(&DSpinBox::valueChanged),
            this, [this](int v) { m_config.setRawValue(QStringLiteral("monitor.cooldownHours"), v); });
    connect(m_retentionSpin, QOverload<int>::of(&DSpinBox::valueChanged),
            this, [this](int v) {
                m_config.setRawValue(QStringLiteral("monitor.retentionDays"), v);
                applyConfigToStore();
            });
    connect(m_budgetBox, QOverload<int>::of(&DComboBox::currentIndexChanged),
            this, [this](int i) {
                const int mb = (i == 2) ? 1024 : (i == 1) ? 512 : 256;
                m_config.setRawValue(QStringLiteral("monitor.storageBudgetMB"), mb);
                applyConfigToStore();
                m_storageUsed->setText(tr("%1 / %2").arg(
                    formatSize(m_store.storageBytes()),
                    formatSize(static_cast<qint64>(mb) * kMB)));
            });
}

void DiskUsagePage::applyConfigToStore()
{
    m_store.setRetentionDays(m_config.retentionDays());
    m_store.setBudgetBytes(static_cast<qint64>(m_config.storageBudgetMB()) * kMB);
    m_store.setSamplesRetentionDays(m_config.samplesRetentionDays());
    m_store.setMemoryCacheDays(m_config.memoryCacheDays());
}

// ----------------------------------------------------------------------
// 设置交互
// ----------------------------------------------------------------------
void DiskUsagePage::updateMonitorSettingsUi()
{
    const bool on = m_config.enabled();
    {
        QSignalBlocker b(m_monitorSwitch);   // 程序化赋值不触发 toggled（避免误启停 systemd）
        m_monitorSwitch->setChecked(on);
    }
    m_alertModeBox->setCurrentIndex(m_config.alertMode() == QStringLiteral("fixed") ? 1 : 0);
    m_fixedThresholdSpin->setValue(m_config.fixedThresholdMB());
    m_fixedThresholdSpin->setEnabled(m_config.alertMode() == QStringLiteral("fixed"));
    m_multiplierSpin->setValue(m_config.baselineMultiplier());
    m_multiplierSpin->setEnabled(m_config.alertMode() != QStringLiteral("fixed"));
    m_cooldownSpin->setValue(m_config.cooldownHours());
    m_retentionSpin->setValue(m_config.retentionDays());
    const int budgetMB = m_config.storageBudgetMB();
    m_budgetBox->setCurrentIndex(budgetMB >= 1024 ? 2 : (budgetMB >= 512 ? 1 : 0));
    m_storageUsed->setText(tr("%1 / %2").arg(
        formatSize(m_store.storageBytes()),
        formatSize(static_cast<qint64>(budgetMB) * kMB)));
    m_excludeModel->setStringList(m_config.excludes());
}

void DiskUsagePage::onMonitorToggled(bool on)
{
    // 顺序有讲究：**先让服务真的起来，再落配置**。
    // 原实现是反的（先写 enabled=true 再启服务、且忽略返回值）→ 故障恰恰是
    // 「服务启不起来」，界面却显示已开启，配置与服务状态长期不一致且无声无息。
    QString err;
    if (!setMonitorService(on, &err)) {
        m_config.setEnabled(!on);          // 回滚配置，与真实状态对齐
        if (m_monitorSwitch) {
            QSignalBlocker b(m_monitorSwitch);
            m_monitorSwitch->setChecked(!on);
        }
        const QString reason = briefError(err);
        showStatus(tr("监控服务%1失败：%2").arg(on ? tr("启动") : tr("停止"),
                                              reason.isEmpty() ? tr("未知错误") : reason));
        updateServiceStatus();
        return;
    }
    m_config.setEnabled(on);
    if (on) {
        runSystemctlUser({QStringLiteral("start"), QStringLiteral("--no-block"),
                          QStringLiteral("panshi-monitor.service")}, 10000);
        showStatus(tr("监控已开启，正在建立基线（由后台服务采样）"));
        refresh();
    } else {
        showStatus(tr("监控已关闭，历史数据保留"));
        updateOverview();
        updateServiceStatus();
    }
}

void DiskUsagePage::onAlertModeChanged(int index)
{
    m_config.setRawValue(QStringLiteral("monitor.alertMode"),
                         index == 1 ? QStringLiteral("fixed") : QStringLiteral("baseline"));
    m_fixedThresholdSpin->setEnabled(index == 1);
    m_multiplierSpin->setEnabled(index == 0);
}

void DiskUsagePage::onAddExclude()
{
    const QString p = m_excludeEdit->text().trimmed();
    if (p.isEmpty())
        return;
    QStringList ex = m_config.excludes();
    for (const QString &e : ex) {
        if (e == p)
            return;
    }
    ex.append(p);
    m_config.setRawValue(QStringLiteral("monitor.excludes"), ex);
    m_excludeModel->setStringList(ex);
    m_excludeEdit->clear();
}

void DiskUsagePage::onRemoveExclude()
{
    // 当前行取自视图索引（原 QListWidget::currentRow() 的等价物）。
    // 无选中项时 currentIndex() 无效 → row() = -1，与原实现「row < 0 直接返回」一致。
    const int row = m_excludeList->currentIndex().row();
    if (row < 0)
        return;
    QStringList ex = m_config.excludes();
    if (row < ex.size()) {
        ex.removeAt(row);
        m_config.setRawValue(QStringLiteral("monitor.excludes"), ex);
    }
    // 以配置为单一数据源重建列表（原 takeItem 只动视图，二者会同源不同步：
    // 若 row 越界则模型未变而视图未删——此处统一改为整表重建，消除该分叉）
    m_excludeModel->setStringList(ex);
}

void DiskUsagePage::onClearHistory()
{
    DDialog dlg(this);
    PanshiUi::disableDialogTransparency(&dlg);   // 关闭 DTK 弹窗默认的模糊/透明效果
    // 降为 WindowModal：确认框开着时主窗口仍可拖动
    // （根因见 components.h::makeDialogNonBlockingWindowModal）
    PanshiUi::makeDialogNonBlockingWindowModal(&dlg);
    dlg.setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    dlg.setTitle(tr("清除全部监控数据"));
    dlg.setMessage(tr("将删除所有历史占用记录与告警，且不可恢复。确定继续？"));
    dlg.addButton(tr("取消"), false, DDialog::ButtonNormal);
    dlg.addButton(tr("清除"), false, DDialog::ButtonWarning);
    if (dlg.exec() != 1)
        return;
    const QDate today = QDate::currentDate();
    m_store.removeRange(QDate(2000, 1, 1), today.addDays(1));
    const QString dir = DiskHistoryStore::historyDir();
    QFile::remove(dir + QStringLiteral("/alert.json"));
    QFile::remove(dir + QStringLiteral("/alert-ui-state.json"));
    m_records.clear();
    m_selected = QDate();
    m_dismissedAlerts.clear();
    m_alertSeenCount = 0;
    m_currentAlertKey.clear();
    refreshAlert();
    refresh();
}

// ----------------------------------------------------------------------
// 立即扫描 / 下钻
// ----------------------------------------------------------------------
void DiskUsagePage::onScanNow()
{
    if (m_scanner.isBusy())
        return;
    m_scanner.setAsRoot(true);
    m_scanner.setRootPolicy(DiskScanner::RootPolicy::AllowPrompt); // 手动扫描允许 pkexec 弹窗
    // 图表区显示扫描动态面板（雷达动画 + 实时路径 + 流动进度）
    m_chart->setScanning(true, tr("正在扫描系统盘目录…（可稍候，完成后自动刷新）"));
    m_scanner.startTopLevelScan();
}

void DiskUsagePage::onScanProgress(int done, const QString &cur)
{
    m_chart->setScanProgress(done, cur);
}

void DiskUsagePage::onScanFinished(bool ok, const QString &err)
{
    m_chart->setScanning(false);
    if (ok) {
        showStatus(tr("扫描完成，今日记录已更新"));
        // 刷新当前范围以显示最新数据
        if (m_from.isValid())
            requestRange(m_from, m_to);
    } else {
        showStatus(err.isEmpty() ? tr("扫描失败") : err, false);
    }
}

void DiskUsagePage::onDrillFinished(const QString &dir, const QList<DiskItem> &items)
{
    // 下钻一帧进栈：保存进入前的路径与子项，供「返回上级 / 面包屑」逐级回退
    m_rankFrames.append(qMakePair(m_rankCurrentPath, m_rankItems));
    m_rankCurrentPath = dir;
    m_rankItems = items;
    std::sort(m_rankItems.begin(), m_rankItems.end(),
              [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    m_timelineDir = dir;      // 时间线跟随进入的目录
    updateTimelineSeries();
    updateAnalysis();         // 表格按当前视角重渲染（构成 / 排行取新子项）
}

void DiskUsagePage::onBackToSystemTimeline()
{
    // 一键回到根：时间线恢复系统盘 + 列表回到顶层 + 清空下钻栈
    m_timelineDir.clear();
    m_rankCurrentPath.clear();
    m_rankFrames.clear();
    m_rankItems = m_records.isEmpty() ? QList<DiskItem>() : m_records.last().topLevel;
    updateTimelineSeries();
    updateAnalysis();
}

// ----------------------------------------------------------------------
// 杂项
// ----------------------------------------------------------------------
void DiskUsagePage::openInFileManager(const QString &path)
{
    if (path.isEmpty())
        return;
    QProcess::startDetached(QStringLiteral("gio"), {QStringLiteral("open"), path});
}

bool DiskUsagePage::setMonitorService(bool on, QString *errorOut)
{
    // 开启前先让 systemd 重新扫描单元目录 —— 装包时做不到这件事
    // （详见 ensureMonitorUnitLoaded() 注释），这是唯一的补救时机。
    if (on && !ensureMonitorUnitLoaded()) {
        if (errorOut)
            *errorOut = tr("systemd 未识别监控单元（panshi-monitor.timer）");
        return false;
    }
    const SystemctlResult r = runSystemctlUser(
        {on ? QStringLiteral("enable") : QStringLiteral("disable"),
         QStringLiteral("--now"),
         QStringLiteral("panshi-monitor.timer")});
    if (r.ok)
        return true;
    if (!on) {
        // 关闭分支：systemd 压根不认识它 ⇔ 本来就没启用，用户意图已达成，
        // 不该因为「单元不存在」把开关弹回去。
        const SystemctlResult en = runSystemctlUser(
            {QStringLiteral("is-enabled"), QStringLiteral("panshi-monitor.timer")});
        if (!en.ok)
            return true;
    }
    if (errorOut)
        *errorOut = r.output.isEmpty() ? tr("systemctl --user 执行失败") : r.output;
    return false;
}

void DiskUsagePage::onHistoryDirChanged(const QString &path)
{
    Q_UNUSED(path);
    m_refreshDebounce->start();
}

void DiskUsagePage::onThemeChanged()
{
    if (m_chart)
        m_chart->update();
    // 图标是**快照**（QLabel::setPixmap 之后 QIcon 换主题也不会影响已画的图，
    // 见 common.h::iconBadge 注释）→ 切主题必须重查一次，否则容量预测图标
    // 会一直停在浅色主题那一版。
    updateForecastIcon();
    updateOverview();
    // ⚠️ 自绘组件在调色板替换后未必自动重绘 —— 显式刷一次整页。
    // 2026-10-03 用户反馈「切换暗色时有时不会变」：本页的自绘卡片
    // （PanshiPanel/StateCard/Badge/InlineBanner 全在 paintEvent 里
    // 实时取 appPalette()，取色逻辑本身是对的）**没有收到重绘通知**，
    // 于是仍显示旧主题的像素。整页 update() 让它们重走 paintEvent。
    update();
}

/** 按 m_fcSemantic 重设容量预测图标（无风险态用空 pixmap 留白） */
void DiskUsagePage::updateForecastIcon()
{
    if (!m_fcIcon)
        return;
    const QSize isz(PanshiUi::Size::IconSmall, PanshiUi::Size::IconSmall);
    if (m_fcSemantic == PanshiUi::Semantic::Warning) {
        m_fcIcon->setPixmap(Ui::themedIcon(QStringLiteral("warning"),
                                           QStyle::SP_MessageBoxWarning)
                                .pixmap(isz));
    } else if (m_fcSemantic == PanshiUi::Semantic::Accent) {
        m_fcIcon->setPixmap(Ui::themedIcon(QStringLiteral("dialog-information"),
                                           QStyle::SP_MessageBoxInformation)
                                .pixmap(isz));
    } else {
        // 无数据态不画图标，但保留占位尺寸（否则标签行会左移，切换时标题跳动）
        m_fcIcon->setPixmap(QPixmap(isz));
    }
}

void DiskUsagePage::onPaletteChanged()
{
    // applicationPaletteChanged：新调色板**已就位**后才发。
    //
    // 为什么需要它（不能只靠 onThemeChanged）：
    //   main.cpp 已实测记录该时序差 —— `themeTypeChanged` 发出时 DTK 的调色板
    //   替换**尚未完成**，此刻重建/重绘会把界面钉成「半新半旧」。
    //   本页的 onThemeChanged 挂在 themeTypeChanged 上（在浅色时是安全的），
    //   但真正需要重绘的时刻是调色板就位之后 → 补挂 applicationPaletteChanged。
    //
    // 两个信号分工：themeTypeChanged 用于「立刻知道要准备」（如重建图标名），
    // applicationPaletteChanged 用于「现在可以安全重绘」。
    onThemeChanged();
}

void DiskUsagePage::showStatus(const QString &text, bool ok)
{
    Q_UNUSED(ok);
    m_loadStatus->setText(text);
}
