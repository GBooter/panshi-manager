// SPDX-License-Identifier: GPL-3.0-or-later
//
// 闪屏根因回归探针（2026-10-04）
//
// 缺陷：磁盘监控服务未开启时，disk-history 目录里没有任何日记录，
//       loadIndex() → rebuildIndex() 仍会把一份**空** index.json 写盘。
//       写盘改变 mtime → QFileSystemWatcher::directoryChanged →
//       onHistoryDirChanged → 400ms 防抖 → requestRange → onRangeLoaded →
//       rebuildSpikeSet → loadIndex → …… 死循环，每轮都 setLoading(true)，
//       表现为「数据区域一直闪屏」。
//
// 修法：rebuildIndex() 里 summaries 为空时不再回写 index.json。
//
// 本探针直接断言「反复 loadIndex() 不产生目录内容变化」，
// 即 directoryChanged 不会再被触发 —— 这是闪屏的必要条件。
// 用法：./probe_history_flicker   （退出码 0 = 通过）

#include "history.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdlib>

using namespace Panshi;

static QString g_dir;

/**
 * 断言失败：必须**立刻**终止进程。
 *
 * ⚠️ 这里刻意不用 QCoreApplication::exit(1)：它只设置事件循环的返回值，
 * 而本探针是纯同步 main()、没有跑事件循环，exit(1) 会被后面的
 * `return 0` 覆盖掉 → 探针在缺陷存在时依然报 PASS（假阴性）。
 * 这类「验证脚本自己骗自己」比没写脚本更危险，故用 std::exit。
 */
static void fail(const QString &msg)
{
    QTextStream(stdout) << "FAIL: " << msg << "\n";
    std::exit(1);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("deepin"));
    QCoreApplication::setApplicationName(QStringLiteral("panshi-manager"));

    g_dir = DiskHistoryStore::historyDir();
    QTextStream out(stdout);

    // 模拟「监控未开启」：清空目录，不留任何日记录
    QDir d(g_dir);
    for (const QString &f : d.entryList(QDir::Files))
        d.remove(f);
    out << "historyDir = " << g_dir << "\n";
    out << "初始文件数 = " << d.entryList(QDir::Files).size() << "\n";
    out.flush();

    // 断言 1：第一次 loadIndex 后不应凭空写出 index.json
    DiskHistoryStore store;
    const DiskHistoryIndex idx1 = store.loadIndex();
    if (!idx1.days.isEmpty()) {
        fail(QStringLiteral("前提不成立：清空后 loadIndex 仍读到 %1 条日记录")
                 .arg(idx1.days.size()));
    }
    const QStringList after1 = d.entryList(QDir::Files);
    out << "loadIndex() x1 后文件 = " << (after1.isEmpty() ? QStringLiteral("(无)")
                                                             : after1.join(',')) << "\n";
    if (after1.contains(QStringLiteral("index.json"))) {
        fail(QStringLiteral("目录为空时仍写出了 index.json —— 闪屏死循环未修复"));
    }

    // 断言 2：连续多次 loadIndex，目录内容必须保持稳定（mtime 不再变化）
    // 记录第一次之后的状态，重复 5 轮
    for (int i = 0; i < 5; ++i)
        store.loadIndex();
    const QStringList after2 = d.entryList(QDir::Files);
    if (after2 != after1) {
        fail(QStringLiteral("重复 loadIndex 改变了目录内容：%1 → %2")
                 .arg(after1.join(','), after2.join(',')));
    }
    out << "loadIndex() x6 后文件数 = " << after2.size() << "（稳定）\n";

    // 断言 3（反向对照）：有日记录时 index.json **必须**被写出。
    // 这是防止「修过头」—— 若空索引不写、有数据的也不写，索引就彻底失效了。
    DiskDayRecord rec;
    rec.date = QDate(2026, 10, 1);
    rec.total = 1024LL * 1024 * 100;
    rec.capacity = 1024LL * 1024 * 512;
    DiskItem it;
    it.path = QStringLiteral("/var");
    it.size = 1024LL * 1024 * 40;
    it.isDir = true;
    rec.topLevel.append(it);
    rec.complete = true;
    if (!store.saveDayRecord(rec))
        fail(QStringLiteral("写入测试日记录失败"));

    const DiskHistoryIndex idx2 = store.loadIndex();
    if (idx2.days.size() != 1) {
        fail(QStringLiteral("有日记录时 loadIndex 应读到 1 条，实际 %1 条 —— 索引被修过头了")
                 .arg(idx2.days.size()));
    }
    const QStringList after3 = d.entryList(QDir::Files);
    out << "写入 1 条日记录后 loadIndex 读到 " << idx2.days.size()
        << " 天，index.json "
        << (after3.contains(QStringLiteral("index.json")) ? QStringLiteral("已生成 ✓")
                                                         : QStringLiteral("缺失 ✗"))
        << "\n";
    if (!after3.contains(QStringLiteral("index.json"))) {
        fail(QStringLiteral("有日记录时未写出 index.json —— 修过头，索引功能失效"));
    }

    // 清理测试数据
    d.removeRecursively();
    QDir().mkpath(g_dir);

    out << "\nPASS: 空目录不写盘（死循环断开）+ 有数据仍写盘（索引未失效）\n";
    out.flush();
    return 0;
}
