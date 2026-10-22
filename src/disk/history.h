// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "disktypes.h"

#include <QObject>
#include <QDate>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QJsonObject>

namespace Panshi {

// ---------- JSON 序列化（GUI 与监控进程共用） ----------
QJsonObject itemToJson(const DiskItem &it);
DiskItem itemFromJson(const QJsonObject &o);
QJsonObject sampleToJson(const DiskSample &s);
DiskSample sampleFromJson(const QJsonObject &o);
QJsonObject dayRecordToJson(const DiskDayRecord &d);
DiskDayRecord dayRecordFromJson(const QJsonObject &o);
QJsonObject daySummaryToJson(const DiskDaySummary &s);
DiskDaySummary daySummaryFromJson(const QJsonObject &o);

/**
 * 磁盘历史存储：逐日文件 + 轻量索引 + 存储预算 + 懒加载。
 * 线程安全说明：
 *  - loadIndex / saveDayRecord 供主线程（GUI 或监控进程主循环）调用；
 *  - requestRange 内部在 QtConcurrent 工作线程逐日读取，进度经信号回传；
 *  - 内存 LRU 缓存由互斥锁保护，跨线程安全。
 */
class DiskHistoryStore : public QObject
{
    Q_OBJECT
public:
    explicit DiskHistoryStore(QObject *parent = nullptr);

    static QString historyDir();
    static QString dayFilePath(const QDate &d);

    // ---- 索引 ----
    DiskHistoryIndex loadIndex();
    bool saveDayRecord(const DiskDayRecord &rec);
    bool removeRange(const QDate &from, const QDate &to);
    void pruneExpired(int retentionDays);
    bool enforceBudget();

    // ---- 同步读取（可在后台线程调用） ----
    QList<DiskDayRecord> loadRange(const QDate &from, const QDate &to);
    bool loadDay(const QDate &d, DiskDayRecord *out);

    // ---- 异步懒加载 ----
    quint64 requestRange(const QDate &from, const QDate &to);
    void cancelRange(quint64 requestId);

    // ---- 配置（由监控进程按 DConfig 设置） ----
    qint64 storageBytes() const;
    void setBudgetBytes(qint64 b) { m_budgetBytes = b; }
    qint64 budgetBytes() const { return m_budgetBytes; }
    void setRetentionDays(int n) { m_retentionDays = n; }
    void setSamplesRetentionDays(int n) { m_samplesRetentionDays = n; }
    void setDrillCacheKeepDays(int n) { m_drillCacheKeepDays = n; }
    void setMemoryCacheDays(int n) { m_cacheDays = n; }
    void setSpikeThreshold(qint64 bytes) { m_spikeThresholdBytes = bytes; }

    // ---- 统计 ----
    static DiskRangeStats computeRangeStats(const QList<DiskDayRecord> &days);
    static QList<DiskSpikeInfo> detectSpikes(const QList<DiskDayRecord> &days, qint64 thresholdBytes);
    static QList<DiskItem> topGrowthItems(const DiskDayRecord &prev, const DiskDayRecord &cur, int n);

signals:
    void rangeProgress(quint64 requestId, int loaded, int total);
    void rangeLoaded(quint64 requestId, const QList<DiskDayRecord> &records);

private:
    DiskHistoryIndex rebuildIndex();
    void writeIndex(const DiskHistoryIndex &idx);
    void updateIndexWith(const DiskDayRecord &rec, DiskHistoryIndex *idx);
    void addToCache(const DiskDayRecord &rec);
    bool isCancelled(quint64 requestId) const;

    qint64 m_budgetBytes = 512LL * 1024 * 1024;      // 默认 512MB
    int    m_retentionDays = 400;
    int    m_samplesRetentionDays = 90;
    int    m_drillCacheKeepDays = 30;
    int    m_cacheDays = 90;
    qint64 m_spikeThresholdBytes = 1024LL * 1024 * 1024; // 摘要级飙升标记（告警阈值由监控进程控制）

    QHash<QDate, DiskDayRecord> m_cache;
    QList<QDate> m_cacheOrder;
    mutable QMutex m_cacheMutex;

    QSet<quint64> m_cancelled;
    mutable QMutex m_cancelMutex;
    quint64 m_nextId = 1;
};

} // namespace Panshi

Q_DECLARE_METATYPE(Panshi::DiskItem)
Q_DECLARE_METATYPE(QList<Panshi::DiskItem>)
Q_DECLARE_METATYPE(Panshi::DiskDayRecord)
Q_DECLARE_METATYPE(QList<Panshi::DiskDayRecord>)
Q_DECLARE_METATYPE(Panshi::DiskDaySummary)
Q_DECLARE_METATYPE(Panshi::DiskHistoryIndex)
Q_DECLARE_METATYPE(Panshi::DiskSpikeInfo)
Q_DECLARE_METATYPE(Panshi::DiskRangeStats)
