// SPDX-License-Identifier: GPL-3.0-or-later
#include "scanner.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSet>
#include <algorithm>
#include <unistd.h>

namespace Panshi {

namespace {
constexpr int kProgressEmitInterval = 20;   // 每解析 N 行发一次进度
} // namespace

DiskScanner::DiskScanner(QObject *parent)
    : QObject(parent)
{
}

DiskScanner::~DiskScanner()
{
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
        m_proc->deleteLater();
    }
}

bool DiskScanner::startTopLevelScan()
{
    if (isBusy())
        return false;
    m_drillMode = false;
    m_drillDir.clear();
    // 口径修正（阶段 3-9）：顶层扫描加 -x（不跨文件系统）。
    //
    // 背景：旧实现不加 -x，du / 会钻入 /media、/persistent、/var、/home 等
    // **独立分区挂载点**，把整块分区的占用计入顶层目录（本机实测 /media 被计入
    // 约 255GB、/persistent 约 18GB），而页面分母 record.total 取自 df /
    // （仅根分区约 8.5GB）→ 「磁盘构成 / 占用排行」的占比出现 >100% 的假值
    // （用户实测 /media 2913.3%）。
    //
    // -x 之后顶层只统计根分区自身（本机 ostree 布局 = /sysroot + /ostree），
    // 与页面「已用 / 总容量」同一口径，占比才是真占比。
    // deepin btrfs 子卷布局下 /var /home /usr 子卷与根同 st_dev，-x 不会漏掉它们；
    // 伪文件系统（/proc /sys /dev /run /tmp）保留显式排除，语义更清晰。
    const QStringList duArgs = {QStringLiteral("du"), QStringLiteral("-xk"),
                                QStringLiteral("--max-depth=1"),
                                QStringLiteral("--exclude=/proc"), QStringLiteral("--exclude=/sys"),
                                QStringLiteral("--exclude=/dev"), QStringLiteral("--exclude=/run"),
                                QStringLiteral("--exclude=/tmp"), QStringLiteral("/")};
    startDu(duArgs, false, QString());
    return true;
}

bool DiskScanner::drillInto(const QString &dir)
{
    if (isBusy())
        return false;
    m_drillMode = true;
    m_drillDir = dir;
    const QStringList duArgs = {QStringLiteral("du"), QStringLiteral("-ak"),
                                QStringLiteral("--max-depth=1"),
                                QStringLiteral("--exclude=/proc"), QStringLiteral("--exclude=/sys"),
                                QStringLiteral("--exclude=/dev"), QStringLiteral("--exclude=/run"),
                                QStringLiteral("--exclude=/tmp"), dir};
    startDu(duArgs, true, dir);
    return true;
}

void DiskScanner::startDu(const QStringList &duArgs, bool drillMode, const QString &drillDir)
{
    Q_UNUSED(drillMode);
    Q_UNUSED(drillDir);
    m_cancelled = false;
    m_degradedRoot = false;
    m_items.clear();
    m_buffer.clear();
    m_lineCount = 0;
    m_elapsed.start();

    // nice -n 19 ionice -c3 du ...
    QStringList niceArgs = {QStringLiteral("-n"), QStringLiteral("19"),
                            QStringLiteral("ionice"), QStringLiteral("-c3")};
    niceArgs += duArgs;

    QStringList launchArgs;
    QString program;
    if (m_asRoot && geteuid() != 0) {
        program = QStringLiteral("sudo");
        launchArgs = {QStringLiteral("-n")};
        launchArgs += niceArgs;
    } else {
        program = QStringLiteral("nice");
        launchArgs = niceArgs;
    }
    m_baseArgs = niceArgs;

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &DiskScanner::onReadyRead);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &DiskScanner::onFinished);
    m_proc->start(program, launchArgs);
}

void DiskScanner::cancel()
{
    m_cancelled = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(3000);
    }
}

bool DiskScanner::dfRoot(qint64 *capacity, qint64 *used, QString *err)
{
    QProcess proc;
    proc.start(QStringLiteral("df"), {QStringLiteral("-B1"), QStringLiteral("/")});
    if (!proc.waitForStarted(5000)) {
        if (err) *err = QObject::tr("无法启动 df 进程");
        return false;
    }
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        if (err) *err = QObject::tr("df 执行超时");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (err) *err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return false;
    }
    const QList<QByteArray> lines = proc.readAllStandardOutput().split('\n');
    for (const QByteArray &raw : lines) {
        const QList<QByteArray> cols = raw.trimmed().split(' ');
        QList<QByteArray> words;
        for (const QByteArray &c : cols)
            if (!c.isEmpty())
                words.append(c);
        if (words.size() >= 3 && words.at(0) != QByteArray("Filesystem")) {
            bool ok1 = false, ok2 = false;
            const qint64 blocks = words.at(1).toLongLong(&ok1);
            const qint64 usedB = words.at(2).toLongLong(&ok2);
            if (ok1 && ok2) {
                if (capacity) *capacity = blocks;
                if (used) *used = usedB;
                return true;
            }
        }
    }
    if (err) *err = QObject::tr("无法解析 df 输出");
    return false;
}

QList<DiskMountInfo> DiskScanner::listOtherPartitions()
{
    QList<DiskMountInfo> out;
    QFile f(QStringLiteral("/proc/self/mounts"));
    if (!f.open(QIODevice::ReadOnly))
        return out;

    // 1) 读挂载表：收集根分区之外的真实块设备挂载点（按设备去重，留主挂载点）
    const QStringList pseudoFs = {
        QStringLiteral("proc"), QStringLiteral("sysfs"), QStringLiteral("tmpfs"),
        QStringLiteral("devpts"), QStringLiteral("devtmpfs"), QStringLiteral("cgroup"),
        QStringLiteral("cgroup2"), QStringLiteral("efivarfs"), QStringLiteral("selinuxfs"),
        QStringLiteral("debugfs"), QStringLiteral("tracefs"), QStringLiteral("securityfs"),
        QStringLiteral("pstore"), QStringLiteral("bpf"), QStringLiteral("ramfs"),
        QStringLiteral("mqueue"), QStringLiteral("hugetlbfs"), QStringLiteral("binfmt_misc"),
        QStringLiteral("fusectl"), QStringLiteral("configfs"), QStringLiteral("overlay"),
        QStringLiteral("autofs"), QStringLiteral("rpc_pipefs"), QStringLiteral("nsfs"),
        QStringLiteral("squashfs"), QStringLiteral("iso9660"),
    };
    QString rootDev;
    QList<DiskMountInfo> mounts;
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &raw : lines) {
        const QList<QByteArray> cols = raw.split(' ');
        if (cols.size() < 3)
            continue;
        const QString dev = QString::fromUtf8(cols.at(0));
        const QString mp = QString::fromUtf8(cols.at(1));
        const QString fs = QString::fromUtf8(cols.at(2));
        if (dev.isEmpty() || mp.isEmpty())
            continue;
        if (mp == QStringLiteral("/")) {          // 记录系统盘设备（用于排除）
            rootDev = dev;
            continue;
        }
        if (pseudoFs.contains(fs))
            continue;
        if (!dev.startsWith(QLatin1String("/dev/")))   // 只留真实块设备
            continue;
        if (dev == rootDev)                            // 排除系统盘自身
            continue;
        // 按设备去重：同一分区的多个挂载点（如 /persistent /var /home）只留首个
        bool seen = false;
        for (const DiskMountInfo &m : mounts) {
            if (m.device == dev) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;
        DiskMountInfo mi;
        mi.device = dev;
        mi.mountPoint = mp;
        mi.fsType = fs;
        mounts.append(mi);
    }
    if (mounts.isEmpty())
        return out;

    // 2) 一次 df -B1 批量取各挂载点容量/已用（比逐个起进程快）
    QStringList args = {QStringLiteral("-B1")};
    for (const DiskMountInfo &m : mounts)
        args << m.mountPoint;
    QProcess proc;
    proc.start(QStringLiteral("df"), args);
    if (!proc.waitForStarted(5000) || !proc.waitForFinished(15000))
        return out;
    // df -B1 多参数输出：Filesystem 1B-blocks Used Available Capacity Mounted on
    const QList<QByteArray> dflines = proc.readAllStandardOutput().split('\n');
    for (const QByteArray &raw : dflines) {
        const QList<QByteArray> cols = raw.trimmed().split(' ');
        if (cols.size() < 6 || cols.at(0) == QByteArray("Filesystem"))
            continue;
        bool ok1 = false, ok2 = false;
        const qint64 blocks = cols.at(1).toLongLong(&ok1);
        const qint64 used = cols.at(2).toLongLong(&ok2);
        if (!ok1 || !ok2)
            continue;
        const QString dev = QString::fromUtf8(cols.at(0));
        for (DiskMountInfo &m : mounts) {
            if (m.device == dev) {
                m.capacity = blocks;
                m.used = used;
                out.append(m);
                break;
            }
        }
    }

    // 3) 按已用降序（大分区在前）
    std::sort(out.begin(), out.end(),
              [](const DiskMountInfo &a, const DiskMountInfo &b) { return a.used > b.used; });
    return out;
}

// ----------------------------------------------------------------------
// 流式解析
// ----------------------------------------------------------------------
void DiskScanner::parseLine(const QByteArray &line)
{
    QByteArray l = line;
    if (l.endsWith('\r'))
        l.chop(1);
    if (l.trimmed().isEmpty())
        return;
    const int tab = l.indexOf('\t');
    if (tab <= 0)
        return;
    bool ok = false;
    const qint64 sizeKB = l.left(tab).toLongLong(&ok);
    if (!ok)
        return;
    const QString path = QString::fromUtf8(l.mid(tab + 1));
    if (m_drillMode) {
        if (path == m_drillDir || path == m_drillDir + QStringLiteral("/"))
            return; // 跳过父目录自身
    } else {
        if (path == QStringLiteral("/") || path == QStringLiteral("//"))
            return; // 跳过 / 的总计行
    }
    DiskItem it;
    it.path = path;
    it.size = sizeKB * 1024;          // du -k → 字节
    it.isDir = true;
    if (m_drillMode)
        it.isDir = QFileInfo(path).isDir(); // -a 模式含文件，逐一判定
    m_items.append(it);
}

void DiskScanner::onReadyRead()
{
    if (!m_proc)
        return;
    m_buffer += m_proc->readAllStandardOutput();
    const QList<QByteArray> lines = m_buffer.split('\n');
    m_buffer.clear();
    for (int i = 0; i < lines.size() - 1; ++i) {
        parseLine(lines[i]);
        ++m_lineCount;
        if (m_lineCount % kProgressEmitInterval == 0) {
            const QString cur = m_items.isEmpty() ? QString() : m_items.last().path;
            emit progress(m_items.size(), cur);
        }
    }
    if (!lines.isEmpty())
        m_buffer = lines.last();
}

void DiskScanner::parseBufferRemainder()
{
    if (m_buffer.isEmpty())
        return;
    const QList<QByteArray> lines = m_buffer.split('\n');
    m_buffer.clear();
    for (const QByteArray &l : lines)
        parseLine(l);
}

void DiskScanner::onFinished(int code, QProcess::ExitStatus status)
{
    QProcess *proc = m_proc;
    m_proc = nullptr;
    const bool normal = (status == QProcess::NormalExit && code == 0);
    if (m_cancelled) {
        proc->deleteLater();
        emit scanFinished(false, tr("扫描已取消"));
        return;
    }
    // sudo -n 需要密码时回退 pkexec（仅 AllowPrompt 模式）
    if (m_asRoot && geteuid() != 0 && !normal && m_policy == RootPolicy::AllowPrompt
        && !m_degradedRoot) {
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (err.contains(QStringLiteral("password"))) {
            proc->deleteLater();
            m_items.clear();
            m_buffer.clear();
            m_lineCount = 0;
            m_cancelled = false;
            auto *p = new QProcess(this);
            m_proc = p;
            connect(p, &QProcess::readyReadStandardOutput, this, &DiskScanner::onReadyRead);
            connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &DiskScanner::onFinished);
            p->start(QStringLiteral("pkexec"), m_baseArgs);
            return;
        }
    }
    // 自动降级：sudo -n 失败（未配置免密）→ 普通用户扫描，数据标记不完整
    if (m_asRoot && geteuid() != 0 && !normal && !m_degradedRoot) {
        m_degradedRoot = true;
        proc->deleteLater();
        m_items.clear();
        m_buffer.clear();
        m_lineCount = 0;
        m_cancelled = false;
        auto *p = new QProcess(this);
        m_proc = p;
        connect(p, &QProcess::readyReadStandardOutput, this, &DiskScanner::onReadyRead);
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DiskScanner::onFinished);
        p->start(QStringLiteral("nice"), m_baseArgs);
        return;
    }
    // 解析缓冲区剩余
    parseBufferRemainder();
    emit progress(m_items.size(), m_items.isEmpty() ? QString() : m_items.last().path);
    const bool complete = normal && !m_degradedRoot;
    if (m_drillMode)
        emit drillFinished(m_drillDir, m_items);
    else
        emit topLevelFinished(m_items, complete);
    emit scanFinished(normal, normal ? QString() : tr("扫描未完整完成"));
    proc->deleteLater();
}

} // namespace Panshi
