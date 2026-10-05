// SPDX-License-Identifier: GPL-3.0-or-later
#include "history.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <algorithm>

namespace Panshi {

namespace {
constexpr int kIndexVersion = 1;

bool writeJsonAtomic(const QString &path, const QJsonDocument &doc)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(doc.toJson(QJsonDocument::Compact));
    return f.commit();
}

QJsonDocument readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonDocument();
    return QJsonDocument::fromJson(f.readAll());
}
} // namespace

// ----------------------------------------------------------------------
// JSON 序列化
// ----------------------------------------------------------------------
QJsonObject itemToJson(const DiskItem &it)
{
    return QJsonObject{
        {QStringLiteral("p"), it.path},
        {QStringLiteral("s"), static_cast<double>(it.size)},
        {QStringLiteral("d"), it.isDir},
    };
}

DiskItem itemFromJson(const QJsonObject &o)
{
    DiskItem it;
    it.path = o.value(QStringLiteral("p")).toString();
    it.size = static_cast<qint64>(o.value(QStringLiteral("s")).toDouble(0));
    it.isDir = o.value(QStringLiteral("d")).toBool(true);
    return it;
}

QJsonObject sampleToJson(const DiskSample &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("t"), s.ts.toString(Qt::ISODateWithMs));
    o.insert(QStringLiteral("total"), static_cast<double>(s.total));
    QJsonObject hot;
    for (auto it = s.hotDirs.cbegin(); it != s.hotDirs.cend(); ++it)
        hot.insert(it.key(), static_cast<double>(it.value()));
    o.insert(QStringLiteral("h"), hot);
    return o;
}

DiskSample sampleFromJson(const QJsonObject &o)
{
    DiskSample s;
    s.ts = QDateTime::fromString(o.value(QStringLiteral("t")).toString(), Qt::ISODateWithMs);
    s.total = static_cast<qint64>(o.value(QStringLiteral("total")).toDouble(0));
    const QJsonObject hot = o.value(QStringLiteral("h")).toObject();
    for (auto it = hot.begin(); it != hot.end(); ++it)
        s.hotDirs.insert(it.key(), static_cast<qint64>(it.value().toDouble(0)));
    return s;
}

QJsonObject dayRecordToJson(const DiskDayRecord &d)
{
    QJsonObject o;
    o.insert(QStringLiteral("date"), d.date.toString(Qt::ISODate));
    o.insert(QStringLiteral("capacity"), static_cast<double>(d.capacity));
    o.insert(QStringLiteral("total"), static_cast<double>(d.total));
    o.insert(QStringLiteral("complete"), d.complete);
    QJsonArray tl;
    for (const DiskItem &it : d.topLevel)
        tl.append(itemToJson(it));
    o.insert(QStringLiteral("topLevel"), tl);
    QJsonObject dc;
    for (auto it = d.drillCache.cbegin(); it != d.drillCache.cend(); ++it) {
        QJsonArray arr;
        for (const DiskItem &i : it.value())
            arr.append(itemToJson(i));
        dc.insert(it.key(), arr);
    }
    o.insert(QStringLiteral("drillCache"), dc);
    QJsonArray sm;
    for (const DiskSample &s : d.samples)
        sm.append(sampleToJson(s));
    o.insert(QStringLiteral("samples"), sm);
    return o;
}

DiskDayRecord dayRecordFromJson(const QJsonObject &o)
{
    DiskDayRecord d;
    d.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
    d.capacity = static_cast<qint64>(o.value(QStringLiteral("capacity")).toDouble(0));
    d.total = static_cast<qint64>(o.value(QStringLiteral("total")).toDouble(0));
    d.complete = o.value(QStringLiteral("complete")).toBool(true);
    const QJsonArray tl = o.value(QStringLiteral("topLevel")).toArray();
    for (const QJsonValue &v : tl)
        d.topLevel.append(itemFromJson(v.toObject()));
    const QJsonObject dc = o.value(QStringLiteral("drillCache")).toObject();
    for (auto it = dc.begin(); it != dc.end(); ++it) {
        QList<DiskItem> lst;
        for (const QJsonValue &v : it.value().toArray())
            lst.append(itemFromJson(v.toObject()));
        d.drillCache.insert(it.key(), lst);
    }
    const QJsonArray sm = o.value(QStringLiteral("samples")).toArray();
    for (const QJsonValue &v : sm)
        d.samples.append(sampleFromJson(v.toObject()));
    return d;
}

QJsonObject daySummaryToJson(const DiskDaySummary &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("date"), s.date.toString(Qt::ISODate));
    o.insert(QStringLiteral("f"), s.file);
    o.insert(QStringLiteral("c"), static_cast<double>(s.capacity));
    o.insert(QStringLiteral("t"), static_cast<double>(s.total));
    o.insert(QStringLiteral("g"), static_cast<double>(s.growth));
    o.insert(QStringLiteral("spike"), s.spike);
    QJsonArray tg;
    for (const DiskItem &it : s.topGrow)
        tg.append(itemToJson(it));
    o.insert(QStringLiteral("top"), tg);
    return o;
}

DiskDaySummary daySummaryFromJson(const QJsonObject &o)
{
    DiskDaySummary s;
    s.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
    s.file = o.value(QStringLiteral("f")).toString();
    s.capacity = static_cast<qint64>(o.value(QStringLiteral("c")).toDouble(0));
    s.total = static_cast<qint64>(o.value(QStringLiteral("t")).toDouble(0));
    s.growth = static_cast<qint64>(o.value(QStringLiteral("g")).toDouble(0));
    s.spike = o.value(QStringLiteral("spike")).toBool(false);
    const QJsonArray tg = o.value(QStringLiteral("top")).toArray();
    for (const QJsonValue &v : tg)
        s.topGrow.append(itemFromJson(v.toObject()));
    return s;
}

// ----------------------------------------------------------------------
// DiskHistoryStore
// ----------------------------------------------------------------------
QString DiskHistoryStore::historyDir()
{
    // 用 GenericDataLocation 显式拼 appName，**不要**用 AppDataLocation：
    // AppDataLocation 在设置了 organizationName 时会多拼一层组织目录
    // （实测 Qt6：GUI 进程 setOrganizationName("deepin") →
    //   ~/.local/share/deepin/panshi-manager，而 monitormain.cpp 只设
    //   applicationName → ~/.local/share/panshi-manager）。守护进程写的监控
    // 数据会因此对 GUI 完全不可见。GenericDataLocation 只解析到 ~/.local/share，
    // 两个进程殊途同归，也与文档/install.sh/translations 的既有布局一致。
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + QStringLiteral("/panshi-manager");
    const QString dir = base + QStringLiteral("/disk-history");
    QDir().mkpath(dir);
    return dir;
}

QString DiskHistoryStore::dayFilePath(const QDate &d)
{
    return historyDir() + QStringLiteral("/%1.json").arg(d.toString(Qt::ISODate));
}

DiskHistoryStore::DiskHistoryStore(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Panshi::DiskDayRecord>();
    qRegisterMetaType<QList<Panshi::DiskDayRecord>>();
    qRegisterMetaType<QList<Panshi::DiskItem>>();
    qRegisterMetaType<Panshi::DiskDaySummary>();
    qRegisterMetaType<Panshi::DiskHistoryIndex>();
    qRegisterMetaType<Panshi::DiskSpikeInfo>();
    qRegisterMetaType<Panshi::DiskRangeStats>();
}

DiskHistoryIndex DiskHistoryStore::loadIndex()
{
    DiskHistoryIndex idx;
    const QJsonDocument doc = readJson(historyDir() + QStringLiteral("/index.json"));
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        const QJsonArray arr = o.value(QStringLiteral("days")).toArray();
        for (const QJsonValue &v : arr)
            idx.days.append(daySummaryFromJson(v.toObject()));
        idx.firstDate = QDate::fromString(o.value(QStringLiteral("first")).toString(), Qt::ISODate);
        idx.lastDate = QDate::fromString(o.value(QStringLiteral("last")).toString(), Qt::ISODate);
        idx.storageBytes = static_cast<qint64>(o.value(QStringLiteral("storage")).toDouble(0));
        idx.budgetBytes = m_budgetBytes;
    }
    if (idx.days.isEmpty())
        return rebuildIndex();
    return idx;
}

DiskHistoryIndex DiskHistoryStore::rebuildIndex()
{
    DiskHistoryIndex idx;
    const QDir dir(historyDir());
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"),
                                            QDir::Files, QDir::Name);
    QList<DiskDaySummary> summaries;
    for (const QString &fname : files) {
        if (fname == QStringLiteral("index.json"))
            continue;
        const QDate d = QDate::fromString(fname.left(10), Qt::ISODate);
        if (!d.isValid())
            continue;
        const QJsonDocument doc = readJson(dir.absoluteFilePath(fname));
        if (!doc.isObject())
            continue;
        const DiskDayRecord rec = dayRecordFromJson(doc.object());
        DiskDaySummary s;
        s.date = d;
        s.file = fname;
        s.capacity = rec.capacity;
        s.total = rec.total;
        summaries.append(s);
    }
    std::sort(summaries.begin(), summaries.end(),
              [](const DiskDaySummary &a, const DiskDaySummary &b) { return a.date < b.date; });
    DiskDaySummary *prev = nullptr;
    for (DiskDaySummary &s : summaries) {
        if (prev) {
            s.growth = s.total - prev->total;
            s.spike = s.growth >= m_spikeThresholdBytes;
        }
        prev = &s;
    }
    idx.days = summaries;
    if (!summaries.isEmpty()) {
        idx.firstDate = summaries.first().date;
        idx.lastDate = summaries.last().date;
    }
    idx.storageBytes = storageBytes();
    idx.budgetBytes = m_budgetBytes;
    // ⚠️ 只有重建出有效索引才回写 index.json。
    // 目录为空、没有任何日记录（监控未开启时正是这种状态）时**不能**写盘，
    // 否则每次 loadIndex 都会走 rebuildIndex 写一份空 index.json，
    // 其 mtime 变化会触发 QFileSystemWatcher::directoryChanged →
    // onHistoryDirChanged → 防抖刷新 → requestRange → onRangeLoaded →
    // rebuildSpikeSet → loadIndex → 又 rebuildIndex …… 形成**死循环**，
    // 每次循环都 setLoading + 重启图表动画，表现为「数据区域一直闪屏」。
    // 监控开启后后台持续写入使索引稳定，循环自然终止，故现象仅出现在未开启时。
    if (!summaries.isEmpty())
        writeIndex(idx);
    return idx;
}

void DiskHistoryStore::writeIndex(const DiskHistoryIndex &idx)
{
    QJsonArray arr;
    for (const DiskDaySummary &s : idx.days)
        arr.append(daySummaryToJson(s));
    QJsonObject o;
    o.insert(QStringLiteral("version"), kIndexVersion);
    o.insert(QStringLiteral("first"), idx.firstDate.toString(Qt::ISODate));
    o.insert(QStringLiteral("last"), idx.lastDate.toString(Qt::ISODate));
    o.insert(QStringLiteral("storage"), static_cast<double>(idx.storageBytes));
    o.insert(QStringLiteral("budget"), static_cast<double>(idx.budgetBytes));
    o.insert(QStringLiteral("days"), arr);
    writeJsonAtomic(historyDir() + QStringLiteral("/index.json"), QJsonDocument(o));
}

void DiskHistoryStore::updateIndexWith(const DiskDayRecord &rec, DiskHistoryIndex *idx)
{
    DiskDaySummary s;
    s.date = rec.date;
    s.file = QFileInfo(dayFilePath(rec.date)).fileName();
    s.capacity = rec.capacity;
    s.total = rec.total;
    // growth / spike / topGrow：与最近一个前一日完整记录对比
    QDate prevDate;
    for (auto it = idx->days.rbegin(); it != idx->days.rend(); ++it) {
        if (it->date < rec.date) {
            prevDate = it->date;
            break;
        }
    }
    DiskDayRecord prevRec;
    if (prevDate.isValid() && loadDay(prevDate, &prevRec)) {
        s.growth = rec.total - prevRec.total;
        s.spike = s.growth >= m_spikeThresholdBytes;
        if (!prevRec.topLevel.isEmpty())
            s.topGrow = topGrowthItems(prevRec, rec, 3);
    } else {
        s.growth = 0;
        s.spike = false;
    }
    bool replaced = false;
    for (DiskDaySummary &d : idx->days) {
        if (d.date == rec.date) {
            d = s;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        idx->days.append(s);
    std::sort(idx->days.begin(), idx->days.end(),
              [](const DiskDaySummary &a, const DiskDaySummary &b) { return a.date < b.date; });
    if (!idx->days.isEmpty()) {
        idx->firstDate = idx->days.first().date;
        idx->lastDate = idx->days.last().date;
    }
    idx->storageBytes = storageBytes();
    idx->budgetBytes = m_budgetBytes;
}

bool DiskHistoryStore::saveDayRecord(const DiskDayRecord &rec)
{
    if (!rec.date.isValid())
        return false;
    const QString path = dayFilePath(rec.date);
    if (!writeJsonAtomic(path, QJsonDocument(dayRecordToJson(rec))))
        return false;
    addToCache(rec);
    DiskHistoryIndex idx = loadIndex();
    updateIndexWith(rec, &idx);
    writeIndex(idx);
    pruneExpired(m_retentionDays);
    enforceBudget();
    return true;
}

bool DiskHistoryStore::loadDay(const QDate &d, DiskDayRecord *out)
{
    {
        QMutexLocker lk(&m_cacheMutex);
        auto it = m_cache.constFind(d);
        if (it != m_cache.constEnd()) {
            *out = it.value();
            return true;
        }
    }
    const QJsonDocument doc = readJson(dayFilePath(d));
    if (!doc.isObject())
        return false;
    *out = dayRecordFromJson(doc.object());
    return true;
}

QList<DiskDayRecord> DiskHistoryStore::loadRange(const QDate &from, const QDate &to)
{
    QList<DiskDayRecord> out;
    if (!from.isValid() || !to.isValid() || from > to)
        return out;
    for (QDate d = from; d <= to; d = d.addDays(1)) {
        DiskDayRecord rec;
        if (loadDay(d, &rec)) {
            out.append(rec);
            addToCache(rec);
        }
    }
    return out;
}

quint64 DiskHistoryStore::requestRange(const QDate &from, const QDate &to)
{
    if (!from.isValid() || !to.isValid() || from > to)
        return 0;
    const quint64 id = m_nextId++;
    auto *watcher = new QFutureWatcher<QList<DiskDayRecord>>(this);
    connect(watcher, &QFutureWatcher<QList<DiskDayRecord>>::finished, this,
            [this, watcher, id] {
                const QList<DiskDayRecord> records = watcher->result();
                watcher->deleteLater();
                emit rangeLoaded(id, records);
            });
    watcher->setFuture(QtConcurrent::run([this, from, to, id]() {
        QList<DiskDayRecord> out;
        const int total = from.daysTo(to) + 1;
        int loaded = 0;
        for (QDate d = from; d <= to; d = d.addDays(1)) {
            if (isCancelled(id))
                break;
            DiskDayRecord rec;
            if (loadDay(d, &rec)) {
                out.append(rec);
                addToCache(rec);
            }
            ++loaded;
            emit rangeProgress(id, loaded, total);
        }
        {
            QMutexLocker lk(&m_cancelMutex);
            m_cancelled.remove(id);
        }
        return out;
    }));
    return id;
}

void DiskHistoryStore::cancelRange(quint64 requestId)
{
    QMutexLocker lk(&m_cancelMutex);
    m_cancelled.insert(requestId);
}

bool DiskHistoryStore::isCancelled(quint64 requestId) const
{
    QMutexLocker lk(&m_cancelMutex);
    return m_cancelled.contains(requestId);
}

bool DiskHistoryStore::removeRange(const QDate &from, const QDate &to)
{
    bool any = false;
    for (QDate d = from; d <= to; d = d.addDays(1)) {
        const QString p = dayFilePath(d);
        if (QFile::exists(p) && QFile::remove(p))
            any = true;
    }
    if (any) {
        {
            QMutexLocker lk(&m_cacheMutex);
            for (QDate d = from; d <= to; d = d.addDays(1)) {
                m_cache.remove(d);
                m_cacheOrder.removeOne(d);
            }
        }
        writeIndex(rebuildIndex());
    }
    return any;
}

// ----------------------------------------------------------------------
// 预算与清理
// ----------------------------------------------------------------------
qint64 DiskHistoryStore::storageBytes() const
{
    qint64 total = 0;
    const QDir dir(historyDir());
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"),
                                            QDir::Files);
    for (const QString &fname : files)
        total += QFileInfo(dir.absoluteFilePath(fname)).size();
    return total;
}

void DiskHistoryStore::pruneExpired(int retentionDays)
{
    if (retentionDays <= 0)
        return;
    const QDate cutoff = QDate::currentDate().addDays(-retentionDays);
    const QDir dir(historyDir());
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"),
                                            QDir::Files);
    bool changed = false;
    for (const QString &fname : files) {
        if (fname == QStringLiteral("index.json"))
            continue;
        const QDate d = QDate::fromString(fname.left(10), Qt::ISODate);
        if (d.isValid() && d < cutoff) {
            QFile::remove(dir.absoluteFilePath(fname));
            changed = true;
        }
    }
    if (changed)
        writeIndex(rebuildIndex());
}

bool DiskHistoryStore::enforceBudget()
{
    bool changed = false;
    // 循环降级直至占用回到预算内或无法再降级
    for (int step = 0; step < 4; ++step) {
        if (storageBytes() <= m_budgetBytes)
            break;
        const QDir dir(historyDir());
        const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"),
                                                QDir::Files);
        // 阶梯1：裁剪非近期样本（只保留最近 samplesRetentionDays 天）
        if (step == 0) {
            const QDate cutoff = QDate::currentDate().addDays(-m_samplesRetentionDays);
            for (const QString &fname : files) {
                if (fname == QStringLiteral("index.json"))
                    continue;
                const QDate d = QDate::fromString(fname.left(10), Qt::ISODate);
                if (d.isValid() && d < cutoff) {
                    const QJsonDocument doc = readJson(dir.absoluteFilePath(fname));
                    if (!doc.isObject())
                        continue;
                    DiskDayRecord rec = dayRecordFromJson(doc.object());
                    if (rec.samples.isEmpty())
                        continue;
                    rec.samples.clear();
                    if (writeJsonAtomic(dir.absoluteFilePath(fname),
                                        QJsonDocument(dayRecordToJson(rec))))
                        changed = true;
                }
            }
        }
        // 阶梯2：裁剪非近期、非飙升日的下钻缓存
        else if (step == 1) {
            const QDate cutoff = QDate::currentDate().addDays(-m_drillCacheKeepDays);
            for (const QString &fname : files) {
                if (fname == QStringLiteral("index.json"))
                    continue;
                const QDate d = QDate::fromString(fname.left(10), Qt::ISODate);
                if (d.isValid() && d < cutoff) {
                    const QJsonDocument doc = readJson(dir.absoluteFilePath(fname));
                    if (!doc.isObject())
                        continue;
                    DiskDayRecord rec = dayRecordFromJson(doc.object());
                    if (rec.drillCache.isEmpty())
                        continue;
                    rec.drillCache.clear();
                    if (writeJsonAtomic(dir.absoluteFilePath(fname),
                                        QJsonDocument(dayRecordToJson(rec))))
                        changed = true;
                }
            }
        }
        // 阶梯3：将最旧一批日记录合并为聚合摘要（丢弃 samples/drillCache）
        else if (step == 2) {
            QList<QDate> dates;
            for (const QString &fname : files) {
                if (fname == QStringLiteral("index.json"))
                    continue;
                const QDate d = QDate::fromString(fname.left(10), Qt::ISODate);
                if (d.isValid())
                    dates.append(d);
            }
            std::sort(dates.begin(), dates.end());
            if (dates.size() > m_drillCacheKeepDays) {
                const int mergeCount = qMin(30, dates.size() / 2);
                for (int i = 0; i < mergeCount; ++i) {
                    const QDate d = dates[i];
                    const QJsonDocument doc = readJson(dir.absoluteFilePath(d.toString(Qt::ISODate) + QStringLiteral(".json")));
                    if (!doc.isObject())
                        continue;
                    DiskDayRecord rec = dayRecordFromJson(doc.object());
                    if (rec.samples.isEmpty() && rec.drillCache.isEmpty())
                        continue;
                    rec.samples.clear();
                    rec.drillCache.clear();
                    if (writeJsonAtomic(dir.absoluteFilePath(d.toString(Qt::ISODate) + QStringLiteral(".json")),
                                        QJsonDocument(dayRecordToJson(rec))))
                        changed = true;
                }
            }
        } else {
            break; // 阶梯4：提示用户（由上层处理），不再自动降级
        }
    }
    if (changed)
        writeIndex(rebuildIndex());
    return changed;
}

void DiskHistoryStore::addToCache(const DiskDayRecord &rec)
{
    QMutexLocker lk(&m_cacheMutex);
    if (m_cache.contains(rec.date))
        m_cacheOrder.removeOne(rec.date);
    m_cache.insert(rec.date, rec);
    m_cacheOrder.append(rec.date);
    while (m_cacheOrder.size() > m_cacheDays && !m_cacheOrder.isEmpty()) {
        const QDate oldest = m_cacheOrder.takeFirst();
        m_cache.remove(oldest);
    }
}

// ----------------------------------------------------------------------
// 统计
// ----------------------------------------------------------------------
QList<DiskItem> DiskHistoryStore::topGrowthItems(const DiskDayRecord &prev,
                                                 const DiskDayRecord &cur, int n)
{
    QMap<QString, qint64> prevSizes;
    for (const DiskItem &it : prev.topLevel)
        prevSizes.insert(it.path, it.size);
    QList<DiskItem> grows;
    for (const DiskItem &it : cur.topLevel) {
        const qint64 base = prevSizes.value(it.path, 0);
        const qint64 delta = it.size - base;
        if (delta > 0)
            grows.append({it.path, delta, it.isDir});
    }
    // 若当日下钻缓存可用，用下钻项补充更细粒度原因
    for (auto it = cur.drillCache.cbegin(); it != cur.drillCache.cend(); ++it) {
        for (const DiskItem &i : it.value())
            grows.append({i.path, i.size, i.isDir});
    }
    std::sort(grows.begin(), grows.end(),
              [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    return grows.mid(0, n);
}

QList<DiskSpikeInfo> DiskHistoryStore::detectSpikes(const QList<DiskDayRecord> &days,
                                                    qint64 thresholdBytes)
{
    QList<DiskSpikeInfo> out;
    if (days.size() < 2)
        return out;
    QList<DiskDayRecord> sorted = days;
    std::sort(sorted.begin(), sorted.end(),
              [](const DiskDayRecord &a, const DiskDayRecord &b) { return a.date < b.date; });
    for (int i = 1; i < sorted.size(); ++i) {
        const qint64 g = sorted[i].total - sorted[i - 1].total;
        if (g < thresholdBytes)
            continue;
        DiskSpikeInfo sp;
        sp.date = sorted[i].date;
        sp.growth = g;
        sp.causes = topGrowthItems(sorted[i - 1], sorted[i], 5);
        if (!sp.causes.isEmpty())
            sp.causeDir = sp.causes.first().path;
        out.append(sp);
    }
    return out;
}

DiskRangeStats DiskHistoryStore::computeRangeStats(const QList<DiskDayRecord> &days)
{
    DiskRangeStats st;
    if (days.isEmpty())
        return st;
    QList<DiskDayRecord> sorted = days;
    std::sort(sorted.begin(), sorted.end(),
              [](const DiskDayRecord &a, const DiskDayRecord &b) { return a.date < b.date; });
    st.beginUsed = sorted.first().total;
    st.endUsed = sorted.last().total;
    st.netGrowth = st.endUsed - st.beginUsed;
    const int n = sorted.size();
    st.dailyAvg = (n > 1) ? (st.netGrowth / (n - 1)) : 0;

    // 最大单日增长 + 最长连续增长段（趋势起始日）
    int runLen = 0, maxRunLen = 0;
    QDate runStart, bestRunStart;
    for (int i = 1; i < n; ++i) {
        const qint64 g = sorted[i].total - sorted[i - 1].total;
        if (g > st.maxDayGrowth) {
            st.maxDayGrowth = g;
            st.maxDay = sorted[i].date;
        }
        if (g > 0) {
            if (runLen == 0)
                runStart = sorted[i].date;
            ++runLen;
            if (runLen > maxRunLen) {
                maxRunLen = runLen;
                bestRunStart = runStart;
            }
        } else {
            runLen = 0;
        }
    }
    st.trendStart = bestRunStart;

    // 容量预测：按日均增长线性外推
    if (st.dailyAvg > 0 && sorted.last().capacity > sorted.last().total)
        st.etaDays = (sorted.last().capacity - sorted.last().total) / st.dailyAvg;
    else
        st.etaDays = -1;

    // 区间增长 Top 项（首末 topLevel 差值）
    QMap<QString, qint64> firstSizes, lastSizes;
    for (const DiskItem &it : sorted.first().topLevel)
        firstSizes.insert(it.path, it.size);
    for (const DiskItem &it : sorted.last().topLevel)
        lastSizes.insert(it.path, it.size);
    QList<DiskItem> grows;
    for (auto it = lastSizes.cbegin(); it != lastSizes.cend(); ++it) {
        const qint64 base = firstSizes.value(it.key(), 0);
        const qint64 delta = it.value() - base;
        if (delta > 0)
            grows.append({it.key(), delta, true});
    }
    std::sort(grows.begin(), grows.end(),
              [](const DiskItem &a, const DiskItem &b) { return a.size > b.size; });
    st.topGrowItems = grows.mid(0, 10);
    return st;
}

} // namespace Panshi
