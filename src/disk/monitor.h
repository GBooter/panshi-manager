// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "history.h"
#include "scanner.h"
#include "monitorconfig.h"

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QMap>

namespace Panshi {

/**
 * 磁盘监控：一轮完整监控周期（由 systemd user timer 周期拉起 --monitor 触发）。
 * 流程：
 *   1. df 采样 → 更新/新建当日记录，追加高频样本；
 *   2. 对比最近完整日记录 → 增长超阈值则定位元凶（顶层 du → 下钻 du）；
 *   3. 若当日尚无完整 topLevel 基线 → 补一次全量 du；
 *   4. 满足冷却/排除/免打扰条件 → 发送 DDE 通知并记录告警；
 *   5. 保存当日记录（预算/保留自动执行）。
 */
class DiskMonitor : public QObject
{
    Q_OBJECT
public:
    explicit DiskMonitor(QObject *parent = nullptr);
    ~DiskMonitor() override;

    void runCycle();
    void shutdown();

signals:
    void finished(bool ok, const QString &message);

private slots:
    void onTopLevelFinished(const QList<DiskItem> &items, bool complete);
    void onDrillFinished(const QString &dir, const QList<DiskItem> &items);
    void onScanFinished(bool ok, const QString &error);
    void onWatchdog();

private:
    void finish(bool ok, const QString &msg);
    void loadConfig();
    bool isQuietHours(const QDateTime &now) const;
    bool isExcluded(const QString &path) const;
    bool inCooldown(const QString &path) const;
    void recordAlertTime(const QString &path);
    qint64 computeThresholdBytes();
    void saveAlerts();
    void loadAlerts();
    void sendNotification(const DiskSpikeInfo &spike);
    void finalize();

    DiskHistoryStore m_store;
    DiskScanner m_scanner;
    MonitorConfig m_config;

    DiskDayRecord m_today;
    DiskDayRecord m_prevDay;
    QList<DiskSpikeInfo> m_spikes;         // 本轮发现的告警（并入 alert.json）

    bool m_running = false;
    bool m_pendingAlert = false;
    qint64 m_pendingGrowth = 0;
    QString m_causeDir;
    QTimer *m_watchdog = nullptr;

    QMap<QString, QDateTime> m_lastAlert;  // 目录 → 上次告警时间（冷却）
    QString m_alertPath;
};

} // namespace Panshi
