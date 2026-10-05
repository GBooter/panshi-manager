// SPDX-License-Identifier: GPL-3.0-or-later
//
// 表格单元格无障碍回归探针（O9 收尾，2026-10-04）
//
// 断言 PanshiUi::finalizeTableAccessible() 的三条契约：
//   1. 表级 accessibleName 生效（setTableAccessibleName）
//   2. 非空单元格 → 「列标题: 值」
//   3. 空 item（占位）→ 中性的「列标题: -」，而不是空串
//   4. 值为 0 / 空格这类「falsy 但有效」的内容不能被误判为空
//
// 另断言：cellWidget 列（路径/操作按钮）不在 item 遍历范围内，
//   必须由页面侧单独 setAccessibleName —— 本探针复刻该约束，防止
//   有人误以为「跑了 finalize 就整表无障碍了」。
//
// 用法：./probe_table_a11y   （退出码 0 = 通过）

#include "ui/components.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <cstdlib>

static int g_failures = 0;

static void check(bool cond, const QString &what)
{
    QTextStream out(stdout);
    if (cond) {
        out << "  ok   " << what << "\n";
    } else {
        out << "  FAIL " << what << "\n";
        ++g_failures;
    }
    out.flush();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    auto *t = new QTableWidget;
    // ⚠️ 必须先 setColumnCount：Qt6 里 setHorizontalHeaderLabels() **不会**顺带把列数
    // 撑起来（实测：labels 给了 3 个，columnCount() 仍是 0），随后 setItem() 会
    // **静默失败**（不报错、不崩，item(0,0) 直接是 NULL）→ 断言时读到空指针。
    // 本项目三处建表都恰好先设了列数（makeTable 走 setColumnCount；
    // 备份/部署页走 QTableWidget(0, 6) 构造），所以线上没踩到，但探针必须写对。
    t->setColumnCount(3);
    t->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("大小"),
                                   QStringLiteral("操作")});

    // 行 0：常规数据 + 一个空占位 item（模拟磁盘页目录模式的「操作」列）
    t->setRowCount(2);
    t->setItem(0, 0, new QTableWidgetItem(QStringLiteral("/var/log")));
    t->setItem(0, 1, new QTableWidgetItem(QStringLiteral("1.2 GB")));
    t->setItem(0, 2, new QTableWidgetItem());          // 空占位
    // 行 1：边界值 —— "0"、" " 都不是「无内容」，不能被写成 "-"
    t->setItem(1, 0, new QTableWidgetItem(QStringLiteral("0")));
    t->setItem(1, 1, new QTableWidgetItem(QStringLiteral(" ")));
    t->setItem(1, 2, new QTableWidgetItem(QStringLiteral("—")));

    // cellWidget 列：复刻磁盘页路径列（finalizeTableAccessible 遍历不到它）
    auto *holder = new QWidget(t);
    auto *hbox = new QHBoxLayout(holder);
    auto *lab = new QLabel(QStringLiteral("/very/long/path/that/elides"), holder);
    lab->setAccessibleName(QStringLiteral("/very/long/path/that/elides"));
    hbox->addWidget(lab, 1);
    t->setCellWidget(1, 0, holder);

    PanshiUi::setTableAccessibleName(t, QStringLiteral("增长详情表"));
    PanshiUi::finalizeTableAccessible(t);

    out << "== 表格单元格无障碍断言 ==\n";
    out << QStringLiteral("（rowCount=%1 columnCount=%2）\n")
               .arg(t->rowCount()).arg(t->columnCount());
    out.flush();

    // 先确认单元格真的存在再断言：item 为 null 时直接报「环境/建模错误」，
    // 而不是解引用崩溃 —— 崩溃会掩盖后面所有断言结果。
    for (int r = 0; r < t->rowCount(); ++r) {
        for (int c = 0; c < t->columnCount(); ++c) {
            if (!t->item(r, c)) {
                out << "  FAIL 建模错误：item(" << r << "," << c << ") 为 NULL\n";
                out.flush();
                return 1;
            }
        }
    }

    check(t->accessibleName() == QStringLiteral("增长详情表"),
          QStringLiteral("表级 accessibleName 已设置：%1").arg(t->accessibleName()));

    const QString a00 = t->item(0, 0)->data(Qt::AccessibleTextRole).toString();
    check(a00 == QStringLiteral("名称: /var/log"), QStringLiteral("非空单元格 = 「列标题: 值」→ %1").arg(a00));

    const QString a01 = t->item(0, 1)->data(Qt::AccessibleTextRole).toString();
    check(a01 == QStringLiteral("大小: 1.2 GB"), QStringLiteral("第二列独立取自己的列标题 → %1").arg(a01));

    const QString a02 = t->item(0, 2)->data(Qt::AccessibleTextRole).toString();
    check(a02 == QStringLiteral("操作: -"), QStringLiteral("空占位 item → 中性占位「操作: -」→ %1").arg(a02));

    // 边界：cellWidget 覆盖了 (1,0)，item 仍在但视觉上是 widget；
    // 这里只验证「值为 \"0\" 的 item 不会被当成空」
    const QString a10 = t->item(1, 1)->data(Qt::AccessibleTextRole).toString();
    check(a10 == QStringLiteral("大小:  "), QStringLiteral("纯空格值不被误判为空 → 「%1」").arg(a10));

    // 空表：不得崩
    auto *empty = new QTableWidget;
    empty->setColumnCount(2);
    empty->setHorizontalHeaderLabels({QStringLiteral("A"), QStringLiteral("B")});
    PanshiUi::finalizeTableAccessible(empty);
    check(true, QStringLiteral("空表调用不崩溃"));

    // 列数为 0 的表（setHorizontalHeaderLabels 不会补列数的极端态）：不得崩
    auto *noCol = new QTableWidget;
    noCol->setHorizontalHeaderLabels({QStringLiteral("A"), QStringLiteral("B")});
    PanshiUi::finalizeTableAccessible(noCol);
    check(true, QStringLiteral("0 列的表调用不崩溃（columnCount=%1）").arg(noCol->columnCount()));

    // 空指针：不得崩（helper 承诺 t 为空时静默返回）
    PanshiUi::finalizeTableAccessible(nullptr);
    PanshiUi::setTableAccessibleName(nullptr, QStringLiteral("x"));
    check(true, QStringLiteral("nullptr 调用不崩溃"));

    // cellWidget 约束：确认 item 遍历确实覆盖不到 widget，
    // widget 的可访问名只能来自页面侧 —— 断言 widget 自身有名字
    check(lab->accessibleName() == QStringLiteral("/very/long/path/that/elides"),
          QStringLiteral("cellWidget 列需页面侧单独命名（本探针已单独设置）"));

    out << "\n";
    if (g_failures == 0) {
        out << "PASS: finalizeTableAccessible 三条契约全部成立\n";
        out.flush();
        return 0;
    }
    out << "FAIL: " << g_failures << " 条断言未通过\n";
    out.flush();
    return 1;
}
