// SPDX-License-Identifier: GPL-3.0-or-later
//
// 「关于」页参考区块离屏截图 + 结构探针 panshi-aboutpageshot（开发期专用，不安装）
//
// 目的（2026-10-27）：技术参考 / 常用命令两块原本是两条居中细字「▸ 标题」，
// 悬在页面下半幅留白里 —— 看起来像没做完的占位（用户反馈「这部分现在的方式不好看」）。
// 重设计后是两张参考卡 + 两列机制网格 + 命令条网格。这类版式问题肉眼能判断，
// 但**结构退化**（列宽不共享 → 复制按钮参差、卡片被压扁、窄窗横向溢出）
// 只有断言能守住。
//
// 断言（判据一律取几何/父子结构，不比对文案内容）：
//   A1 两张参考卡都在、都可见、**默认折叠**（可见条目数 0，标题+副标题仍在，
//      「复制全部」仍挂在标题行）；判据必须数 isVisible() 的条目，不能用
//      findChildren 的数量（收起时 widget 仍在树上，数量恒为 6 → 假通过）；
//   A1b 从折叠态展开：caret 无障碍名=「展开」，两卡高度变大；
//   A2 机制条目 6 个且排成两列（同列 x 相同、右列 x > 左列 x）；
//   A3 命令条 6 个，每条都挂了 trailing 复制按钮（1 个 QAbstractButton）；
//   A4 命令条左右边缘完全对齐（QGridLayout 共享列宽的直接判据；
//      逐行独立 QHBoxLayout 会退化成参差不齐）；
//   A5 命令条高度足够（≥ 28px，防止被卡片压缩成一条缝）；
//   A6 窄窗（--width）下无横向溢出（外层 ScrollBarAlwaysOff 会静默裁切，
//      溢出不可达 —— 只能用几何断言发现）；
//   A7 折叠交互有效：setExpanded(false) 后内容区隐藏且高度回到初始折叠值；
//
// 用法：
//   QT_QPA_PLATFORM=offscreen ./panshi-aboutpageshot [--png 前缀] [--width 1080]
// 退出码 = 失败断言数 > 0 ? 1 : 0
//
// ⚠️ 暗色那张图（_dark.png）只能看「卡片/图标/命令条」，**不能**用来判「页面底色是否跟随主题」：
//    离屏下 setPaletteType() 不会改写各控件的 palette（见跨会话教训：主题类断言必须真机），
//    于是容器底色仍留在亮色，而走 appPalette() 的卡片底已经是暗色 —— 看起来像"半暗"，
//    那是离屏假象，不是缺陷。
#include "mainwindow.h"
#include "pages/aboutpage.h"

#include <DGuiApplicationHelper>

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDir>
#include <QMouseEvent>
#include <QLabel>
#include <QTextStream>
#include <QWidget>

#include <cstdlib>

namespace {

int g_fail = 0;

void check(bool ok, const QString &what)
{
    QTextStream(stdout) << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    if (!ok)
        ++g_fail;
}

QString geo(const QWidget *w, const QWidget *ref)
{
    const QPoint p = w->mapTo(ref, QPoint(0, 0));
    return QStringLiteral("x=%1 y=%2 w=%3 h=%4").arg(p.x()).arg(p.y()).arg(w->width()).arg(w->height());
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("deepin"));
    QCoreApplication::setApplicationName(QStringLiteral("panshi-manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PANSHI_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption pngOpt(QStringList{"png"}, "输出 PNG 前缀", "prefix",
                              QStringLiteral(".tmp/about-page"));
    QCommandLineOption widthOpt(QStringList{"width"}, "窗口宽度", "w", QStringLiteral("1080"));
    parser.addOption(pngOpt);
    parser.addOption(widthOpt);
    parser.process(a);

    const QString pngPrefix = parser.value(pngOpt);
    bool ok = false;
    const int winWidth = parser.value(widthOpt).toInt(&ok);
    if (!ok || winWidth < 320) {
        QTextStream(stdout) << "FAIL: --width 需要 ≥320 的整数\n";
        return 1;
    }

    MainWindow w;
    w.resize(winWidth, 900);
    w.show();
    for (int i = 0; i < 6; ++i)
        QCoreApplication::processEvents();
    w.gotoPage(PageId::About);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    // 离屏首渲染假象：先丢弃式 grab 再真 grab（跨会话教训 5）
    w.grab();
    for (int i = 0; i < 4; ++i)
        QCoreApplication::processEvents();

    auto *page = qobject_cast<AboutPage *>(w.page(PageId::About));
    if (!page) {
        QTextStream(stdout) << "FAIL: 取不到 AboutPage\n";
        return 1;
    }

    // ---- A1：两张参考卡 + 默认折叠 ----
    QWidget *techCard = page->findChild<QWidget *>(QStringLiteral("panshiAboutTechCard"));
    QWidget *cmdCard = page->findChild<QWidget *>(QStringLiteral("panshiAboutCommandCard"));
    check(techCard && techCard->isVisible(), QStringLiteral("A1 技术参考卡存在且可见"));
    check(cmdCard && cmdCard->isVisible(), QStringLiteral("A1 常用命令卡存在且可见"));
    if (!techCard || !cmdCard) {
        QTextStream(stdout) << "参考卡缺失，后续断言无法进行\n";
        return 1;
    }

    // ⚠️ 判据必须数「可见的」条目：折叠只把内容宿主 setVisible(false)，
    //    条目 widget 仍在对象树上 —— findChildren 的数量在收起态依然是 6，
    //    用它判「是否收起」会永远通过（假通过）。
    const auto visibleObj = [page](const QString &name) {
        QList<QWidget *> out;
        for (QWidget *w : page->findChildren<QWidget *>(name)) {
            if (w->isVisible() && w->height() > 0)
                out << w;
        }
        return out;
    };

    check(visibleObj(QStringLiteral("panshiReferenceItem")).isEmpty(),
          QStringLiteral("A1 机制条目默认收起（可见 %1 个）")
              .arg(visibleObj(QStringLiteral("panshiReferenceItem")).size()));
    check(visibleObj(QStringLiteral("panshiCommandChip")).isEmpty(),
          QStringLiteral("A1 命令条默认收起（可见 %1 个）")
              .arg(visibleObj(QStringLiteral("panshiCommandChip")).size()));

    // 折叠态不能是"空壳"：标题与副标题必须还在（否则用户不知道里面是什么）
    const auto hasVisibleLabel = [](QWidget *card, const QString &text) {
        for (QLabel *l : card->findChildren<QLabel *>())
            if (l->isVisible() && l->text() == text)
                return true;
        return false;
    };
    check(hasVisibleLabel(techCard, QObject::tr("磐石系统技术参考"))
              && hasVisibleLabel(techCard, QObject::tr("不可变系统的六条核心机制")),
          QStringLiteral("A1 折叠态仍显示标题 + 副标题"));
    // 组级「复制全部」挂在标题行，不随内容收起
    {
        QAbstractButton *copyAll = nullptr;
        for (QAbstractButton *b : cmdCard->findChildren<QAbstractButton *>())
            if (b->isVisible() && b->text() == QObject::tr("复制全部"))
                copyAll = b;
        check(copyAll != nullptr, QStringLiteral("A1 折叠态下「复制全部」仍可见"));
    }

    // ---- A1b：从折叠态展开（caret 无障碍名在收起态应为「展开」）----
    // 先把用户真正看到的默认态存图：这次改版的全部意义就在这张图上
    QDir().mkpath(QStringLiteral(".tmp"));
    const QString pngDefault = pngPrefix + QStringLiteral("_w%1.png").arg(winWidth);
    check(w.grab().save(pngDefault),
          QStringLiteral("A8 出图（默认折叠态）%1").arg(pngDefault));

    const auto expandCard = [](QWidget *card) {
        for (QAbstractButton *b : card->findChildren<QAbstractButton *>()) {
            if (b->accessibleName() == QObject::tr("展开")) {
                b->click();
                return true;
            }
        }
        return false;
    };
    const int hTechCollapsed = techCard->height();
    const int hCmdCollapsed = cmdCard->height();
    check(expandCard(techCard), QStringLiteral("A1b 技术参考卡收起态 caret 无障碍名=展开"));
    check(expandCard(cmdCard), QStringLiteral("A1b 常用命令卡收起态 caret 无障碍名=展开"));
    for (int i = 0; i < 6; ++i)
        QCoreApplication::processEvents();
    check(techCard->height() > hTechCollapsed,
          QStringLiteral("A1b 技术参考卡展开后变高（%1 → %2）")
              .arg(hTechCollapsed).arg(techCard->height()));
    check(cmdCard->height() > hCmdCollapsed,
          QStringLiteral("A1b 常用命令卡展开后变高（%1 → %2）")
              .arg(hCmdCollapsed).arg(cmdCard->height()));

    // ---- A2：机制条目两列 ----
    // ⚠️ 两列网格是**行优先**填充：下标 0/2/4 在左列、1/3/5 在右列。
    //    最初按"列优先"写的判据（0/1/2 同 x、0 与 3 同 y）必然假失败 ——
    //    判据写错和实现写错在结果上无法区分，所以判据也要能被回退验证。
    const QList<QWidget *> refItems = visibleObj(QStringLiteral("panshiReferenceItem"));
    check(refItems.size() == 6, QStringLiteral("A2 机制条目 6 个（实际 %1）").arg(refItems.size()));
    if (refItems.size() == 6) {
        const auto pos = [&refItems, techCard](int i) {
            return refItems.at(i)->mapTo(techCard, QPoint(0, 0));
        };
        const QPoint p0 = pos(0);
        const QPoint p1 = pos(1);
        const QPoint p2 = pos(2);
        const QPoint p4 = pos(4);
        check(p0.x() == p2.x() && p0.x() == p4.x(),
              QStringLiteral("A2 左列（0/2/4）x 相同（%1 / %2 / %3）").arg(p0.x()).arg(p2.x()).arg(p4.x()));
        check(p1.x() > p0.x() && p1.x() == pos(3).x() && p1.x() == pos(5).x(),
              QStringLiteral("A2 右列（1/3/5）x 相同且在左侧（%1 > %2）").arg(p1.x()).arg(p0.x()));
        check(p0.y() == p1.y(),
              QStringLiteral("A2 首行两列同高（%1 vs %2）").arg(p0.y()).arg(p1.y()));
        check(p2.y() > p0.y(),
              QStringLiteral("A2 第二行在第一行之下（%1 > %2）").arg(p2.y()).arg(p0.y()));
    }

    // ---- A3 / A4 / A5：命令条 ----
    const QList<QWidget *> chips = visibleObj(QStringLiteral("panshiCommandChip"));
    check(chips.size() == 6, QStringLiteral("A3 命令条 6 个（实际 %1）").arg(chips.size()));

    int withCopy = 0;
    int minH = 1 << 30;
    int refLeft = -1;
    int refRight = -1;
    bool aligned = true;
    for (QWidget *chip : chips) {
        int btn = 0;
        for (QAbstractButton *b : chip->findChildren<QAbstractButton *>())
            if (b->isVisible())
                ++btn;
        if (btn == 1)
            ++withCopy;
        minH = qMin(minH, chip->height());

        const QPoint p = chip->mapTo(cmdCard, QPoint(0, 0));
        const int right = p.x() + chip->width();
        if (refLeft < 0) {
            refLeft = p.x();
            refRight = right;
        } else if (p.x() != refLeft || right != refRight) {
            aligned = false;
            QTextStream(stdout) << "    命令条未对齐: " << geo(chip, cmdCard) << "\n";
        }
    }
    check(!chips.isEmpty() && withCopy == chips.size(),
          QStringLiteral("A3 每条命令条都有 1 个复制按钮（%1/%2）").arg(withCopy).arg(chips.size()));
    check(aligned, QStringLiteral("A4 命令条左右边缘对齐（left=%1 right=%2）").arg(refLeft).arg(refRight));
    check(minH >= 28, QStringLiteral("A5 命令条高度 ≥28px（最小 %1）").arg(minH));

    // ---- A6：窄窗横向溢出 ----
    int overflow = 0;
    for (QWidget *cw : page->findChildren<QWidget *>()) {
        if (!cw->isVisible() || cw->parentWidget() == nullptr)
            continue;
        // 只检查参考卡内部（页面其余部分不在本次改动范围，避免噪声）
        if (!techCard->isAncestorOf(cw) && !cmdCard->isAncestorOf(cw))
            continue;
        const QPoint p = cw->mapTo(page, QPoint(0, 0));
        if (p.x() + cw->width() > page->width())
            ++overflow;
    }
    check(overflow == 0, QStringLiteral("A6 无横向溢出（越界控件 %1 个，页宽 %2）")
                             .arg(overflow).arg(page->width()));

    // ---- A7：折叠交互 ----
    auto *tech = qobject_cast<QWidget *>(techCard);
    const int hExpanded = tech->height();
    // 通过「标题行整行可点」的公开路径触发：找不到内部事件源时退化为直接找 caret 按钮
    bool collapsed = false;
    for (QAbstractButton *b : tech->findChildren<QAbstractButton *>()) {
        if (b->accessibleName() == QObject::tr("收起")) {
            b->click();
            collapsed = true;
            break;
        }
    }
    for (int i = 0; i < 4; ++i)
        QCoreApplication::processEvents();
    check(collapsed, QStringLiteral("A7 找到展开态 caret（无障碍名=收起）"));
    check(tech->height() < hExpanded,
          QStringLiteral("A7 收起后卡片变矮（%1 → %2）").arg(hExpanded).arg(tech->height()));
    // 回到折叠态应当**等于**初始折叠高度：不等说明 setVisible 之外还有残留在撑高度
    check(tech->height() == hTechCollapsed,
          QStringLiteral("A7 收起高度回到初始折叠高度（%1 vs %2）")
              .arg(tech->height()).arg(hTechCollapsed));

    // 恢复展开，出图用展开态
    for (QAbstractButton *b : tech->findChildren<QAbstractButton *>()) {
        if (b->accessibleName() == QObject::tr("展开")) {
            b->click();
            break;
        }
    }
    for (int i = 0; i < 6; ++i)
        QCoreApplication::processEvents();

    // ---- A10：标题行整行可点（点标题文字也能展开/收起）----
    //
    // ⚠️ 这条断言不是为了好看：标题行可点靠的是「标签忽略鼠标事件 → 冒泡到标题容器」，
    //    而 Qt 的按下/释放是**分别**投递的 —— 如果标题容器不接受 press，它就拿不到
    //    release，"整行可点"会静默失效（只有 caret 能点）。只看代码看不出来，必须实测。
    {
        QLabel *titleLabel = nullptr;
        for (QLabel *l : tech->findChildren<QLabel *>()) {
            if (l->isVisible() && l->text() == QObject::tr("磐石系统技术参考")) {
                titleLabel = l;
                break;
            }
        }
        check(titleLabel != nullptr, QStringLiteral("A10 找到卡片标题标签"));
        if (titleLabel) {
            const auto clickOn = [titleLabel] {
                const QPoint c = titleLabel->rect().center();
                const QPoint g = titleLabel->mapToGlobal(c);
                QMouseEvent press(QEvent::MouseButtonPress, c, g, Qt::LeftButton, Qt::LeftButton,
                                  Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, c, g, Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(titleLabel, &press);
                QCoreApplication::sendEvent(titleLabel, &release);
                for (int i = 0; i < 4; ++i)
                    QCoreApplication::processEvents();
            };
            const int h0 = tech->height();
            clickOn();
            const int h1 = tech->height();
            check(h1 != h0, QStringLiteral("A10 点标题文字可收起（%1 → %2）").arg(h0).arg(h1));
            clickOn();
            const int h2 = tech->height();
            check(h2 == h0, QStringLiteral("A10 再点可展开回原状（%1 → %2）").arg(h1).arg(h2));
        }
    }
    w.grab();

    // ---- A11：组级「复制全部」真的把六条命令写进剪贴板 ----
    // 按钮存在 ≠ 按钮有用：文案/连接漏掉时按钮照样显示、点了没反应。
    {
        QAbstractButton *copyAll = nullptr;
        for (QAbstractButton *b : cmdCard->findChildren<QAbstractButton *>()) {
            if (b->text() == QObject::tr("复制全部")) {
                copyAll = b;
                break;
            }
        }
        check(copyAll != nullptr, QStringLiteral("A11 找到「复制全部」按钮"));
        if (copyAll) {
            QApplication::clipboard()->clear();
            copyAll->click();
            for (int i = 0; i < 2; ++i)
                QCoreApplication::processEvents();
            const QStringList lines =
                QApplication::clipboard()->text().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            check(lines.size() == 6,
                  QStringLiteral("A11 剪贴板得到 6 条命令（实际 %1）").arg(lines.size()));
            check(!lines.isEmpty()
                      && lines.first().startsWith(QStringLiteral("sudo deepin-immutable-ctl snapshot")),
                  QStringLiteral("A11 首行是快照命令"));
            // 复原文案（点击后会变成「已复制」，2 秒后自动复位）
            copyAll->click();
        }
    }

    QDir().mkpath(QStringLiteral(".tmp"));
    const QString pngOpen = pngPrefix + QStringLiteral("_w%1_open.png").arg(winWidth);
    check(w.grab().save(pngOpen), QStringLiteral("A8 出图（展开态）%1").arg(pngOpen));

    // ---- A9：暗色一套 ----
    // 图块/图标是 pixmap 快照、命令条底取 itemBackground() —— 亮暗两套必须各出一张，
    // 否则「暗色下图标消失/命令条与卡片同色」这类问题肉眼无从发现。
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    for (int i = 0; i < 8; ++i)
        QCoreApplication::processEvents();
    const QString pngDark = pngPrefix + QStringLiteral("_w%1_dark.png").arg(winWidth);
    check(w.grab().save(pngDark), QStringLiteral("A9 出图 %1").arg(pngDark));

    QTextStream(stdout) << (g_fail == 0 ? "全部通过\n" : QStringLiteral("失败 %1 项\n").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
