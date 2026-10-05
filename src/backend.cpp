// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.h"

#include <QProcess>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QThreadPool>
#include <QtConcurrent>
#include <QSet>
#include <QHash>

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <ctime>

namespace Panshi {

namespace {
/**
 * 判断 sudo 的失败输出是否属于「需要密码」类（决定能否回退 pkexec）。
 *
 * 必须做**本地化匹配**（缺陷 A3-1，2026-10-02 实测）：
 *   sudo 的提示随 LANG 变化，中文环境下输出「sudo: 需要密码」，
 *   完全不含拉丁字母的 "password"。只匹配英文会让**中文 locale 下
 *   pkexec 回退永不触发** —— 而中文是本项目主语言，等于主路径失效。
 *
 * 覆盖措辞：
 *   password / passphrase —— 英文系（含 "a password is required"）
 *   密码                 —— 中文系（"需要密码"）
 *
 * 刻意**不**匹配的情况：用户不在 sudoers 组时的
 * "user is not in the sudoers file" —— 该用户本就无权，回退只是白等授权框。
 */
bool needsPassword(const QString &stderrText)
{
    return stderrText.contains(QStringLiteral("password"), Qt::CaseInsensitive)
        || stderrText.contains(QStringLiteral("passphrase"), Qt::CaseInsensitive)
        || stderrText.contains(QString::fromUtf8("密码"));
}

const QString CTL = QStringLiteral("/usr/sbin/deepin-immutable-ctl");
const QString WRITABLE = QStringLiteral("/usr/bin/deepin-immutable-writable");
const QString OSTREE = QStringLiteral("/usr/bin/ostree");
const QString SYS_REPO = QStringLiteral("/ostree/repo");
const QString PERSIST_REPO = QStringLiteral("/persistent/ostree/repo");
const QString SNAPSHOT_DIR = QStringLiteral("/persistent/ostree/snapshot");
const QString CTL_CONF = QStringLiteral("/etc/deepin-immutable-ctl/deepin-immutable-ctl.conf");
const QString STATE_FILE = QStringLiteral("/var/lib/panshi-manager/rollback-state.json");

QString cacheDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir d(base);
    if (!d.exists()) d.mkpath(".");
    return base;
}

CommandResult runProcess(const QString &program, const QStringList &args, int timeoutMs, bool asRoot)
{
    CommandResult r;
    QStringList full = args;
    QProcess proc;
    if (asRoot && geteuid() != 0) {
        // 优先 sudo -n 免密
        proc.start(QStringLiteral("sudo"), QStringList() << QStringLiteral("-n") << program << full);
    } else {
        proc.start(program, full);
    }
    if (!proc.waitForStarted(5000)) {
        r.errorMessage = QObject::tr("无法启动进程: %1").arg(program);
        return r;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(3000);
        r.errorMessage = QObject::tr("命令执行超时: %1").arg(program);
        return r;
    }
    r.exitCode = proc.exitCode();
    r.stdoutText = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    r.stderrText = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    r.ok = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);

    // sudo 需要密码时回退 pkexec
    //
    // ⚠️ 必须覆盖**本地化**输出（2026-10-02 实测缺陷 A3-1）：
    //   sudo 的「需要密码」提示随 LANG 变化 —— 英文环境是
    //     "sudo: a password is required"
    //   中文环境则是
    //     "sudo: 需要密码"（不含任何拉丁字母的 "password"）
    //   原实现只匹配 "password"，导致 **zh_CN 下 pkexec 回退永不触发** ——
    //   而 zh_CN 恰是本项目的主语言与默认 locale，即「主路径失效」。
    //   症状：英文实例一切正常，中文实例的 root 操作直接失败 Banner。
    //
    // 判据设计：命中「需要密码」类措辞才回退。
    //   - "password" / "密码"           —— sudo 的标准措辞（en / zh_CN）
    //   - "passphrase"                  —— 部分发行版/配置下的变体
    // 反之，用户不在 sudoers 组时报 "user is not in the sudoers file"，
    // 不含上述任一措辞 → 不回退、直接失败（这是**期望行为**：
    // 该用户本就无权，弹 polkit 框也只是白等）。
    if (!r.ok && asRoot && geteuid() != 0 && needsPassword(r.stderrText)) {
        QProcess pk;
        pk.start(QStringLiteral("pkexec"), QStringList() << program << full);
        if (pk.waitForStarted(5000) && pk.waitForFinished(timeoutMs)) {
            r.exitCode = pk.exitCode();
            r.stdoutText = QString::fromUtf8(pk.readAllStandardOutput()).trimmed();
            r.stderrText = QString::fromUtf8(pk.readAllStandardError()).trimmed();
            r.ok = (pk.exitStatus() == QProcess::NormalExit && pk.exitCode() == 0);
        }
    }

    if (!r.ok) {
        r.errorMessage = !r.stderrText.isEmpty() ? r.stderrText : r.stdoutText;
        if (r.errorMessage.isEmpty()) r.errorMessage = QObject::tr("命令执行失败");
    }
    return r;
}

CommandResult runCtl(const QStringList &args, bool asRoot, int timeoutMs)
{
    return runProcess(CTL, args, timeoutMs, asRoot);
}

CommandResult runWritable(const QStringList &args, bool asRoot, int timeoutMs)
{
    return runProcess(WRITABLE, args, timeoutMs, asRoot);
}

QJsonObject parseJson(const QString &text, bool *okPtr = nullptr)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        if (okPtr) *okPtr = false;
        return QJsonObject();
    }
    if (okPtr) *okPtr = true;
    return doc.object();
}

// du 结果内存缓存 (30 秒 TTL, 避免重复 du 大目录拖慢启动)
qint64 duPath(const QString &path)
{
    struct DuCache { qint64 ts = 0; qint64 size = 0; };
    static QHash<QString, DuCache> cache;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto it = cache.find(path);
    if (it != cache.end() && now - it->ts < 30)
        return it->size;

    qint64 size = 0;
    const CommandResult r = runProcess(QStringLiteral("du"), QStringList() << "-sb" << path, 60000, false);
    if (r.ok) {
        const QStringList parts = r.stdoutText.split(QLatin1Char('\t'));
        bool ok = false;
        const qint64 v = parts.isEmpty() ? 0 : parts.first().trimmed().toLongLong(&ok);
        if (ok) size = v;
    }
    cache.insert(path, {now, size});
    return size;
}

QStringList readRefs(const QString &prefix)
{
    QStringList refs;
    const QDir base(SYS_REPO + QStringLiteral("/refs/heads/") + prefix);
    if (!base.exists()) return refs;
    const auto entries = base.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : entries) {
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly)) {
            const QString commit = QString::fromUtf8(f.readAll()).trimmed();
            if (!commit.isEmpty()) refs << commit;
        }
    }
    return refs;
}

// 读取 refs/heads/<prefix> 下的所有 ref, 返回 {ref文件名: commit}
QMap<QString, QString> readRefMap(const QString &prefix)
{
    QMap<QString, QString> refs;
    const QDir base(SYS_REPO + QStringLiteral("/refs/heads/") + prefix);
    if (!base.exists()) return refs;
    const auto entries = base.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : entries) {
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly)) {
            const QString commit = QString::fromUtf8(f.readAll()).trimmed();
            if (!commit.isEmpty()) refs.insert(fi.baseName(), commit);
        }
    }
    return refs;
}

QMap<QString, qint64> ostreeLsTree(const QString &commit)
{
    QMap<QString, qint64> result;
    const CommandResult r = runProcess(OSTREE,
        QStringList() << ("--repo=" + SYS_REPO) << "ls" << "-R" << "-C" << commit,
        300000, false);
    if (!r.ok) return result;
    const QStringList lines = r.stdoutText.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 6) continue;
        const QChar typ = parts[0].at(0);
        bool okSize = false;
        const qint64 size = parts[3].toLongLong(&okSize);
        const QString csum = parts[4];
        if (typ == QLatin1Char('-') && okSize && csum.size() == 64) {
            result.insert(csum, size);
        }
    }
    return result;
}

// 带文件缓存的 ostree ls 结果 (与 Python 版 _load_tree_cache 一致)
// 缓存路径: ~/.cache/panshi-manager/tree_<key>.json
QMap<QString, qint64> ostreeLsTreeCached(const QString &key, const QString &commit)
{
    const QString cacheFile = cacheDir() + QStringLiteral("/tree_") + key + QStringLiteral(".json");
    QFile f(cacheFile);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("commit")).toString() == commit) {
                QMap<QString, qint64> files;
                const QJsonObject filesObj = obj.value(QStringLiteral("files")).toObject();
                for (auto it = filesObj.constBegin(); it != filesObj.constEnd(); ++it)
                    files.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
                return files;
            }
        }
    }
    // 未命中: 计算并写缓存
    const QMap<QString, qint64> files = ostreeLsTree(commit);
    QJsonObject filesObj;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it)
        filesObj.insert(it.key(), static_cast<double>(it.value()));
    QJsonObject obj;
    obj.insert(QStringLiteral("commit"), commit);
    obj.insert(QStringLiteral("files"), filesObj);
    QFile wf(cacheFile);
    if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        wf.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return files;
}

} // anonymous namespace

// ----------------------------------------------------------------------
// 基本信息
// ----------------------------------------------------------------------
bool isImmutable()
{
    const CommandResult r = runCtl(QStringList() << "-s", false, 30000);
    if (!r.ok) return true;
    const QRegularExpression re(QStringLiteral("Immutable mode:\\s*(true|false)"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(r.stdoutText);
    return m.hasMatch() ? m.captured(1).compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 : true;
}

QString getKernel()
{
    struct utsname u {};
    if (uname(&u) == 0) return QString::fromUtf8(u.release);
    return QString();
}

QJsonObject getOsVersion()
{
    QJsonObject res;
    res.insert(QStringLiteral("system"), QStringLiteral("Deepin"));
    for (const QString &path : {QStringLiteral("/etc/os-version"), QStringLiteral("/etc/deepin-version")}) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString txt = QString::fromUtf8(f.readAll());
        QRegularExpression m(QStringLiteral("SystemName\\s*=\\s*(.+)"));
        auto mm = m.match(txt);
        if (mm.hasMatch()) res.insert(QStringLiteral("system"), mm.captured(1).trimmed());
        m = QRegularExpression(QStringLiteral("Version\\s*=\\s*(\\S+)"));
        mm = m.match(txt);
        if (mm.hasMatch()) res.insert(QStringLiteral("version"), mm.captured(1).trimmed());
        m = QRegularExpression(QStringLiteral("EditionName(?:\\[zh_CN\\])?\\s*=\\s*(.+)"));
        mm = m.match(txt);
        if (mm.hasMatch()) res.insert(QStringLiteral("edition"), mm.captured(1).trimmed());
        break;
    }
    return res;
}

QString getDeployVersion()
{
    const CommandResult r = runCtl(QStringList() << "admin" << "status" << "-j", false, 60000);
    if (!r.ok) return QString();
    const QJsonArray dl = parseJson(r.stdoutText).value(QStringLiteral("data")).toObject()
                              .value(QStringLiteral("deploy_list")).toArray();

    // ⚠️ 不要取 dl.first()（缺陷：总览页「磐石部署」显示为空，2026-10-05 定位）。
    //
    //   实测 deepin-immutable-ctl admin status -j 的输出里，deploy_list 的
    //   **第一项不是当前运行的部署**，而是待生效的新部署：
    //     [0] IsBooted=false  SystemVersion=""                    ← first()
    //     [1] IsBooted=true   SystemVersion="deepin-V25-25.2.0-100.100"
    //     [2] IsBooted=false  SystemVersion="deepin-V25-25.2.2-103.100"
    //   取 first() 拿到的是空串，页面上就退化成 "-"。
    //
    //   判定「当前运行」必须用 IsBooted —— 与同文件的 getBootedDeployId()
    //   保持一致（那里本来就是这么写的，此处是漏改的复制粘贴）。
    //   若已启动项自身版本号为空（新部署尚未写入版本信息），退而取第一个
    //   非空版本，总比整栏空白好。
    QString booted;
    QString firstNonEmpty;
    for (const QJsonValue &v : dl) {
        const QJsonObject o = v.toObject();
        const QString ver = o.value(QStringLiteral("SystemVersion")).toString();
        if (ver.isEmpty())
            continue;
        if (firstNonEmpty.isEmpty())
            firstNonEmpty = ver;
        if (o.value(QStringLiteral("IsBooted")).toBool()) {
            booted = ver;
            break;
        }
    }
    return booted.isEmpty() ? firstNonEmpty : booted;
}

// ----------------------------------------------------------------------
// 快照
// ----------------------------------------------------------------------
QList<SnapshotInfo> getSnapshots()
{
    QList<SnapshotInfo> list;
    const CommandResult r = runCtl(QStringList() << "snapshot" << "list" << "-j", false, 60000);
    if (!r.ok) return list;
    const QJsonObject obj = parseJson(r.stdoutText);
    const QJsonArray arr = obj.value(QStringLiteral("data")).toObject()
                              .value(QStringLiteral("snapshot_list")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        SnapshotInfo s;
        s.id = o.value(QStringLiteral("id")).toString();
        s.name = o.value(QStringLiteral("name")).toString();
        s.desc = o.value(QStringLiteral("desc")).toString();
        s.time = o.value(QStringLiteral("time")).toVariant().toLongLong();
        s.isFirst = (s.name.contains(QStringLiteral("First"))
                     || s.desc.contains(QStringLiteral("System initialization")));
        list.append(s);
    }
    // 按时间升序
    std::sort(list.begin(), list.end(), [](const SnapshotInfo &a, const SnapshotInfo &b) {
        return a.time < b.time;
    });
    return list;
}

SnapshotInfo getSnapshotDetail(const QString &id)
{
    SnapshotInfo s;
    const CommandResult r = runCtl(QStringList() << "snapshot" << "show" << id << "-j", false, 60000);
    if (!r.ok) return s;
    const QJsonObject o = parseJson(r.stdoutText).value(QStringLiteral("data")).toObject();
    s.id = o.value(QStringLiteral("id")).toString();
    s.name = o.value(QStringLiteral("name")).toString();
    s.desc = o.value(QStringLiteral("desc")).toString();
    s.time = o.value(QStringLiteral("time")).toVariant().toLongLong();
    return s;
}

CommandResult createSnapshot(const QString &name, const QString &desc)
{
    QStringList args = {QStringLiteral("snapshot"), QStringLiteral("create")};
    if (!name.isEmpty()) args << name;
    // ⚠️ 描述**必须始终作为第二位置参数传入**（缺陷 A3-2，2026-10-02 实测）：
    //   deepin-immutable-ctl 的 snapshot create 要求 `<名称> <描述>` 双参数，
    //   省略描述会导致参数个数不符而**直接失败**。UI 上「描述」标的是「(可选)」，
    //   与 CLI 契约矛盾 —— 此处按「可选的是内容、不是位置」处理：
    //   传空串占位（比传 "无描述" 这类硬编码文案更干净，且不污染 UI 语言）。
    //   同理适用于 modifySnapshot（同一 CLI 约定）。
    args << (desc.isEmpty() ? QString() : desc);
    return runCtl(args, true, 900000);
}

CommandResult deleteSnapshot(const QString &id)
{
    return runCtl(QStringList() << "snapshot" << "delete" << id, true, 900000);
}

CommandResult modifySnapshot(const QString &id, const QString &name, const QString &desc)
{
    QStringList args = {QStringLiteral("snapshot"), QStringLiteral("modify"), id};
    if (!name.isEmpty()) args << name;
    // 与 createSnapshot 同因（A3-2）：描述作为位置参数始终传入，缺省用空串占位
    args << (desc.isEmpty() ? QString() : desc);
    return runCtl(args, true, 300000);
}

CommandResult rollbackSnapshot(const QString &id, bool deleteOthers)
{
    QStringList args = {QStringLiteral("snapshot"), QStringLiteral("rollback"), id};
    if (deleteOthers) args << QStringLiteral("--delete-others");
    return runCtl(args, true, 1800000);
}

// ----------------------------------------------------------------------
// 部署
// ----------------------------------------------------------------------
QList<DeployInfo> getDeploys()
{
    QList<DeployInfo> list;
    const CommandResult r = runCtl(QStringList() << "admin" << "status" << "-j", false, 60000);
    if (!r.ok) return list;
    const QJsonArray arr = parseJson(r.stdoutText).value(QStringLiteral("data")).toObject()
                              .value(QStringLiteral("deploy_list")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        DeployInfo d;
        d.systemVersion = o.value(QStringLiteral("SystemVersion")).toString();
        d.commitId = o.value(QStringLiteral("CommitIDIdx")).toString();
        d.time = o.value(QStringLiteral("Time")).toVariant().toLongLong();
        d.isBooted = o.value(QStringLiteral("IsBooted")).toBool();
        d.isRollbackBackup = o.value(QStringLiteral("IsRollbackBackup")).toBool();
        list.append(d);
    }
    return list;
}

CommandResult undeploy(int index)
{
    return runCtl(QStringList() << "admin" << "undeploy" << QString::number(index), true, 600000);
}

// ----------------------------------------------------------------------
// 可写层 / 系统保护
// ----------------------------------------------------------------------
WritableStatus getWritableStatus()
{
    WritableStatus w;
    const CommandResult r = runWritable(QStringList() << "status" << "-j", false, 30000);
    if (!r.ok) return w;
    w.raw = parseJson(r.stdoutText).value(QStringLiteral("data")).toObject()
               .value(QStringLiteral("conf")).toObject();
    w.enable = w.raw.value(QStringLiteral("enable")).toBool(false);
    w.clearAfterReboot = w.raw.value(QStringLiteral("clearAfterReboot")).toBool(false);
    return w;
}

CommandResult enableWritable(bool clearAfterReboot, bool allDirs)
{
    QStringList args = {QStringLiteral("enable")};
    if (allDirs) args << QStringLiteral("-a");
    if (clearAfterReboot) args << QStringLiteral("-c");
    args << QStringLiteral("-y");
    return runWritable(args, true, 300000);
}

CommandResult disableWritable()
{
    return runWritable(QStringList() << "disable" << "-y", true, 300000);
}

// ----------------------------------------------------------------------
// 空间统计
// ----------------------------------------------------------------------
RepoSizes getRepoSizes()
{
    RepoSizes s;
    s.sysRepo = duPath(SYS_REPO);
    s.persistRepo = duPath(PERSIST_REPO);
    s.snapshotRoot = duPath(SNAPSHOT_DIR);
    for (const QString &path : {QStringLiteral("/"), QStringLiteral("/persistent")}) {
        struct statvfs st {};
        if (statvfs(path.toUtf8().constData(), &st) == 0) {
            const qint64 free = static_cast<qint64>(st.f_bavail) * st.f_frsize;
            if (path == QLatin1String("/")) s.freeRoot = free;
        }
    }
    return s;
}

QMap<QString, QPair<qint64, qint64>> computeSnapshotSizes()
{
    // 60 秒内存缓存: 总览页与备份管理页频繁调用, 避免重复计算
    struct SizeCache { qint64 ts = 0; QMap<QString, QPair<qint64, qint64>> data; };
    static SizeCache memCache;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (!memCache.data.isEmpty() && now - memCache.ts < 60)
        return memCache.data;

    QMap<QString, QPair<qint64, qint64>> result;
    const QList<SnapshotInfo> snaps = getSnapshots();
    if (snaps.isEmpty()) return result;

    const QStringList depCommits = readRefs(QStringLiteral("deployment"));
    const QStringList snapCommits = readRefs(QStringLiteral("snapshot"));

    // 收集所有 commit 的文件对象集合 (共享对象, 走文件缓存)
    QStringList allCommits = depCommits;
    for (const QString &c : snapCommits) allCommits << c;
    allCommits.removeDuplicates();

    QSet<QString> shared;
    // 并行加载各 commit 的文件对象 (首启并行计算, 二次走文件缓存)
    {
        QList<QFuture<QMap<QString, qint64>>> futures;
        for (const QString &c : allCommits) {
            const QString key = QStringLiteral("share_") + c.left(8);
            futures << QtConcurrent::run([key, c]() { return ostreeLsTreeCached(key, c); });
        }
        for (auto &f : futures) {
            f.waitForFinished();
            const QMap<QString, qint64> files = f.result();
            for (auto it = files.constBegin(); it != files.constEnd(); ++it)
                shared.insert(it.key());
        }
    }

    // 每个快照: extra = 数据目录物理大小; unique = 树中非共享对象逻辑大小
    const QMap<QString, QString> snapRefMap = readRefMap(QStringLiteral("snapshot"));
    for (const SnapshotInfo &s : snaps) {
        const QString sid = s.id;
        const qint64 extra = duPath(SNAPSHOT_DIR + QLatin1Char('/') + sid);
        qint64 unique = 0;
        // 找到该快照对应的 commit (snapshot ref 的文件名 = id)
        const QString commit = snapRefMap.value(sid);
        if (!commit.isEmpty()) {
            const QString key = QStringLiteral("snap_") + sid.left(8);
            const QMap<QString, qint64> files = ostreeLsTreeCached(key, commit);
            for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
                if (!shared.contains(it.key())) unique += it.value();
            }
        }
        result.insert(sid, qMakePair(extra, unique));
    }
    memCache = {now, result};
    return result;
}

// ----------------------------------------------------------------------
// 配置文件读写
// ----------------------------------------------------------------------
QMap<QString, QString> getCtlConfig()
{
    QMap<QString, QString> cfg;
    QFile f(CTL_CONF);
    if (!f.open(QIODevice::ReadOnly)) return cfg;
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString s = line.trimmed();
        if (s.isEmpty() || s.startsWith(QLatin1Char('#')) || s.startsWith(QLatin1Char('['))) continue;
        const int eq = s.indexOf(QLatin1Char('='));
        if (eq > 0) {
            cfg.insert(s.left(eq).trimmed(), s.mid(eq + 1).trimmed());
        }
    }
    return cfg;
}

CommandResult setCtlConfig(const QMap<QString, QString> &updates)
{
    CommandResult r;
    QStringList lines;
    QFile f(CTL_CONF);
    if (f.open(QIODevice::ReadOnly)) {
        lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    } else {
        lines = {QStringLiteral("[global]")};
    }

    bool inGlobal = false;
    QSet<QString> updated;
    QStringList out;
    for (const QString &line : lines) {
        const QString s = line.trimmed();
        if (s.startsWith(QLatin1Char('['))) {
            inGlobal = (s == QLatin1String("[global]"));
            out << line;
            continue;
        }
        if (inGlobal && s.contains(QLatin1Char('=')) && !s.startsWith(QLatin1Char('#'))) {
            const QString k = s.left(s.indexOf(QLatin1Char('='))).trimmed();
            if (updates.contains(k)) {
                out << (k + QLatin1Char('=') + updates.value(k));
                updated.insert(k);
                continue;
            }
        }
        out << line;
    }
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        if (!updated.contains(it.key())) {
            out << (it.key() + QLatin1Char('=') + it.value());
        }
    }

    const QString tmp = QDir::tempPath() + QStringLiteral("/.panshi_conf_%1").arg(getpid());
    {
        QFile tf(tmp);
        if (!tf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            r.errorMessage = QObject::tr("无法写入临时配置文件");
            return r;
        }
        tf.write(out.join(QLatin1Char('\n')).toUtf8());
        tf.write("\n");
    }

    // 以 root 安装到目标位置
    r = runProcess(QStringLiteral("install"),
                   QStringList() << "-o" << "root" << "-g" << "root" << "-m" << "0644"
                                 << tmp << CTL_CONF,
                   30000, true);
    QFile::remove(tmp);
    return r;
}

// ----------------------------------------------------------------------
// 系统更新
// ----------------------------------------------------------------------
CommandResult checkUpgrade()
{
    return runCtl(QStringList() << "upgrade" << "check" << "-j", true, 300000);
}

CommandResult runUpgrade()
{
    return runCtl(QStringList() << "upgrade", true, 7200000);
}

CommandResult cleanUpgradeCache(bool allCache)
{
    QStringList args = {QStringLiteral("upgrade"), QStringLiteral("clean")};
    if (allCache) args << QStringLiteral("-a");
    return runCtl(args, true, 300000);
}

// ----------------------------------------------------------------------
// 回滚状态管理
// ----------------------------------------------------------------------
RollbackState getRollbackState()
{
    RollbackState st;
    QFile f(STATE_FILE);
    if (!f.open(QIODevice::ReadOnly)) return st;
    const QJsonObject o = parseJson(QString::fromUtf8(f.readAll()));
    if (o.isEmpty()) return st;
    st.rollbackTarget = o.value(QStringLiteral("rollback_target")).toString();
    st.rollbackTargetName = o.value(QStringLiteral("rollback_target_name")).toString();
    st.backupSnapshotId = o.value(QStringLiteral("backup_snapshot_id")).toString();
    st.backupSnapshotName = o.value(QStringLiteral("backup_snapshot_name")).toString();
    st.originalBooted = o.value(QStringLiteral("original_booted")).toString();
    st.strategy = o.value(QStringLiteral("strategy")).toString();
    st.status = o.value(QStringLiteral("status")).toString();
    st.rebootCount = o.value(QStringLiteral("reboot_count")).toInt();
    st.rebootLimit = o.value(QStringLiteral("reboot_limit")).toInt(3);
    st.createdAt = o.value(QStringLiteral("created_at")).toVariant().toLongLong();
    st.valid = true;
    return st;
}

CommandResult saveRollbackState(const QJsonObject &state)
{
    const QString tmp = QDir::tempPath() + QStringLiteral("/.panshi_state_%1").arg(getpid());
    {
        QFile tf(tmp);
        if (!tf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            CommandResult r; r.errorMessage = QObject::tr("无法写入临时状态文件");
            return r;
        }
        tf.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
    }
    CommandResult r = runProcess(QStringLiteral("install"),
                                 QStringList() << "-o" << "root" << "-g" << "root" << "-m" << "0644"
                                               << tmp << STATE_FILE,
                                 30000, true);
    QFile::remove(tmp);
    return r;
}

void clearRollbackState()
{
    runProcess(QStringLiteral("rm"), QStringList() << "-f" << STATE_FILE, 20000, true);
}

QString getBootedDeployId()
{
    const CommandResult r = runCtl(QStringList() << "admin" << "status" << "-j", false, 60000);
    if (!r.ok) return QString();
    const QJsonArray dl = parseJson(r.stdoutText).value(QStringLiteral("data")).toObject()
                              .value(QStringLiteral("deploy_list")).toArray();
    for (const QJsonValue &v : dl) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("IsBooted")).toBool()) {
            return o.value(QStringLiteral("CommitIDIdx")).toString();
        }
    }
    return QString();
}

CommandResult installRollbackCleanup()
{
    // systemd 服务 + timer + 开机计数脚本
    CommandResult r;
    const QString script = QStringLiteral(
        "#!/bin/sh\n"
        "# panshi rollback cleanup boot counter\n"
        "if [ -e /run/panshi-rollback-counter-run ]; then exit 0; fi\n"
        "touch /run/panshi-rollback-counter-run\n"
        "test -f %1 || exit 0\n"
        "PY=/usr/bin/python3\n"
        "if [ -x $PY ]; then $PY -c \"import json,os,sys;f='%1';d=json.load(open(f));d['reboot_count']=d.get('reboot_count',0)+1;l=d.get('reboot_limit',3);b=d.get('backup_snapshot_id','');st=d.get('status','');\n"
        "import subprocess\n"
        "if st=='active' and d['reboot_count']>=l and b:\n"
        " p=subprocess.run(['/usr/sbin/deepin-immutable-ctl','snapshot','delete',b],capture_output=True,text=True)\n"
        " d['status']='done' if p.returncode==0 else 'failed'\n"
        "json.dump(d,open(f,'w'),ensure_ascii=False,indent=2)\" ; fi\n"
    ).arg(STATE_FILE);

    const QString scriptPath = QStringLiteral("/usr/libexec/panshi-manager/reboot-counter.sh");
    const QString servicePath = QStringLiteral("/usr/lib/systemd/system/panshi-rollback-cleanup.service");
    const QString timerPath = QStringLiteral("/usr/lib/systemd/system/panshi-rollback-cleanup.timer");

    const QString service = QStringLiteral(
        "[Unit]\nDescription=Clean up rollback backup snapshot after N successful reboots\n"
        "ConditionPathExists=%1\n\n"
        "[Service]\nType=oneshot\nExecStart=/usr/libexec/panshi-manager/reboot-counter.sh\n"
        "Nice=19\nIOSchedulingClass=idle\nRestart=no\n").arg(STATE_FILE);
    const QString timer = QStringLiteral(
        "[Unit]\nDescription=Clean up rollback backup snapshot after N successful reboots\n\n"
        "[Timer]\nOnBootSec=3min\n\n[Install]\nWantedBy=timers.target\n");

    const QString tmp = QDir::tempPath() + QStringLiteral("/.panshi_inst_%1").arg(getpid());
    auto writeRoot = [&](const QString &content, const QString &dest, const QString &mode) -> bool {
        QFile tf(tmp);
        if (!tf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        tf.write(content.toUtf8());
        tf.close();
        CommandResult rr = runProcess(QStringLiteral("install"),
                                      QStringList() << "-o" << "root" << "-g" << "root"
                                                    << "-m" << mode << tmp << dest,
                                      30000, true);
        return rr.ok;
    };
    bool ok = writeRoot(script, scriptPath, QStringLiteral("0755"));
    ok = writeRoot(service, servicePath, QStringLiteral("0644")) && ok;
    ok = writeRoot(timer, timerPath, QStringLiteral("0644")) && ok;
    QFile::remove(tmp);
    if (!ok) {
        r.errorMessage = QObject::tr("安装 systemd 服务失败");
        return r;
    }
    runProcess(QStringLiteral("systemctl"), QStringList() << "daemon-reload", 30000, true);
    runProcess(QStringLiteral("systemctl"), QStringList() << "enable" << "--now"
                << "panshi-rollback-cleanup.timer", 60000, true);
    r.ok = true;
    return r;
}

CommandResult uninstallRollbackCleanup()
{
    CommandResult r;
    runProcess(QStringLiteral("systemctl"), QStringList() << "disable" << "--now"
                << "panshi-rollback-cleanup.timer", 60000, true);
    const CommandResult rm = runProcess(QStringLiteral("rm"),
        QStringList() << "-f" << QStringLiteral("/usr/lib/systemd/system/panshi-rollback-cleanup.service")
                      << QStringLiteral("/usr/lib/systemd/system/panshi-rollback-cleanup.timer")
                      << QStringLiteral("/usr/libexec/panshi-manager/reboot-counter.sh"),
        20000, true);
    runProcess(QStringLiteral("systemctl"), QStringList() << "daemon-reload", 30000, true);
    r = rm;
    return r;
}

bool rollbackCleanupInstalled()
{
    const CommandResult r = runProcess(QStringLiteral("systemctl"),
        QStringList() << "is-enabled" << "panshi-rollback-cleanup.timer", 20000, true);
    return r.ok;
}

// ----------------------------------------------------------------------
// 工具函数
// ----------------------------------------------------------------------
QString formatSize(qint64 bytes)
{
    double num = static_cast<double>(bytes);
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    while (qAbs(num) >= 1024.0 && idx < 4) {
        num /= 1024.0;
        ++idx;
    }
    if (idx == 0) return QString::number(static_cast<qint64>(num)) + QStringLiteral(" B");
    return QString::number(num, 'f', 2) + QStringLiteral(" ") + QString::fromLatin1(units[idx]);
}

QString formatTime(qint64 ts)
{
    if (ts <= 0) return QStringLiteral("-");
    const std::time_t t = static_cast<std::time_t>(ts);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return QString::fromUtf8(buf);
}

QString jsonField(const QJsonObject &obj, const QStringList &names, const QString &def)
{
    for (const QString &n : names) {
        if (obj.contains(n)) return obj.value(n).toString();
    }
    return def;
}

} // namespace Panshi
