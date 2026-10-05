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
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace Panshi;

namespace {
constexpr qint64 kMB = 1024LL * 1024;

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

void DiskUsagePage::buildUi()
{
    auto *scroll = Ui::makeScrollArea(this);
    m_contentScroll = scroll;
    auto *content = new QWidget;
    scroll->setWidget(content);
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
    buildTimelineCard(lay);
    buildAnalysisTabs(lay);
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

    m_monitorStatus = Ui::rowDesc(QString());
    cl1->addWidget(m_monitorStatus);

    // 监控服务运行状态标识 + 设置入口
    // 徽章语义由 PanshiBadge 承担（原按 objectName 切 panshiTagXxx 的 QSS）
    auto *svcRow = new QHBoxLayout;
    svcRow->setSpacing(10);
    m_serviceBadge = new PanshiUi::PanshiBadge(tr("正在检测服务状态…"),
                                               PanshiUi::Semantic::Muted, c1);
    svcRow->addWidget(m_serviceBadge);
    m_serviceStartBtn = Ui::button(tr("立即启动"));
    m_serviceStartBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("system-run")));
    m_serviceStartBtn->hide();
    connect(m_serviceStartBtn, &DPushButton::clicked, this, &DiskUsagePage::onStartMonitorService);
    svcRow->addWidget(m_serviceStartBtn);
    svcRow->addStretch();
    auto *svcBtn = Ui::button(tr("监控设置"));
    svcBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("preferences-system")));
    connect(svcBtn, &DPushButton::clicked, this, &DiskUsagePage::onOpenSettings);
    svcRow->addWidget(svcBtn);
    cl1->addLayout(svcRow);

    // 告警：语义 Banner（图标 + 柔和底 + 中性正文），替代原 panshiWarnBox 自绘框
    m_alertBanner = new PanshiUi::PanshiInlineBanner(c1);
    m_alertBanner->hide();
    cl1->addWidget(m_alertBanner);
    lay->addWidget(c1);
}

// ---------- ② 时间线 ----------
void DiskUsagePage::buildTimelineCard(QVBoxLayout *lay)
{
    auto *c2 = new PanshiUi::PanshiSection(tr("时间线"),
                                           tr("折线展示每日磁盘占用；点击数据点查看该日增长详情"));
    auto *tl = c2->content();

    // 工具栏：图例 + 立即扫描（进度条同排）；图例色取自 DPalette，自动跟随主题
    // 「立即扫描」作为该组的组级操作挂在标题行右侧（与 policypage 的保存按钮一致）
    auto *scanBtn = Ui::button(tr("立即扫描"));
    connect(scanBtn, &DPushButton::clicked, this, &DiskUsagePage::onScanNow);
    c2->addHeaderWidget(scanBtn);

    auto *toolRow = new QHBoxLayout;
    toolRow->setSpacing(18);
    toolRow->addWidget(Ui::legendDot(PanshiUi::Semantic::Accent, tr("正常增长")));
    toolRow->addWidget(Ui::legendDot(PanshiUi::Semantic::Danger, tr("异常增长")));
    toolRow->addWidget(Ui::legendDot(PanshiUi::Semantic::Warning, tr("选中日")));
    toolRow->addWidget(Ui::legendLine(tr("趋势")));
    toolRow->addStretch();
    tl->addLayout(toolRow);

    // 范围选择行：预设 + 自定义（点击弹出日历选择）
    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(8);
    const int presets[] = {15, 30, 90, 180, 365};
    for (int days : presets) {
        auto *b = Ui::button(tr("%1 天").arg(days));
        connect(b, &DPushButton::clicked, this, [this, days] { onPreset(days); });
        presetRow->addWidget(b);
    }
    presetRow->addStretch();
    auto *customBtn = Ui::button(tr("自定义"));
    customBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("appointment-soon")));
    connect(customBtn, &DPushButton::clicked, this, &DiskUsagePage::onOpenCustomRange);
    presetRow->addWidget(customBtn);
    tl->addLayout(presetRow);

    // 当前显示范围提示（系统盘 / 指定目录）+ 返回入口
    // 提示属说明性文字，用中性 Muted（原 panshiTipText 会给它上强调色，语义过重）
    auto *hintRow = new QHBoxLayout;
    hintRow->setSpacing(10);
    m_timelineHint = Ui::rowDesc(QString());
    hintRow->addWidget(m_timelineHint, 1);
    m_backTimelineBtn = Ui::button(tr("返回系统盘"));
    m_backTimelineBtn->setIcon(DIconTheme::findQIcon(QStringLiteral("view-refresh")));
    m_backTimelineBtn->hide();
    connect(m_backTimelineBtn, &DPushButton::clicked, this, &DiskUsagePage::onBackToSystemTimeline);
    hintRow->addWidget(m_backTimelineBtn);
    tl->addLayout(hintRow);

    m_chart = new UsageChart(c2);
    tl->addWidget(m_chart);
    connect(m_chart, &UsageChart::dateSelected, this, &DiskUsagePage::onChartDateSelected);

    m_loadStatus = Ui::rowDesc(QString());
    tl->addWidget(m_loadStatus);
    lay->addWidget(c2);
}

// ---------- ③ 分析标签页：汇总 / 详情 / 排行 ----------
void DiskUsagePage::buildAnalysisTabs(QVBoxLayout *lay)
{
    m_tabBar = new DTabBar(this);
    m_tabBar->setObjectName(QStringLiteral("panshiTabBar"));
    m_tabBar->setVisibleAddButton(false);
    m_tabBar->addTab(tr("区间汇总"));
    m_tabBar->addTab(tr("增长详情"));
    m_tabBar->addTab(tr("占用排行"));
    lay->addWidget(m_tabBar);

    m_tabs = new QStackedWidget(this);
    m_tabs->setObjectName(QStringLiteral("panshiContent"));

    auto *sumPage = new QWidget(m_tabs);
    auto *sumLay = new QVBoxLayout(sumPage);
    sumLay->setContentsMargins(0, 12, 0, 0);
    sumLay->setSpacing(12);
    buildSummaryTab(sumLay);
    m_tabs->addWidget(sumPage);

    auto *detPage = new QWidget(m_tabs);
    auto *detLay = new QVBoxLayout(detPage);
    detLay->setContentsMargins(0, 12, 0, 0);
    detLay->setSpacing(12);
    buildDetailTab(detLay);
    m_tabs->addWidget(detPage);

    auto *rankPage = new QWidget(m_tabs);
    auto *rankLay = new QVBoxLayout(rankPage);
    rankLay->setContentsMargins(0, 12, 0, 0);
    rankLay->setSpacing(12);
    buildRankTab(rankLay);
    m_tabs->addWidget(rankPage);

    lay->addWidget(m_tabs, 1);

    connect(m_tabBar, &DTabBar::currentChanged, m_tabs, &QStackedWidget::setCurrentIndex);
    // 切到「区间汇总」标签后补喂一次宽度：汇总卡行在该标签页内，
    // 标签页非当前时它是隐藏的（QStackedWidget 内 widget 的 show
    // 不会冒泡成 BasePage::showEvent），故不能只依赖基类那两条路径。
    connect(m_tabBar, &DTabBar::currentChanged, this, [this](int idx) {
        if (idx == 0 && m_sumRow) {
            int avail = width();
            if (m_contentScroll && m_contentScroll->viewport())
                avail = m_contentScroll->viewport()->width();
            m_sumRow->setAvailableWidth(avail - 24 * 2);
        }
    });
}

// ---------- ③a 区间汇总标签 ----------
void DiskUsagePage::buildSummaryTab(QVBoxLayout *lay)
{
    // 数值卡：改用 PanshiStateCard（组件承担语义色与字号，不再用
    // panshiStatValue + stat 属性 + QSS 属性选择器染色）
    auto makeStat = [](const QString &title, PanshiUi::Semantic s,
                       PanshiUi::PanshiStateCard **out) {
        auto *card = new PanshiUi::PanshiStateCard(title);
        card->setValue(QStringLiteral("-"), s);
        *out = card;
        return card;
    };

    // 数值卡排布改用 PanshiResponsiveCardRow：窄窗下自动折行。
    // 原为固定 4 列 QGridLayout —— 与总览页同一问题：窄窗下整行横向溢出后
    // 被外层 makeScrollArea（ScrollBarAlwaysOff）静默裁剪，第 4 卡不可达。
    auto *row = new PanshiUi::PanshiResponsiveCardRow(4);
    row->addCard(makeStat(tr("净增长"), PanshiUi::Semantic::Accent, &m_sumNet));
    row->addCard(makeStat(tr("日均增长"), PanshiUi::Semantic::Accent, &m_sumDaily));
    // 最大单日：可视为需留意的峰值 → Warning（原 orange）
    row->addCard(makeStat(tr("最大单日"), PanshiUi::Semantic::Warning, &m_sumMax));
    row->addCard(makeStat(tr("趋势起始日"), PanshiUi::Semantic::Accent, &m_sumTrend));
    m_sumRow = row;
    lay->addWidget(row);
    // 宽度喂入交给 BasePage（viewport Resize + showEvent 双路径），
    // 与总览页/保护页口径统一 —— 单靠 resizeEvent 会在隐藏页首次显示时采到未布局尺寸。
    registerResponsiveCardRow(m_sumRow, m_contentScroll, 24 * 2);

    // 容量预测：语义 Banner（图标 + 柔和底 + 中性正文），替代原 panshiTipBar 自绘框
    m_sumEtaBar = new PanshiUi::PanshiInlineBanner;
    m_sumEtaBar->setSemantic(PanshiUi::Semantic::Warning);
    lay->addWidget(m_sumEtaBar);

    auto *topTitle = Ui::rowLabel(tr("区间增长 Top 项"));
    lay->addWidget(topTitle);
    m_sumTop = Ui::rowDesc(QString());
    lay->addWidget(m_sumTop);
}

// ---------- ③b 增长详情标签 ----------
void DiskUsagePage::buildDetailTab(QVBoxLayout *lay)
{
    // 说明：解释"增长详情"含义，避免用户对列表项数变化产生疑惑
    auto *tip = Ui::rowDesc(tr("说明：这里列出所选日期（点击时间线数据点）相对前一日有增长的目录/文件，"
                               "按增长量排序，最多 8 项；每天增长的目录数量不同，列表项数也随之变化，"
                               "当天无增长时则显示提示。点击目录行可切换上方时间线。"));
    lay->addWidget(tip);

    m_detailTable = makeTable({tr("名称"), tr("大小"), tr("操作")}, this);
    applyTableWidths(m_detailTable, 3);
    PanshiUi::setTableAccessibleName(m_detailTable, tr("增长详情表"));
    connect(m_detailTable, &QTableWidget::cellClicked,
            this, &DiskUsagePage::onDetailCellClicked);
    lay->addWidget(m_detailTable);
}

// ---------- ③c 占用排行标签 ----------
void DiskUsagePage::buildRankTab(QVBoxLayout *lay)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_rankBackBtn = Ui::button(tr("返回上级"));
    m_rankBackBtn->hide();
    connect(m_rankBackBtn, &DPushButton::clicked, this, &DiskUsagePage::onRankBack);
    m_rankPath = Ui::rowLabel(QStringLiteral("/"));
    row->addWidget(m_rankBackBtn);
    row->addWidget(m_rankPath, 1);
    lay->addLayout(row);
    m_rankTable = makeTable({tr("名称"), tr("大小"), tr("占比"), tr("操作")}, this);
    applyTableWidths(m_rankTable, 4);
    PanshiUi::setTableAccessibleName(m_rankTable, tr("占用排行表"));
    connect(m_rankTable, &QTableWidget::cellClicked,
            this, &DiskUsagePage::onRankCellClicked);
    connect(m_rankTable, &QTableWidget::cellDoubleClicked,
            this, &DiskUsagePage::onRankCellDoubleClicked);
    lay->addWidget(m_rankTable);
}

// ---------- ④ 独立设置窗口 ----------
void DiskUsagePage::buildSettingsDialog()
{
    m_settingsDlg = new DDialog(this);
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
        m_cooldownSpin->setSuffix(QStringLiteral(" 小时"));
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
        m_retentionSpin->setSuffix(QStringLiteral(" 天"));
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
        m_detailTable->setRowCount(0);
        m_chart->setSelectedDate(QDate());
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

    // 监控状态行
    QString status;
    if (m_config.enabled())
        status = tr("● 监控中 · 每 %1 分钟采样一次 · 历史保留 %2 天")
                     .arg(m_config.pollMinutes()).arg(m_config.retentionDays());
    else
        status = tr("○ 监控未开启：开启后将自动建立基线并持续监控");
    const DiskHistoryIndex idx = m_store.loadIndex();
    checkStep(tr("读取历史数据索引"));
    if (!idx.days.isEmpty()) {
        const DiskDaySummary &last = idx.days.last();
        status += tr("\n最近记录 %1 · 该日增长 %2")
                      .arg(last.date.toString(QStringLiteral("yyyy-MM-dd")),
                           formatSize(last.growth));
    }
    m_monitorStatus->setText(status);

    // 告警横幅（alert.json 最近一条）
    QFile f(DiskHistoryStore::historyDir() + QStringLiteral("/alert.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        const QJsonArray spikes = doc.object().value(QStringLiteral("spikes")).toArray();
        if (!spikes.isEmpty()) {
            const QJsonObject last = spikes.last().toObject();
            // 文案不再自带「⚠」符号 —— 语义由 Banner 的图标承载（WCAG 1.4.1：
            // 不依赖字符/颜色传递信息，图标 + 柔和底 + 中性正文承担语义）
            m_alertBanner->setSemantic(PanshiUi::Semantic::Danger);
            m_alertBanner->setText(tr("%1 · %2 增长 %3")
                                       .arg(last.value(QStringLiteral("date")).toString(),
                                            last.value(QStringLiteral("causeDir")).toString(),
                                            formatSize(static_cast<qint64>(
                                                last.value(QStringLiteral("growth")).toDouble(0)))));
            m_alertBanner->show();
        } else {
            m_alertBanner->hide();
        }
    } else {
        m_alertBanner->hide();
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
    m_sumNet->setValue(formatSize(st.netGrowth), PanshiUi::Semantic::Accent);
    m_sumDaily->setValue(formatSize(st.dailyAvg), PanshiUi::Semantic::Accent);
    // 最大单日：有值才上 Warning；无数据时用 Muted —— 空格子不该带"需留意"的语义
    if (st.maxDay.isValid()) {
        m_sumMax->setValue(tr("%1（%2）").arg(formatSize(st.maxDayGrowth),
                                              st.maxDay.toString(QStringLiteral("yyyy-MM-dd"))),
                           PanshiUi::Semantic::Warning);
    } else {
        m_sumMax->setValue(QStringLiteral("-"), PanshiUi::Semantic::Muted);
    }
    m_sumTrend->setValue(st.trendStart.isValid()
                             ? st.trendStart.toString(QStringLiteral("yyyy-MM-dd"))
                             : tr("无连续增长"),
                         PanshiUi::Semantic::Accent);
    // 容量预测：语义随内容变化 —— 有明确写满日期时 Warning，增速趋稳时 Accent
    if (st.etaDays > 0) {
        m_sumEtaBar->setSemantic(PanshiUi::Semantic::Warning);
        m_sumEtaBar->setText(tr("按当前增速约 %1 天后写满").arg(st.etaDays));
    } else {
        m_sumEtaBar->setSemantic(PanshiUi::Semantic::Accent);
        m_sumEtaBar->setText(tr("增长趋稳，暂无容量风险"));
    }
    QString top;
    for (const DiskItem &it : st.topGrowItems)
        top += QStringLiteral("· %1  %2\n").arg(it.path, formatSize(it.size));
    m_sumTop->setText(top.trimmed().isEmpty() ? tr("区间内无明显增长") : top.trimmed());
}

void DiskUsagePage::updateDayDetail(const QDate &d)
{
    // 无障碍收尾挂作用域守卫：本函数 4 条 return 出口，统一在析构时补
    const A11yOnScopeExit a11y{m_detailTable};
    m_selected = d;
    m_chart->setSelectedDate(d);
    m_detailTable->setRowCount(0);
    m_detailItems.clear();

    // 目录时间线模式：显示该目录在所选日的大小与环比变化
    if (!m_timelineDir.isEmpty()) {
        int idx = -1;
        for (int i = 0; i < m_records.size(); ++i) {
            if (m_records.at(i).date == d) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            m_detailTable->setRowCount(1);
            m_detailTable->setItem(0, 0, new QTableWidgetItem(tr("所选日期不在当前加载范围内")));
            return;
        }
        const qint64 cur = dirSizeInRecord(m_records.at(idx), m_timelineDir);
        const qint64 prev = idx > 0 ? dirSizeInRecord(m_records.at(idx - 1), m_timelineDir) : -1;
        m_detailTable->setRowCount(2);
        m_detailTable->setItem(0, 0, new QTableWidgetItem(tr("目录 %1").arg(m_timelineDir)));
        m_detailTable->setItem(0, 1, new QTableWidgetItem(cur >= 0 ? formatSize(cur) : tr("无数据")));
        m_detailTable->setItem(0, 2, new QTableWidgetItem());
        if (prev >= 0 && cur >= 0) {
            const qint64 diff = cur - prev;
            m_detailTable->setItem(1, 0, new QTableWidgetItem(tr("较前一日变化")));
            m_detailTable->setItem(1, 1, new QTableWidgetItem((diff >= 0 ? QStringLiteral("+") : QString()) + formatSize(diff)));
        } else {
            m_detailTable->setItem(1, 0, new QTableWidgetItem(tr("前一日暂无数据")));
        }
        m_detailTable->setItem(1, 2, new QTableWidgetItem());
        return;
    }

    int idx = -1;
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records.at(i).date == d) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        m_detailTable->setRowCount(1);
        m_detailTable->setItem(0, 0, new QTableWidgetItem(tr("所选日期不在当前加载范围内")));
        return;
    }
    DiskDayRecord prev;
    if (idx > 0)
        prev = m_records.at(idx - 1);
    const QList<DiskItem> items = DiskHistoryStore::topGrowthItems(prev, m_records.at(idx), 8);
    if (items.isEmpty()) {
        m_detailTable->setRowCount(1);
        m_detailTable->setItem(0, 0, new QTableWidgetItem(tr("该日无显著增长")));
        return;
    }
    m_detailItems = items;
    m_detailTable->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const DiskItem &it = items.at(i);
        setPathCell(m_detailTable, i, 0, it.path);
        m_detailTable->setItem(i, 1, new QTableWidgetItem(formatSize(it.size)));
        auto *btn = makeOpenButton(it.path);
        const QString path = it.path;
        connect(btn, &DIconButton::clicked, this, [this, path] { openInFileManager(path); });
        m_detailTable->setCellWidget(i, 2, btn);
    }
}

void DiskUsagePage::loadRanking()
{
    // 同 updateDayDetail：空数据占位那条 early return 也得补无障碍，用守卫兜住
    const A11yOnScopeExit a11y{m_rankTable};
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
    m_rankFrames.clear();
    m_rankCurrentPath.clear();
    m_rankBackBtn->hide();
    m_rankPath->setText(QStringLiteral("/"));
    m_timelineDir.clear();   // 排行回到根 → 时间线恢复系统盘

    m_rankTable->setRowCount(0);
    if (latest.topLevel.isEmpty()) {
        m_rankTable->setRowCount(1);
        m_rankTable->setItem(0, 0, new QTableWidgetItem(tr("暂无数据：请开启监控或点击“立即扫描”")));
        updateTimelineSeries();
        return;
    }
    m_rankItems = latest.topLevel;
    std::sort(m_rankItems.begin(), m_rankItems.end(),
              [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    const qint64 base = latest.total;
    m_rankTable->setRowCount(0);
    const int n = qMin(m_rankItems.size(), 200);
    m_rankTable->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const DiskItem &it = m_rankItems.at(i);
        setPathCell(m_rankTable, i, 0, it.path);
        m_rankTable->setItem(i, 1, new QTableWidgetItem(formatSize(it.size)));
        const double pct = base > 0 ? double(it.size) * 100.0 / base : 0.0;
        m_rankTable->setItem(i, 2, new QTableWidgetItem(QString::number(pct, 'f', 1) + QStringLiteral("%")));
        auto *btn = makeOpenButton(it.path);
        const QString path = it.path;
        connect(btn, &DIconButton::clicked, this, [this, path] { openInFileManager(path); });
        m_rankTable->setCellWidget(i, 3, btn);
    }
    updateTimelineSeries();
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
        if (m_timelineDir.isEmpty()) {
            m_timelineHint->setText(tr("当前显示：系统盘占用"));
            if (m_backTimelineBtn)
                m_backTimelineBtn->hide();
        } else {
            m_timelineHint->setText(tr("目录“%1”暂无逐日数据，请先开启监控或在该目录下钻").arg(m_timelineDir));
            if (m_backTimelineBtn)
                m_backTimelineBtn->show();
        }
        return;
    }

    if (m_timelineDir.isEmpty()) {
        m_timelineHint->setText(tr("当前显示：系统盘占用变化"));
        if (m_backTimelineBtn)
            m_backTimelineBtn->hide();
        rebuildSpikeSet();
        m_chart->setSpikeDays(m_spikeSet);
    } else {
        m_timelineHint->setText(tr("当前显示：目录 %1 的占用变化").arg(m_timelineDir));
        if (m_backTimelineBtn)
            m_backTimelineBtn->show();
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
    dlg.setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    dlg.setTitle(tr("清除全部监控数据"));
    dlg.setMessage(tr("将删除所有历史占用记录与告警，且不可恢复。确定继续？"));
    dlg.addButton(tr("取消"), false, DDialog::ButtonNormal);
    dlg.addButton(tr("清除"), false, DDialog::ButtonWarning);
    if (dlg.exec() != 1)
        return;
    const QDate today = QDate::currentDate();
    m_store.removeRange(QDate(2000, 1, 1), today.addDays(1));
    QFile::remove(DiskHistoryStore::historyDir() + QStringLiteral("/alert.json"));
    m_records.clear();
    m_selected = QDate();
    m_alertBanner->hide();
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
    // 记录当前帧
    DiskItem parent;
    parent.path = dir;
    m_rankFrames.append(qMakePair(m_rankCurrentPath, m_rankItems));
    m_rankCurrentPath = dir;
    m_rankItems = items;
    std::sort(m_rankItems.begin(), m_rankItems.end(),
              [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    m_rankBackBtn->show();
    m_rankPath->setText(dir);
    m_timelineDir = dir;
    updateTimelineSeries();
    m_rankTable->setRowCount(0);
    const int n = qMin(m_rankItems.size(), 200);
    m_rankTable->setRowCount(n);
    const qint64 base = m_records.isEmpty() ? 0 : m_records.last().total;
    for (int i = 0; i < n; ++i) {
        const DiskItem &it = m_rankItems.at(i);
        setPathCell(m_rankTable, i, 0, it.path);
        m_rankTable->setItem(i, 1, new QTableWidgetItem(formatSize(it.size)));
        const double pct = base > 0 ? double(it.size) * 100.0 / base : 0.0;
        m_rankTable->setItem(i, 2, new QTableWidgetItem(QString::number(pct, 'f', 1) + QStringLiteral("%")));
        auto *btn = makeOpenButton(it.path);
        const QString path = it.path;
        connect(btn, &DIconButton::clicked, this, [this, path] { openInFileManager(path); });
        m_rankTable->setCellWidget(i, 3, btn);
    }
    PanshiUi::finalizeTableAccessible(m_rankTable);
}

void DiskUsagePage::onBackToSystemTimeline()
{
    // 从目录时间线一键返回系统盘时间线
    m_timelineDir.clear();
    updateTimelineSeries();
    if (m_selected.isValid())
        updateDayDetail(m_selected);   // 详情表恢复系统盘增长详情
}

void DiskUsagePage::onDetailCellClicked(int row, int column)
{
    Q_UNUSED(column);
    if (!m_timelineDir.isEmpty())
        return;                       // 目录模式下两行是信息行，不切换
    if (row < 0 || row >= m_detailItems.size())
        return;
    const DiskItem &it = m_detailItems.at(row);
    if (!it.isDir)
        return;
    // 与占用排行一致：点击目录行 → 上方时间线切换为该目录的占用变化
    m_timelineDir = it.path;
    updateTimelineSeries();
    if (m_selected.isValid())
        updateDayDetail(m_selected);  // 详情表同步显示该目录当日变化
}

void DiskUsagePage::onRankCellClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_rankItems.size())
        return;
    const DiskItem &it = m_rankItems.at(row);
    if (!it.isDir)
        return;
    // 单击选择目录：上方时间线切换为该目录的占用变化
    m_timelineDir = it.path;
    updateTimelineSeries();
}

void DiskUsagePage::onRankCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_rankItems.size())
        return;
    const DiskItem &it = m_rankItems.at(row);
    if (!it.isDir)
        return;
    if (m_scanner.isBusy())
        return;
    m_scanner.setAsRoot(true);
    m_scanner.setRootPolicy(DiskScanner::RootPolicy::AllowPrompt);
    showStatus(tr("正在读取 %1 的子目录…").arg(it.path));
    m_scanner.drillInto(it.path);
}

void DiskUsagePage::onRankBack()
{
    if (m_rankFrames.isEmpty())
        return;
    const auto frame = m_rankFrames.takeLast();
    m_rankCurrentPath = frame.first;
    m_rankItems = frame.second;
    if (m_rankFrames.isEmpty()) {
        m_rankBackBtn->hide();
        m_rankPath->setText(QStringLiteral("/"));
        m_timelineDir.clear();
    } else {
        m_rankPath->setText(m_rankCurrentPath);
        m_timelineDir = m_rankCurrentPath;
    }
    updateTimelineSeries();
    m_rankTable->setRowCount(0);
    const int n = qMin(m_rankItems.size(), 200);
    m_rankTable->setRowCount(n);
    const qint64 base = m_records.isEmpty() ? 0 : m_records.last().total;
    for (int i = 0; i < n; ++i) {
        const DiskItem &it = m_rankItems.at(i);
        setPathCell(m_rankTable, i, 0, it.path);
        m_rankTable->setItem(i, 1, new QTableWidgetItem(formatSize(it.size)));
        const double pct = base > 0 ? double(it.size) * 100.0 / base : 0.0;
        m_rankTable->setItem(i, 2, new QTableWidgetItem(QString::number(pct, 'f', 1) + QStringLiteral("%")));
        auto *btn = makeOpenButton(it.path);
        const QString path = it.path;
        connect(btn, &DIconButton::clicked, this, [this, path] { openInFileManager(path); });
        m_rankTable->setCellWidget(i, 3, btn);
    }
    PanshiUi::finalizeTableAccessible(m_rankTable);
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
    updateOverview();
    // ⚠️ 自绘组件在调色板替换后未必自动重绘 —— 显式刷一次整页。
    // 2026-10-03 用户反馈「切换暗色时有时不会变」：本页的自绘卡片
    // （PanshiPanel/StateCard/Badge/InlineBanner 全在 paintEvent 里
    // 实时取 appPalette()，取色逻辑本身是对的）**没有收到重绘通知**，
    // 于是仍显示旧主题的像素。整页 update() 让它们重走 paintEvent。
    update();
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
