// SPDX-License-Identifier: GPL-3.0-or-later
//
// 磁盘页「分析区整合 + 导航 + 告警关闭」离屏验证探针 panshi-disknavprobe
//（开发期专用，不安装）
//
// 背景（2026-10-10）：原「磁盘构成 / 增长详情 / 占用排行」是三套列表 + 三套交互，
//   数据源重复（构成 = 排行前 5 + 其他）；「返回系统盘」按钮又埋在时间线卡里，
//   离"点目录"的位置很远。现整合为单一「磁盘分析」区 + 三视角（构成/增长/排行）
//   共用一张表；导航改为「常驻返回按钮 + 面包屑」贴着操作对象；告警支持手动关闭，
//   关闭后收进「告警历史」弹窗。
//
// 断言（黑盒：只通过控件树找控件、调 click() / 发信号，不依赖私有成员）：
//   N1 分析区存在 3 个视角按钮（构成 / 增长 / 排行），初始选中「构成」；
//   N2 「排行」视角下表格能列出测试记录的顶层目录；
//   N3 双击目录行下钻后：面包屑出现末段按钮，且「返回系统盘」变为可用
//      —— 修复前 updateAnalysis() 不刷新上下文条，按钮会一直灰着（缺陷点）；
//   N4 点「返回系统盘」后：面包屑收回只剩「系统盘」，按钮重新变灰；
//   N5 切到「增长」视角后，视角说明文案随之变为增长口径；
//   N6 告警：有 alert.json 时横幅可见；点关闭后横幅收起，
//      且 alert-ui-state.json 里落了 dismissed 记录（关闭 = 视图状态，不删数据）。
//
// 用法：
//   QT_QPA_PLATFORM=offscreen ./panshi-disknavprobe [--png .tmp/disk-nav]
//   数据目录用 XDG_DATA_HOME 隔离（探针内自造记录，不碰真实数据）。
// 退出码 = 失败断言数 > 0 ? 1 : 0
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTextStream>
#include <QThread>
#include <cstdlib>

#include <DPushButton>

#include "disk/history.h"
#include "mainwindow.h"

using namespace Panshi;

// 演示用下钻目录。注意：DiskScanner::drillInto 显式 --exclude=/tmp，
// 所以不能用 /tmp；/var/tmp 不在排除列表内。
static const char *kDemoRoot = "/var/tmp/panshi-disknav-demo";

static int g_fails = 0;

static void fail(const QString &msg)
{
    QTextStream(stdout) << "FAIL: " << msg << "\n";
    ++g_fails;
}

static void pass(const QString &msg)
{
    QTextStream(stdout) << "ok  : " << msg << "\n";
}

/** 静置若干轮事件循环（离屏下异步 QProcess 需要泵事件才会推进） */
static void settle(int rounds = 8)
{
    for (int i = 0; i < rounds; ++i)
        QCoreApplication::processEvents();
}

template <typename T>
static QList<T> visible(QWidget *root)
{
    QList<T> out;
    for (T w : root->findChildren<T>()) {
        if (w && w->isVisible())
            out.append(w);
    }
    return out;
}

/**
 * 按文本找按钮（只取可见的）。
 *
 * 注意：这里必须用 QAbstractButton 而不是 DPushButton —— 视角切换用的是
 * Dtk::Widget::DButtonBoxButton，普通操作按钮是 QPushButton（Ui::button），
 * 两者都不派生自 DPushButton，按 DPushButton 查会全部落空（实测踩过）。
 */
static QAbstractButton *findButton(QWidget *root, const QString &text)
{
    for (QAbstractButton *b : visible<QAbstractButton *>(root)) {
        if (b->text() == text)
            return b;
    }
    return nullptr;
}

/** 找分析区那张表（用无障碍名定位，比索引稳） */
static QTableWidget *findAnalysisTable(QWidget *root)
{
    for (QTableWidget *t : visible<QTableWidget *>(root)) {
        if (t->accessibleName() == QObject::tr("磁盘分析表"))
            return t;
    }
    return nullptr;
}

/** 面包屑：以「系统盘」按钮的父容器为界，取其中所有可见按钮文本 */
static QStringList breadcrumb(QWidget *root)
{
    QAbstractButton *rootBtn = findButton(root, QObject::tr("系统盘"));
    if (!rootBtn || !rootBtn->parentWidget())
        return {};
    QStringList out;
    for (QAbstractButton *b : visible<QAbstractButton *>(rootBtn->parentWidget()))
        out.append(b->text());
    return out;
}

int main(int argc, char **argv)
{
    // 数据隔离必须在 QApplication 构造前完成
    const QString dataHome = QDir::currentPath() + "/.tmp/disknav-xdgdata";
    // 每轮清空：alert-ui-state.json 是「用户关过哪些告警」的持久记录，
    // 不重置的话第二轮起横幅就再也不弹（正是预期行为，但会让探针不可重复跑）。
    QDir(dataHome).removeRecursively();
    QDir().mkpath(dataHome);
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("deepin"));
    QCoreApplication::setApplicationName(QStringLiteral("panshi-manager"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption pngOpt(QStringList{"png"}, "输出 PNG 前缀", "prefix",
                              QStringLiteral(".tmp/disk-nav"));
    parser.addOption(pngOpt);
    parser.process(a);
    const QString pngPrefix = parser.value(pngOpt);

    // ---------- 造演示目录与 3 天记录 ----------
    QDir demo(kDemoRoot);
    demo.removeRecursively();
    QDir().mkpath(QString(kDemoRoot) + "/alpha");
    QDir().mkpath(QString(kDemoRoot) + "/beta");
    {
        QFile f1(QString(kDemoRoot) + "/alpha/a.bin");
        if (f1.open(QIODevice::WriteOnly))
            f1.write(QByteArray(64 * 1024, 'a'));
        QFile f2(QString(kDemoRoot) + "/beta/b.bin");
        if (f2.open(QIODevice::WriteOnly))
            f2.write(QByteArray(32 * 1024, 'b'));
    }

    DiskHistoryStore store;
    const qint64 GB = 1024LL * 1024 * 1024;
    const qint64 totals[3] = {20 * GB, 20 * GB + 30LL * 1024 * 1024,
                              20 * GB + 1480LL * 1024 * 1024};
    for (int i = 2; i >= 0; --i) {
        DiskDayRecord rec;
        rec.date = QDate::currentDate().addDays(-i);
        rec.total = totals[2 - i];
        rec.capacity = 40 * GB;
        DiskItem it;
        it.path = QString::fromLatin1(kDemoRoot);
        it.size = totals[2 - i] / 2;
        it.isDir = true;
        rec.topLevel.append(it);
        rec.complete = true;
        if (!store.saveDayRecord(rec))
            fail(QStringLiteral("写入第 %1 天测试记录失败").arg(i));
    }

    // 造一条「告警」：刷新时应弹出横幅
    const QString histDir = DiskHistoryStore::historyDir();
    {
        QJsonArray spikes;
        QJsonObject s;
        s.insert(QStringLiteral("date"), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
        s.insert(QStringLiteral("causeDir"), QString::fromLatin1(kDemoRoot));
        s.insert(QStringLiteral("growth"), 1480.0 * 1024 * 1024);
        spikes.append(s);
        QJsonObject root;
        root.insert(QStringLiteral("spikes"), spikes);
        QFile f(histDir + QStringLiteral("/alert.json"));
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    MainWindow w;
    w.resize(1080, 1200);
    w.show();
    settle(6);
    w.gotoPage(PageId::DiskUsage);
    settle(12);
    // 离屏首渲染假象防线：先丢弃式 grab 再真 grab（见跨会话教训 5）
    w.grab();
    settle(4);

    // ---------- N1：三视角按钮存在且默认「构成」 ----------
    QAbstractButton *bCompo = findButton(&w, QObject::tr("构成"));
    QAbstractButton *bGrowth = findButton(&w, QObject::tr("增长"));
    QAbstractButton *bRank = findButton(&w, QObject::tr("排行"));
    if (!bCompo || !bGrowth || !bRank) {
        fail(QStringLiteral("N1 失败：分析区三视角按钮不全（构成=%1 增长=%2 排行=%3）")
                 .arg(bCompo != nullptr)
                 .arg(bGrowth != nullptr)
                 .arg(bRank != nullptr));
    } else if (!bCompo->isChecked()) {
        fail(QStringLiteral("N1 失败：初始视角应为「构成」，实际选中=%1")
                 .arg(bGrowth->isChecked() ? QStringLiteral("增长")
                                           : QStringLiteral("排行")));
    } else {
        pass(QStringLiteral("N1 三视角按钮齐全，默认「构成」"));
    }

    QTableWidget *table = findAnalysisTable(&w);
    if (!table) {
        fail(QStringLiteral("N2 失败：找不到「磁盘分析表」"));
        return 1;
    }

    // ---------- N2：排行视角列出顶层目录 ----------
    if (bRank)
        bRank->click();
    settle(10);

    // 「名称」列用 setPathCell 塞的是 cellWidget（ElidedLabel），不是 QTableWidgetItem ——
    // 只有空数据占位行才是 setItem。所以判据用 cellWidget 是否存在来区分数据行/占位行。
    int dataRows = 0;
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->cellWidget(r, 0))
            ++dataRows;
    }
    if (dataRows < 1) {
        fail(QStringLiteral("N2 失败：「排行」视角下表格没有任何目录行"));
    } else {
        pass(QStringLiteral("N2 排行视角列出 %1 个目录").arg(dataRows));
    }

    // ---------- N3：双击下钻 → 面包屑变深 + 返回按钮可用 ----------
    const QStringList crumbBefore = breadcrumb(&w);
    if (table->rowCount() > 0)
        Q_EMIT table->cellDoubleClicked(0, 0);

    // 下钻是异步 QProcess（nice du），离屏下必须泵事件等它跑完
    bool drilled = false;
    for (int i = 0; i < 300 && !drilled; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(30);
        if (breadcrumb(&w).size() > crumbBefore.size())
            drilled = true;
    }
    QAbstractButton *bBackRoot = findButton(&w, QObject::tr("返回系统盘"));
    QAbstractButton *bBackUp = findButton(&w, QObject::tr("返回上级"));

    if (!drilled) {
        fail(QStringLiteral("N3 失败：双击后面包屑未变深（前=%1 后=%2），下钻未生效")
                 .arg(crumbBefore.join(QStringLiteral("›")))
                 .arg(breadcrumb(&w).join(QStringLiteral("›"))));
    } else if (!bBackRoot || !bBackRoot->isEnabled()) {
        fail(QStringLiteral("N3 失败：下钻后「返回系统盘」应变为可用"
                            "（updateAnalysis 未刷新上下文条），实际=%1")
                 .arg(bBackRoot ? (bBackRoot->isEnabled() ? QStringLiteral("可用")
                                                          : QStringLiteral("灰显"))
                                : QStringLiteral("不存在")));
    } else if (!bBackUp || !bBackUp->isVisible() || !bBackUp->isEnabled()) {
        fail(QStringLiteral("N3 失败：下钻后「返回上级」应可见且可用，实际=%1")
                 .arg(bBackUp ? (bBackUp->isVisible() ? QStringLiteral("可见")
                                                      : QStringLiteral("隐藏"))
                              : QStringLiteral("不存在")));
    } else {
        pass(QStringLiteral("N3 下钻生效：面包屑 %1，返回按钮已激活")
                 .arg(breadcrumb(&w).join(QStringLiteral(" › "))));
        // 下钻态截图：核对导航条（常驻返回按钮 + 面包屑）是否贴着表格
        w.grab();
        settle(4);
        w.grab().save(pngPrefix + QStringLiteral("-drilled.png"));
    }

    // ---------- N4：返回系统盘 → 面包屑收回 + 按钮变灰 ----------
    if (bBackRoot)
        bBackRoot->click();
    settle(12);

    const QStringList crumbAfter = breadcrumb(&w);
    const QStringList wantRoot{QObject::tr("系统盘")};
    if (crumbAfter != wantRoot) {
        fail(QStringLiteral("N4 失败：返回系统盘后面包屑应只剩「系统盘」，实际=%1")
                 .arg(crumbAfter.join(QStringLiteral(" › "))));
    } else if (bBackRoot && bBackRoot->isEnabled()) {
        fail(QStringLiteral("N4 失败：已在根，「返回系统盘」应重新变灰"));
    } else if (bBackUp && bBackUp->isVisible()) {
        fail(QStringLiteral("N4 失败：已在根，「返回上级」不应再显示"));
    } else {
        pass(QStringLiteral("N4 返回系统盘生效：面包屑复位、返回入口收起"));
    }

    // ---------- N5：视角说明随视角变化 ----------
    if (bGrowth)
        bGrowth->click();
    settle(10);

    QString hintText;
    for (QLabel *lb : visible<QLabel *>(&w)) {
        if (lb->text().contains(QObject::tr("增长："))) {
            hintText = lb->text();
            break;
        }
    }
    if (hintText.isEmpty()) {
        fail(QStringLiteral("N5 失败：切到「增长」后，说明文案未更新为增长口径"));
    } else {
        pass(QStringLiteral("N5 视角说明随视角更新"));
    }

    // ---------- N6：告警横幅可见 → 点关闭 → 收起 + 落盘 dismissed ----------
    // 找到横幅：含「增长」且不为空的文本标签所在的内联条；直接用关闭按钮定位。
    QAbstractButton *closeBtn = nullptr;
    for (QAbstractButton *b : visible<QAbstractButton *>(&w)) {
        if (b->accessibleName() == QObject::tr("关闭提示")) {
            closeBtn = b;
            break;
        }
    }
    if (!closeBtn) {
        fail(QStringLiteral("N6 失败：未找到告警横幅的关闭按钮（横幅未显示或不可关闭）"));
    } else {
        closeBtn->click();
        settle(12);

        // 关闭后横幅应收起（按钮随之隐藏）
        bool stillShown = false;
        for (QAbstractButton *b : visible<QAbstractButton *>(&w)) {
            if (b->accessibleName() == QObject::tr("关闭提示")) {
                stillShown = true;
                break;
            }
        }
        if (stillShown) {
            fail(QStringLiteral("N6 失败：点击关闭后告警横幅仍然可见"));
        } else {
            // 关闭 = 视图状态：数据仍在 alert.json，关闭记录落 alert-ui-state.json
            QFile st(histDir + QStringLiteral("/alert-ui-state.json"));
            bool recorded = false;
            if (st.open(QIODevice::ReadOnly)) {
                const QJsonObject o = QJsonDocument::fromJson(st.readAll()).object();
                const QJsonArray arr = o.value(QStringLiteral("dismissed")).toArray();
                recorded = !arr.isEmpty() && arr.first().toString().contains(QLatin1String(kDemoRoot));
            }
            const bool alertKept = QFile::exists(histDir + QStringLiteral("/alert.json"));
            if (!recorded) {
                fail(QStringLiteral("N6 失败：关闭记录未写入 alert-ui-state.json"));
            } else if (!alertKept) {
                fail(QStringLiteral("N6 失败：关闭不应删除 alert.json（历史里还要能回看）"));
            } else {
                pass(QStringLiteral("N6 告警可手动关闭：横幅收起、记录落盘、历史数据保留"));
            }
        }
    }

    // ---------- 截图供人工核对 ----------
    const QString png = pngPrefix + QStringLiteral(".png");
    QDir().mkpath(QFileInfo(png).absolutePath());
    w.grab();
    settle(4);
    if (!w.grab().save(png))
        fail(QStringLiteral("截图写出失败 %1").arg(png));

    QDir(kDemoRoot).removeRecursively();

    QTextStream(stdout) << (g_fails ? "=== 磁盘页导航验证 FAILED (" : "=== 磁盘页导航验证 PASSED (")
                        << g_fails << " 项失败) ===\nPNG: " << png << "\n";
    return g_fails ? 1 : 0;
}
