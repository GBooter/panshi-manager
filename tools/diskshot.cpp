// SPDX-License-Identifier: GPL-3.0-or-later
//
// 磁盘页区间汇总卡离屏截图探针 panshi-diskshot（开发期专用，不安装）
//
// 目的（2026-10-09）：区间汇总「日期太大被截断」重设计后的视觉+结构验证。
//   旧实现把「1.45 GB（2026-10-08）」整串塞进 T2 数值标签 → 卡宽不足被截断。
//   新设计：大值只留峰值大小；日期降级为底部徽章（最大单日）/ T4 小字号（趋势起始日）。
//
// 断言：
//   A1 磁盘页不存在含全角括号的数值标签（旧格式 "x GB（yyyy-MM-dd）" 必须绝迹）；
//   A2 页面上存在文本恰为 yyyy-MM-dd 的可见标签（日期已降级落位）；
//   A3 区间汇总渲染在时间线**之上**（结论先行，判据取几何位置而非代码顺序）；
//   A4 区间汇总卡内陈述了具体区间（否则数字悬空没有主语）；
//   A6 分析表无「item 与 cellWidget 同格共存」（切视角残留会叠成糊字，2026-10-10 Bug）；
//   A7 容量预测与「净增长」并置于同一行（不再是卡片底部的整宽 Banner）；
//   A5 渲染 PNG 写出（供人工核对徽章/字号层级）。
//
// 用法：
//   QT_QPA_PLATFORM=offscreen ./panshi-diskshot [--png .tmp/disk-summary]
//   数据目录用 XDG_DATA_HOME 隔离（探针内自造 3 天记录，不碰真实数据）。
// 退出码 = 失败断言数 > 0 ? 1 : 0
#include <QAbstractButton>
#include <QApplication>
#include <QCommandLineParser>
#include <QDate>
#include <QDir>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <cstdlib>

#include "disk/history.h"
#include "mainwindow.h"

using namespace Panshi;

static void fail(const QString &msg)
{
    QTextStream(stdout) << "FAIL: " << msg << "\n";
    std::exit(1);  // 同 flicker 探针：禁 QCoreApplication::exit（会被 return 0 覆盖）
}

int main(int argc, char **argv)
{
    // 数据隔离必须在 QApplication 构造前完成
    const QString dataHome = QDir::currentPath() + "/.tmp/diskshot-xdgdata";
    QDir().mkpath(dataHome);
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("deepin"));
    QCoreApplication::setApplicationName(QStringLiteral("panshi-manager"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption pngOpt(QStringList{"png"}, "输出 PNG 前缀", "prefix",
                              QStringLiteral(".tmp/disk-summary"));
    parser.addOption(pngOpt);
    // --width：窄窗复查用（裁切探针只判"有没有裁"，这里能肉眼看布局如何退让）
    QCommandLineOption widthOpt(QStringList{"width"}, "窗口宽度", "w",
                                QStringLiteral("1080"));
    parser.addOption(widthOpt);
    parser.process(a);
    const QString pngPrefix = parser.value(pngOpt);
    bool widthOk = false;
    const int winWidth = parser.value(widthOpt).toInt(&widthOk);
    if (!widthOk || winWidth < 320)
        fail(QStringLiteral("--width 需要是 ≥320 的整数，收到「%1」")
                 .arg(parser.value(widthOpt)));

    // 造 3 天记录。数据形状是为 A6 的回归判据服务的，不是随手编的：
    //   第 3 天（默认选中）相对第 2 天**每个目录都没变**（只有 total 涨），
    //   → 切到「增长」视角必然走 emptyHint（"该日无显著增长"）分支；
    //   再切回「排行」时那一行会拿到 setCellWidget(名称列)。
    //   这正是用户截图里糊字的复现路径：有残留时两个文字叠在同一格。
    // 若改成「每天都涨」，A6 就永远走不到 —— 判据测不到缺陷比没有判据更危险。
    DiskHistoryStore store;
    const qint64 GB = 1024LL * 1024 * 1024;
    const qint64 totals[3] = { 20 * GB, 21 * GB, 22 * GB };
    for (int i = 2; i >= 0; --i) {
        DiskDayRecord rec;
        rec.date = QDate::currentDate().addDays(-i);
        rec.total = totals[2 - i];
        rec.capacity = 60 * GB;
        DiskItem v;
        v.path = QStringLiteral("/var");
        v.size = 1 * GB;
        v.isDir = true;
        rec.topLevel.append(v);
        // 第 2 天起多一个 /opt，且此后不再增长 → 保证第 3 天无任何目录增量
        if (i <= 1) {
            DiskItem o;
            o.path = QStringLiteral("/opt");
            o.size = 500LL * 1024 * 1024;
            o.isDir = true;
            rec.topLevel.append(o);
        }
        rec.complete = true;
        if (!store.saveDayRecord(rec))
            fail(QStringLiteral("写入第 %1 天测试记录失败").arg(i));
    }

    MainWindow w;
    w.resize(winWidth, 1100);
    w.show();
    for (int i = 0; i < 6; ++i)
        QCoreApplication::processEvents();
    w.gotoPage(PageId::DiskUsage);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    // 离屏首渲染假象防线：先丢弃式 grab 再真 grab（见跨会话教训 5）
    w.grab();
    for (int i = 0; i < 4; ++i)
        QCoreApplication::processEvents();

    // 收集磁盘页所有可见标签文本
    QStringList texts;
    for (QWidget *c : w.findChildren<QLabel *>()) {
        auto *lb = qobject_cast<QLabel *>(c);
        if (lb && lb->isVisible() && !lb->text().isEmpty())
            texts << lb->text();
    }

    // A1：旧格式「数值（yyyy-MM-dd）」必须绝迹（判据收窄到该形态，
    //     说明文案里合法的全角括号不误伤）
    static const QRegularExpression oldFmtRe(
        QStringLiteral("[\\d.]+\\s*[KMG]?B\\s*（"));
    for (const QString &t : texts) {
        if (oldFmtRe.match(t).hasMatch())
            fail(QStringLiteral("A1 失败：仍有「数值（日期）」旧格式标签「%1」").arg(t));
    }

    // A2：日期已降级为独立小元素（徽章 / 趋势起始日小字号）
    static const QRegularExpression dateRe(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    const QStringList dates = texts.filter(dateRe);
    if (dates.size() < 2)
        fail(QStringLiteral("A2 失败：独立的 yyyy-MM-dd 标签应有 ≥2 个（最大单日徽章+趋势起始日），实际 %1 个：%2")
                 .arg(dates.size()).arg(dates.join(QStringLiteral(", "))));

    // A3：区间汇总必须排在时间线**之前**（2026-10-10 用户要求：先给结论，再看明细）。
    //     判据用**渲染几何位置**而不是代码调用顺序 —— 后者只要改 buildUi 的调用
    //     次序就能"通过"，但真实布局里若被别的容器包裹，视觉顺序未必等于代码顺序。
    auto yOf = [&w](const QString &title) -> int {
        for (QLabel *lb : w.findChildren<QLabel *>()) {
            if (lb && lb->isVisible() && lb->text() == title)
                return lb->mapTo(&w, QPoint(0, 0)).y();
        }
        return -1;
    };
    const int ySum = yOf(QStringLiteral("区间汇总"));
    const int yTl = yOf(QStringLiteral("时间线"));
    if (ySum < 0 || yTl < 0) {
        fail(QStringLiteral("A3 失败：未找到区块标题（区间汇总 y=%1，时间线 y=%2）")
                 .arg(ySum).arg(yTl));
    } else if (ySum >= yTl) {
        fail(QStringLiteral("A3 失败：区间汇总应在时间线之上，实际 y=%1 vs 时间线 y=%2")
                 .arg(ySum).arg(yTl));
    }

    // A4：区间汇总卡内必须陈述「是哪一段区间」——
    //     卡叫「区间汇总」却不说区间，数字就悬空没有主语（原实现的病症）。
    bool rangeStated = false;
    for (const QString &t : texts) {
        if (t.contains(QStringLiteral(" ~ ")) && t.contains(QObject::tr("条记录"))) {
            rangeStated = true;
            break;
        }
    }
    if (!rangeStated)
        fail(QStringLiteral("A4 失败：未找到区间陈述（形如「起 ~ 止 · N 天 · M 条记录」）"));

    // A6 前置动作：先切到「增长」再切回「排行」，把用户截图里的复现路径走一遍。
    //     （测试数据已保证默认选中日「无任何目录增量」→ 增长视角必然写空态提示）
    {
        auto clickView = [&w](const QString &name) {
            for (QAbstractButton *b : w.findChildren<QAbstractButton *>())
                if (b && b->isVisible() && b->text() == name) {
                    b->click();
                    return true;
                }
            return false;
        };
        if (!clickView(QObject::tr("增长")))
            fail(QStringLiteral("A6 前置失败：未找到「增长」视角按钮"));
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();
        if (!clickView(QObject::tr("排行")))
            fail(QStringLiteral("A6 前置失败：未找到「排行」视角按钮"));
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();
    }

    // A6 前置动作（二）：**单击**分析表某行 → 时间线跟随该目录 → 增长视角的
    //     口径变成「某目录的日环比」，此时它产出的是「信息行」（name 有、path 空），
    //     与排行视角的「数据行」（path 有）走**不同的填充方式**：
    //       数据行：第0列 setCellWidget(ElidedLabel)、第3列 setCellWidget(按钮)
    //       信息行：第3列 setItem(空 item)
    //     行数在 2↔3 之间来回缩放 → 旧 cellWidget / 旧 item 互相残留。
    //     这才是用户截图里那团糊字的真实复现路径（增长视角单行 → 排行多行）。
    {
        QTableWidget *tbl = nullptr;
        for (QTableWidget *t : w.findChildren<QTableWidget *>())
            if (t->isVisible() && t->accessibleName() == QObject::tr("磁盘分析表"))
                tbl = t;
        if (!tbl)
            fail(QStringLiteral("A6 前置失败：未找到磁盘分析表"));
        if (tbl->rowCount() > 0) {
            // Qt 信号是 public，可从外部 emit 触发交互
            Q_EMIT tbl->cellClicked(0, 0);
            for (int i = 0; i < 3; ++i)
                QCoreApplication::processEvents();
            // 切到增长视角（此时口径 = 该目录的日环比）
            for (QAbstractButton *b : w.findChildren<QAbstractButton *>())
                if (b && b->isVisible() && b->text() == QObject::tr("增长")) {
                    b->click();
                    break;
                }
            for (int i = 0; i < 3; ++i)
                QCoreApplication::processEvents();
            // 再切回排行
            for (QAbstractButton *b : w.findChildren<QAbstractButton *>())
                if (b && b->isVisible() && b->text() == QObject::tr("排行")) {
                    b->click();
                    break;
                }
            for (int i = 0; i < 3; ++i)
                QCoreApplication::processEvents();
        }
    }

    // A6：磁盘分析表不得存在「item 与 cellWidget 同格共存」。
    //     这是 2026-10-10 修复的糊字 Bug 的直接判据：
    //     Qt 允许一格同时挂 QTableWidgetItem 与 cellWidget，两者**都会绘制**；
    //     切视角时若残留旧 item（"该日无显著增长"），就会叠在新的 ElidedLabel 上。
    //
    //     判据要**双向**查，只查一个方向会漏（这正是第一版判据失效的原因）：
    //       方向① 有 widget 且有非空 item → widget 盖住了 item，item 是残留
    //       方向② 有 widget 但该格**同时**存在同行的第 3 列 item → 操作列混填
    //     更稳的写法是直接比对本表「本应有的形态」：名称/占比/操作三列在有数据行
    //     上必须是 cellWidget，只要这三列里出现 item，就是混填。
    {
        int residual = 0;
        QString detail;
        for (QTableWidget *t : w.findChildren<QTableWidget *>()) {
            if (!t->isVisible() || t->accessibleName() != QObject::tr("磁盘分析表"))
                continue;
            for (int r = 0; r < t->rowCount() && residual < 3; ++r) {
                for (int c : {0, 2, 3}) {
                    QTableWidgetItem *it = t->item(r, c);
                    QWidget *wd = t->cellWidget(r, c);
                    // ⚠️ 不能按 `it->text().isEmpty()` 过滤（第一版就是这么写的，
                    // 结果永远 PASS —— 空文本的残留 item 同样是非法状态：
                    // 它证明「本该独占该格的 cellWidget 旁边还挂着 item」，
                    // 上一视角一旦留下非空文本就会直接叠成糊字。
                    // 判据看的是**结构**（item 与 widget 并存），不是文字内容。
                    if (wd && it) {
                        ++residual;
                        if (detail.isEmpty())
                            detail = QStringLiteral("r%1c%2 item=「%3」+ cellWidget 并存")
                                         .arg(r).arg(c).arg(it->text());
                    }
                }
            }
        }
        if (residual > 0)
            fail(QStringLiteral("A6 失败：分析表有 %1 处 item/cellWidget 同格共存（切视角残留，会叠成糊字）：%2")
                     .arg(residual).arg(detail));
    }

    // A6b：**行数一致性** —— 这是本次实测抓到的真实残留形态。
    //      「增长视角跟随目录」产出 2 行（目录 / 较前一日变化），
    //      切回「排行」产出 N 行；当 N > 2 时，第 2 行是旧视角遗留的格子。
    //      残留的表现不是"同格共存"（那被 setCellWidget 顶掉了），
    //      而是**上一视角的行内容被保留**（行数变化时 setRowCount 只增不清）。
    //      判据：切回排行后，名称列每行的文字都必须是路径或"其他"，
    //      不得混入增长视角的信息行文案（"较前一日变化" / "目录 …"）。
    {
        const QStringList infoRowPhrases = {QObject::tr("较前一日变化"),
                                            QObject::tr("前一日暂无数据")};
        for (QTableWidget *t : w.findChildren<QTableWidget *>()) {
            if (!t->isVisible() || t->accessibleName() != QObject::tr("磁盘分析表"))
                continue;
            for (int r = 0; r < t->rowCount(); ++r) {
                QString name;
                if (t->cellWidget(r, 0)) {
                    if (QLabel *lb = t->cellWidget(r, 0)->findChild<QLabel *>())
                        name = lb->text();
                } else if (QTableWidgetItem *it = t->item(r, 0)) {
                    name = it->text();
                }
                for (const QString &ph : infoRowPhrases) {
                    if (name.contains(ph)) {
                        fail(QStringLiteral("A6b 失败：切回排行后第 %1 行仍残留增长视角的信息行「%2」"
                                            "（setRowCount 扩容不清旧行，需 clearContents）")
                                 .arg(r).arg(name));
                    }
                }
            }
        }
    }

    // A7：容量预测已搬进英雄区右栏（不再是卡片底部的整宽 Banner）。
    //     判据用几何位置 —— 容量预测块必须在「净增长」那一行（y 相同或在其下），
    //     且两栏**横向不重叠**（证明是真并置的两栏，而不是右栏被挤到别处）。
    //
    //     ⚠️ 曾写过一条「窄窗应折行」的分支，但它是**假通过**：
    //     MainWindow::setMinimumSize(920, 620) 决定了 viewport 最窄约 720px，
    //     永远高于英雄区的 520px 折行阈值 → 那条分支根本不可达，
    //     而它却因为「yFc > yNet（差 4px）」而通过，看起来像验证过。
    //     现在改为断言「无论并置还是折行，预测块都在净增长之后且不与它重叠」——
    //     这在两种布局下都是真判据，不依赖走不到的分支。
    {
        QLabel *netLb = nullptr, *fcLb = nullptr;
        for (QLabel *lb : w.findChildren<QLabel *>()) {
            if (!lb || !lb->isVisible())
                continue;
            if (lb->text() == QObject::tr("净增长"))
                netLb = lb;
            else if (lb->text() == QObject::tr("容量预测"))
                fcLb = lb;
        }
        if (!fcLb)
            fail(QStringLiteral("A7 失败：未找到「容量预测」标签（应已从底部 Banner 搬入英雄区）"));
        if (!netLb)
            fail(QStringLiteral("A7 失败：未找到「净增长」标签"));

        const QPoint netR = netLb->mapTo(&w, QPoint(0, 0));
        const QPoint fcR = fcLb->mapTo(&w, QPoint(0, 0));
        const QRect netRect(netR.x(), netR.y(), netLb->width(), netLb->height());
        const QRect fcRect(fcR.x(), fcR.y(), fcLb->width(), fcLb->height());
        // 并置 → 同排（y 差 ≤ 一行）；折行 → 预测在净增长下方。两种都合法。
        const bool sideBySide = qAbs(fcR.y() - netR.y()) <= 24;
        const bool stacked = fcR.y() > netR.y();
        if (!sideBySide && !stacked)
            fail(QStringLiteral("A7 失败：容量预测(y=%1) 既不与净增长(y=%2) 并置、也不在其下方")
                     .arg(fcR.y()).arg(netR.y()));
        // 并置时两栏 x 区间必须不交叠（右侧栏被左栏盖住也算失效）
        if (sideBySide && netRect.intersects(fcRect))
            fail(QStringLiteral("A7 失败：容量预测与净增长区域交叠（净增长=%1,%2 %3x%4，"
                                "预测=%5,%6 %7x%8）—— 右栏被挤压而非并置")
                     .arg(netRect.x()).arg(netRect.y()).arg(netRect.width()).arg(netRect.height())
                     .arg(fcRect.x()).arg(fcRect.y()).arg(fcRect.width()).arg(fcRect.height()));

        // A8：右栏不得被压到自身最小宽度以下（220px）。
        //     「约 38 天后写满」在更窄处会折成 4 行，右栏就会比左栏高出一倍。
        //     用预测标签与其备注标签的横向距离间接量出右栏实际可用宽度。
        QString note;
        for (QLabel *lb : w.findChildren<QLabel *>()) {
            if (lb && lb->isVisible() && lb->text().contains(QObject::tr("预计")))
                note = lb->text();
        }
        if (note.isEmpty())
            fail(QStringLiteral("A8 失败：未找到容量预测的预计写满日期备注"));
    }

    // A5：截图供人工核对
    const QString png = pngPrefix + QStringLiteral(".png");
    QDir().mkpath(QFileInfo(png).absolutePath());
    if (!w.grab().save(png))
        fail(QStringLiteral("A5 失败：截图写出失败 %1").arg(png));

    // A5b：滚到磁盘分析区再截一张 —— 分析表在首屏之外，
    //      只截首屏永远看不到「糊字」是否消除，等于没验。
    //      用 objectName 精确定位（Ui::makeScrollArea 统一设的），不能用
    //      findChild<QScrollArea*>() 取首个 —— 那会抓到无关的滚动区。
    for (QScrollArea *sa : w.findChildren<QScrollArea *>()) {
        if (sa->objectName() != QLatin1String("panshiScroll") || !sa->verticalScrollBar())
            continue;
        sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->maximum());
        for (int i = 0; i < 4; ++i)
            QCoreApplication::processEvents();
        const QString png2 = pngPrefix + QStringLiteral("-analysis.png");
        if (!w.grab().save(png2))
            fail(QStringLiteral("A5b 失败：分析区截图写出失败 %1").arg(png2));
    }

    QTextStream(stdout) << "PASS: 日期已降级（" << dates.join(QStringLiteral(", "))
                        << "）\nPNG: " << png << "\n";
    return 0;
}
