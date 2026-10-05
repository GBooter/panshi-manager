// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>

namespace Panshi {

// ---------- 数据模型 ----------
struct SnapshotInfo {
    QString id;
    QString name;
    QString desc;
    qint64  time = 0;
    bool    isFirst = false;      // 系统初始备份
    bool    isRollbackBackup = false;
    qint64  extraSize = 0;        // 快照数据目录物理大小
    qint64  uniqueSize = 0;       // 独占 OSTree 对象大小
    qint64  totalSize = 0;
};

struct DeployInfo {
    QString systemVersion;
    QString commitId;
    qint64  time = 0;
    bool    isBooted = false;
    bool    isRollbackBackup = false;
};

struct WritableStatus {
    bool enable = false;          // 可写层是否开启
    bool clearAfterReboot = false;
    QJsonObject raw;
};

struct RepoSizes {
    qint64 sysRepo = 0;
    qint64 persistRepo = 0;
    qint64 snapshotRoot = 0;
    qint64 freeRoot = 0;
};

struct RollbackState {
    QString rollbackTarget;
    QString rollbackTargetName;
    QString backupSnapshotId;
    QString backupSnapshotName;
    QString originalBooted;
    QString strategy;             // manual / reboot3 / none
    QString status;               // pending / active / done / failed / cancelled
    int     rebootCount = 0;
    int     rebootLimit = 3;
    qint64  createdAt = 0;
    bool    valid = false;
};

// ---------- 执行结果 ----------
struct CommandResult {
    bool ok = false;
    int  exitCode = -1;
    QString stdoutText;
    QString stderrText;
    QString errorMessage;         // 非零退出时的错误提示
};

// ---------- 后端 API ----------
bool isImmutable();
QString getKernel();
QJsonObject getOsVersion();
QString getDeployVersion();

QList<SnapshotInfo> getSnapshots();
SnapshotInfo getSnapshotDetail(const QString &id);
CommandResult createSnapshot(const QString &name, const QString &desc);
CommandResult deleteSnapshot(const QString &id);
CommandResult modifySnapshot(const QString &id, const QString &name, const QString &desc);
CommandResult rollbackSnapshot(const QString &id, bool deleteOthers);

QList<DeployInfo> getDeploys();
CommandResult undeploy(int index);

WritableStatus getWritableStatus();
CommandResult enableWritable(bool clearAfterReboot, bool allDirs);
CommandResult disableWritable();

RepoSizes getRepoSizes();
QMap<QString, QPair<qint64, qint64>> computeSnapshotSizes(); // id -> (extra, unique)

QMap<QString, QString> getCtlConfig();
CommandResult setCtlConfig(const QMap<QString, QString> &updates);

CommandResult checkUpgrade();
CommandResult runUpgrade();
CommandResult cleanUpgradeCache(bool allCache);

RollbackState getRollbackState();
CommandResult saveRollbackState(const QJsonObject &state);
void clearRollbackState();
CommandResult installRollbackCleanup();
CommandResult uninstallRollbackCleanup();
bool rollbackCleanupInstalled();
QString getBootedDeployId();

// ---------- 工具函数 ----------
QString formatSize(qint64 bytes);
QString formatTime(qint64 ts);
QString jsonField(const QJsonObject &obj, const QStringList &names, const QString &def = QString());

} // namespace Panshi

Q_DECLARE_METATYPE(Panshi::SnapshotInfo)
Q_DECLARE_METATYPE(QList<Panshi::SnapshotInfo>)
Q_DECLARE_METATYPE(Panshi::DeployInfo)
Q_DECLARE_METATYPE(QList<Panshi::DeployInfo>)
Q_DECLARE_METATYPE(Panshi::WritableStatus)
Q_DECLARE_METATYPE(Panshi::RepoSizes)
Q_DECLARE_METATYPE(Panshi::RollbackState)
Q_DECLARE_METATYPE(Panshi::CommandResult)
