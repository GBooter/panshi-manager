// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor.h"
#include "backend.h"   // Panshi::formatSize

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTime>
#include <QTimer>

namespace Panshi {

namespace {
constexpr qint64 kMB = 1024LL * 1024;

bool writeJsonAtomic(const QString &path, const QJsonDocument &doc)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(doc.toJson(QJsonDocument::Compact));
    return f.commit();
}
} // namespace

DiskMonitor::DiskMonitor(QObject *parent)
    : QObject(parent)
{
    m_alertPath = DiskHistoryStore::historyDir() + QStringLiteral("/alert.json");
    loadAlerts();
    // 自动扫描：优先 sudo -n 免密，失败自动降级为普通用户扫描（数据标记不完整）
    m_scanner.setAsRoot(true);
    m_scanner.setRootPolicy(DiskScanner::RootPolicy::AutoNoPrompt);
    connect(&m_scanner, &DiskScanner::topLevelFinished, this, &DiskMonitor::onTopLevelFinished);
    connect(&m_scanner, &DiskScanner::drillFinished, this, &DiskMonitor::onDrillFinished);
    connect(&m_scanner, &DiskScanner::scanFinished, this, &DiskMonitor::onScanFinished);
    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, &DiskMonitor::onWatchdog);
}

DiskMonitor::~DiskMonitor()
{
    m_scanner.cancel();
}

void DiskMonitor::runCycle()
{
    if (m_running) {
        emit finished(false, tr("监控周期已在运行"));
        return;
    }
    m_running = true;
    loadConfig();
    if (!m_config.enabled()) {
        finish(false, tr("监控未开启"));
        return;
    }
    m_pendingAlert = false;
    m_pendingGrowth = 0;
    m_causeDir.clear();
    m_spikes.clear();
    m_watchdog->start(30 * 60 * 1000); // 30 分钟看门狗

    // 1. df 采样
    qint64 capacity = 0, used = 0;
    if (!DiskScanner::dfRoot(&capacity, &used)) {
        finish(false, tr("df 查询失败"));
        return;
    }
    const QDate today = QDate::currentDate();
    DiskDayRecord rec;
    if (!m_store.loadDay(today, &rec)) {
        rec = DiskDayRecord();
        rec.date = today;
    }
    rec.capacity = capacity;
    rec.total = used;
    DiskSample s;
    s.ts = QDateTime::currentDateTime();
    s.total = used;
    rec.samples.append(s);
    m_today = rec;

    // 2. 最近一个有完整 topLevel 的历史日
    m_prevDay = DiskDayRecord();
    for (int back = 1; back <= 60; ++back) {
        DiskDayRecord r;
        if (m_store.loadDay(today.addDays(-back), &r) && !r.topLevel.isEmpty()) {
            m_prevDay = r;
            break;
        }
    }

    const qint64 threshold = computeThresholdBytes();
    m_store.setSpikeThreshold(threshold);
    const qint64 growth = m_prevDay.date.isValid() ? (used - m_prevDay.total) : 0;
    const bool hasFullToday = !m_today.topLevel.isEmpty();

    // 3. 异常增长检测
    if (growth >= threshold && m_prevDay.date.isValid()) {
        m_pendingAlert = true;
        m_pendingGrowth = growth;
        if (hasFullToday) {
            // 已有今日顶层数据：直接 diff 定位
            const QList<DiskItem> grows = DiskHistoryStore::topGrowthItems(m_prevDay, m_today, 1);
            if (!grows.isEmpty() && grows.first().isDir && !isExcluded(grows.first().path)) {
                m_causeDir = grows.first().path;
                m_scanner.drillInto(m_causeDir);
                return;
            }
            finalize();
            return;
        }
        // 无今日基线：先全量扫描（同时完成今日基线）
        m_scanner.startTopLevelScan();
        return;
    }
    // 4. 每日完整基线
    if (!hasFullToday) {
        m_scanner.startTopLevelScan();
        return;
    }
    // 5. 常规采样保存
    finalize();
}

// ----------------------------------------------------------------------
// 扫描回调
// ----------------------------------------------------------------------
void DiskMonitor::onTopLevelFinished(const QList<DiskItem> &items, bool complete)
{
    m_today.topLevel = items;
    m_today.complete = complete;
    if (m_pendingAlert) {
        const QList<DiskItem> grows = DiskHistoryStore::topGrowthItems(m_prevDay, m_today, 1);
        if (!grows.isEmpty() && grows.first().isDir && !isExcluded(grows.first().path)) {
            m_causeDir = grows.first().path;
            m_scanner.drillInto(m_causeDir);
            return;
        }
    }
    finalize();
}

void DiskMonitor::onDrillFinished(const QString &dir, const QList<DiskItem> &items)
{
    m_today.drillCache.insert(dir, items);
    if (m_pendingAlert) {
        DiskSpikeInfo sp;
        sp.date = m_today.date;
        sp.growth = m_pendingGrowth;
        sp.causeDir = dir;
        sp.causes = items.mid(0, 5);
        if (!inCooldown(dir) && !isQuietHours(QDateTime::currentDateTime())) {
            sendNotification(sp);
            recordAlertTime(dir);
            sp.alerted = true;
        }
        m_spikes.append(sp);
        saveAlerts();
    }
    finalize();
}

void DiskMonitor::onScanFinished(bool ok, const QString &error)
{
    if (!m_running)
        return;
    if (!ok)
        finish(false, error);
    // 成功时数据信号已在 topLevelFinished / drillFinished 处理
}

void DiskMonitor::onWatchdog()
{
    finish(false, tr("监控周期超时"));
}

// ----------------------------------------------------------------------
// 收尾
// ----------------------------------------------------------------------
void DiskMonitor::finalize()
{
    m_store.setRetentionDays(m_config.retentionDays());
    m_store.setBudgetBytes(static_cast<qint64>(m_config.storageBudgetMB()) * kMB);
    m_store.setSamplesRetentionDays(m_config.samplesRetentionDays());
    m_store.setMemoryCacheDays(m_config.memoryCacheDays());
    m_store.saveDayRecord(m_today);
    finish(true, tr("监控完成"));
}

void DiskMonitor::finish(bool ok, const QString &msg)
{
    if (!m_running)
        return;
    m_running = false;
    m_watchdog->stop();
    m_scanner.cancel();
    emit finished(ok, msg);
}

void DiskMonitor::shutdown()
{
    m_watchdog->stop();
    m_scanner.cancel();
}

void DiskMonitor::loadConfig()
{
    // 配置为惰性读取（MonitorConfig 每次取值都即时回读 DConfig/QSettings）
}

// ----------------------------------------------------------------------
// 判定辅助
// ----------------------------------------------------------------------
bool DiskMonitor::isQuietHours(const QDateTime &now) const
{
    const QTime t = now.time();
    const QTime start = QTime::fromString(m_config.quietStart(), QStringLiteral("HH:mm"));
    const QTime end = QTime::fromString(m_config.quietEnd(), QStringLiteral("HH:mm"));
    if (!start.isValid() || !end.isValid())
        return false;
    if (start <= end)
        return t >= start && t < end;
    return t >= start || t < end; // 跨午夜
}

bool DiskMonitor::isExcluded(const QString &path) const
{
    const QString home = QDir::homePath();
    const QStringList ex = m_config.excludes();
    for (const QString &raw : ex) {
        QString p = raw;
        if (p == QLatin1String("~"))
            p = home;
        else if (p.startsWith(QLatin1String("~/")))
            p = home + p.mid(1);
        if (!p.isEmpty() && (path == p || path.startsWith(p + QLatin1Char('/'))))
            return true;
    }
    return false;
}

bool DiskMonitor::inCooldown(const QString &path) const
{
    const auto it = m_lastAlert.constFind(path);
    if (it == m_lastAlert.constEnd())
        return false;
    return it.value().secsTo(QDateTime::currentDateTime()) < m_config.cooldownHours() * 3600;
}

void DiskMonitor::recordAlertTime(const QString &path)
{
    m_lastAlert.insert(path, QDateTime::currentDateTime());
}

qint64 DiskMonitor::computeThresholdBytes()
{
    const qint64 minBytes = static_cast<qint64>(m_config.minAlertMB()) * kMB;
    if (m_config.alertMode() != QStringLiteral("baseline"))
        return qMax(minBytes, static_cast<qint64>(m_config.fixedThresholdMB()) * kMB);
    // 智能基线：近 baselineDays 天正向日均增长 × 倍数（至少 minAlertMB）
    const DiskHistoryIndex idx = m_store.loadIndex();
    const QDate today = QDate::currentDate();
    const int N = qMax(3, m_config.baselineDays());
    qint64 sum = 0;
    int count = 0;
    int taken = 0;
    for (int i = idx.days.size() - 1; i >= 0 && taken < N; --i) {
        const DiskDaySummary &s = idx.days.at(i);
        if (s.date >= today)
            continue;
        if (s.growth > 0) {
            sum += s.growth;
            ++count;
        }
        ++taken;
    }
    const qint64 base = (count > 0) ? (sum / count) : 0;
    const qint64 computed = static_cast<qint64>(static_cast<double>(base) * m_config.baselineMultiplier());
    return qMax(minBytes, computed);
}

// ----------------------------------------------------------------------
// 告警持久化
// ----------------------------------------------------------------------
void DiskMonitor::loadAlerts()
{
    QFile f(m_alertPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject la = doc.object().value(QStringLiteral("lastAlert")).toObject();
    for (auto it = la.begin(); it != la.end(); ++it) {
        m_lastAlert.insert(it.key(),
                           QDateTime::fromString(it.value().toString(), Qt::ISODateWithMs));
    }
}

void DiskMonitor::saveAlerts()
{
    QJsonObject la;
    for (auto it = m_lastAlert.cbegin(); it != m_lastAlert.cend(); ++it)
        la.insert(it.key(), it.value().toString(Qt::ISODateWithMs));

    QJsonArray spikes;
    QFile f(m_alertPath);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject())
            spikes = doc.object().value(QStringLiteral("spikes")).toArray();
    }
    for (const DiskSpikeInfo &sp : m_spikes) {
        bool dup = false;
        for (const QJsonValue &v : spikes) {
            const QJsonObject old = v.toObject();
            if (old.value(QStringLiteral("date")).toString() == sp.date.toString(Qt::ISODate)
                && old.value(QStringLiteral("causeDir")).toString() == sp.causeDir) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        QJsonObject o;
        o.insert(QStringLiteral("date"), sp.date.toString(Qt::ISODate));
        o.insert(QStringLiteral("growth"), static_cast<double>(sp.growth));
        o.insert(QStringLiteral("causeDir"), sp.causeDir);
        o.insert(QStringLiteral("alerted"), sp.alerted);
        QJsonArray causes;
        for (const DiskItem &it : sp.causes)
            causes.append(itemToJson(it));
        o.insert(QStringLiteral("causes"), causes);
        spikes.append(o);
    }
    while (spikes.size() > 200)
        spikes.removeAt(0);

    QJsonObject out;
    out.insert(QStringLiteral("lastAlert"), la);
    out.insert(QStringLiteral("spikes"), spikes);
    writeJsonAtomic(m_alertPath, QJsonDocument(out));
}

// ----------------------------------------------------------------------
// 通知
// ----------------------------------------------------------------------
void DiskMonitor::sendNotification(const DiskSpikeInfo &spike)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.Notifications"),
                         QStringLiteral("/org/freedesktop/Notifications"),
                         QStringLiteral("org.freedesktop.Notifications"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid())
        return;
    const QString summary = tr("磁盘空间异常增长");
    const QString body = tr("系统盘 %1 增长 %2，主要来源：%3")
                             .arg(spike.date.toString(Qt::ISODate),
                                  formatSize(spike.growth),
                                  spike.causeDir);
    QStringList actions;
    actions << QStringLiteral("default") << tr("查看详情");
    QVariantMap hints;
    hints.insert(QStringLiteral("x-deepin-action-view"),
                 QStringLiteral("panshi-manager,--show-disk,%1")
                     .arg(spike.date.toString(Qt::ISODate)));
    QList<QVariant> args;
    args << QStringLiteral("panshi-manager")      // app_name
         << QVariant(0u)                          // replaces_id
         << QStringLiteral("drive-harddisk")      // app_icon
         << summary << body
         << actions << hints
         << QVariant(-1);                         // expire_timeout: 系统默认
    iface.callWithArgumentList(QDBus::AutoDetect, QStringLiteral("Notify"), args);
}

} // namespace Panshi
