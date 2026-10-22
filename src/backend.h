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
    int     index = -1;       // ctl 真实 Index（隐藏主线固定为 0，可能与 booted 冲突，故删除改传 commitId）
    bool    hidden = false;   // 隐藏主线部署：待生效的替换主线，受系统保护不可删除
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
CommandResult undeploy(const QString &commitId);
QStringList undeployArgs(const QString &commitId);          // 删除命令参数（探针用：断言「传 commitId 而非下标」）
bool isDeployDeletable(const DeployInfo &d);                // 删除按钮可用性判据（当前运行/待生效主线/无标识一律不可删，单一真相）

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

/**
 * 待更新软件包明细（apt 视图）。
 *
 * deepin-immutable-ctl upgrade check 只输出计数（Total count / Need download
 * count），不告诉用户"要更新哪些包"。本函数从 apt 侧取明细（只读查询，
 * 不提权、不写任何文件），供更新页分组展示。
 */
struct UpgradablePackage {
    QString name;        // deb 包名
    QString newVersion;  // 仓库候选版本
    QString oldVersion;  // 当前安装版本（deb-ostree 同版本重建时两者相同）
    QString section;     // apt Section（分类依据；查询失败时为空）
};
QList<UpgradablePackage> queryUpgradablePackages();

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
/**
 * 把 pkexec / 命令执行的英文错误信息转成中文友好提示。
 * 识别不出的原始信息原样返回（不丢信息）。所有页面展示后端错误时都应走这里。
 */
QString friendlyExecError(const QString &raw);

} // namespace Panshi

Q_DECLARE_METATYPE(Panshi::SnapshotInfo)
Q_DECLARE_METATYPE(QList<Panshi::SnapshotInfo>)
Q_DECLARE_METATYPE(Panshi::DeployInfo)
Q_DECLARE_METATYPE(QList<Panshi::DeployInfo>)
Q_DECLARE_METATYPE(Panshi::WritableStatus)
Q_DECLARE_METATYPE(Panshi::RepoSizes)
Q_DECLARE_METATYPE(Panshi::RollbackState)
Q_DECLARE_METATYPE(Panshi::CommandResult)
