// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "disktypes.h"

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>

namespace Panshi {

/**
 * 磁盘扫描器：三层采样（df 轮询 / 事件触发 du 下钻 / 每日完整 du）。
 * - QProcess 流式解析 du 输出，可实时进度与取消；
 * - 自动扫描仅用 sudo -n（免密），不弹窗；AllowPrompt 模式下失败回退 pkexec；
 * - 所有 du 以 nice -n 19 + ionice -c3 低优先级执行，不拖慢系统。
 */
class DiskScanner : public QObject
{
    Q_OBJECT
public:
    enum class RootPolicy { AutoNoPrompt, AllowPrompt };

    explicit DiskScanner(QObject *parent = nullptr);
    ~DiskScanner() override;

    bool isBusy() const { return m_proc != nullptr; }
    void setRootPolicy(RootPolicy p) { m_policy = p; }
    void setAsRoot(bool asRoot) { m_asRoot = asRoot; }

    // 扫描 / 下的一级目录（du -xsk --max-depth=1 /）
    bool startTopLevelScan();
    // 下钻某目录的直接子项（目录与文件）
    bool drillInto(const QString &dir);
    void cancel();

    // df -B1 / 快速总量查询（无需 root）
    static bool dfRoot(qint64 *capacity, qint64 *used, QString *err = nullptr);

    /**
     * 枚举根分区之外的其它真实分区（全盘视角）。
     *
     * 数据来源 /proc/self/mounts + 一次批量 df -B1：
     *  - 过滤伪文件系统（proc/sysfs/tmpfs/overlay/cgroup 等）与非常规设备；
     *  - 按**设备**去重（同一分区多个挂载点只留主挂载点，避免 /var /home
     *    /usr 等子挂载点重复计数）；
     *  - 排除系统盘（根分区）自身 —— 它由「磁盘构成」单独展示。
     */
    static QList<Panshi::DiskMountInfo> listOtherPartitions();

signals:
    void progress(int itemsDone, const QString &currentPath);
    void topLevelFinished(const QList<DiskItem> &items, bool complete);
    void drillFinished(const QString &dir, const QList<DiskItem> &items);
    void scanFinished(bool ok, const QString &error);

private:
    void startDu(const QStringList &duArgs, bool drillMode, const QString &drillDir);
    void onReadyRead();
    void onFinished(int code, QProcess::ExitStatus status);
    void parseLine(const QByteArray &line);
    void parseBufferRemainder();
    void restartAsPkexec(const QStringList &args);

    QProcess *m_proc = nullptr;
    QStringList m_baseArgs;
    bool m_asRoot = false;
    RootPolicy m_policy = RootPolicy::AutoNoPrompt;
    bool m_drillMode = false;
    QString m_drillDir;
    QList<DiskItem> m_items;
    QByteArray m_buffer;
    int m_lineCount = 0;
    bool m_cancelled = false;
    bool m_degradedRoot = false;   // sudo -n 失败已回退普通用户扫描（数据不完整）
    QElapsedTimer m_elapsed;
};

} // namespace Panshi
