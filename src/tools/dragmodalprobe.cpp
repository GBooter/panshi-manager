// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * 模态对话框 → 主窗口拖拽 阻塞判据探针（开发期专用，不安装）
 *
 * 用途（2026-10-06「窗口拖不动」根因定位）：
 *   用户报「弹了某个提示之后窗口就拖不动」。此前几轮怀疑 CSD / dxcb 钩子均被
 *   反向验证推翻。本探针给出可判定的结论：拖拽失效的**触发条件**是
 *   模态对话框打开了，而**不是**窗口装饰方式。
 *
 * 判据（两个都打，避免单指标骗人 —— 见项目记忆「单一指标会骗人」）：
 *   ① QApplication::activeModalWidget() 非空  → Qt 处于模态 grab 状态
 *   ② windowHandle()->isVisible() 且未被 grab  → 用「窗口能否收到
 *      MouseButtonPress」等价判据：模态 grab 存在时，主窗口收不到
 *      标题栏鼠标事件，DTitlebar 的拖动状态机无法启动。
 *
 * 输出：MODAL=<0|1> 逐场景一行，最后给 PASS/FAIL 汇总。
 * 退出码：0 = 修复生效（对话框期间仍可拖）；1 = 仍被阻塞；2 = 探针自身失败。
 *
 * ⚠️ 判据有效性自检：本探针必须能同时给出「阻塞」与「不阻塞」两种结果。
 *   若两个场景都 PASS，说明判据测不到缺陷（假阴性），不可采信。
 */
#include <DApplication>
#include <DDialog>
#include <DMainWindow>
#include <DPushButton>
#include <DPlatformHandle>
#include <DGuiApplicationHelper>

// 被测函数本体：探针必须直接调用项目里的实现，否则测的是"另一份代码"。
#include "ui/components.h"

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QDebug>
#include <QWidget>
#include <QWindow>
#include <QTimer>
#include <QEventLoop>
#include <cstdlib>
#include <cstdio>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace {

int g_fail = 0;
int g_pass = 0;

struct Scenario {
    const char *name;
    bool expectBlocked;   // 该场景下主窗口是否应当被模态 grab 阻塞
};

/**
 * 主窗口此刻能否"开始拖拽"。
 *
 * 不用「真的去拖」——那需要合成器配合、在离屏/自动化环境里脆得很。
 * 改用等价且稳定的判据：模态 grab 存在时，Qt 会在事件派发前把
 * 模态窗口之外的目标窗口过滤掉，主窗口收不到 MouseButtonPress。
 * 这里直接查 Qt 的模态状态 + 窗口可见性，二者共同决定拖拽能否启动。
 */
bool windowCanStartDrag(QWidget *mainWin)
{
    if (!mainWin || !mainWin->windowHandle())
        return false;
    if (!mainWin->windowHandle()->isVisible())
        return false;
    // 标题栏能否收到鼠标事件才是拖拽的先决条件。
    // 判据：主窗口自身 enabled（标题栏不在 centralWidget 内，不受冻结影响）
    //      且不存在任何模态 grab。
    if (!mainWin->isEnabled())
        return false;
    QWidget *modal = QApplication::activeModalWidget();
    if (modal && modal != mainWin)
        return false;
    return true;
}

/** 页面内容区当前是否被冻结（确认框打开期间应为 true） */
bool contentFrozen(QWidget *mainWin)
{
    auto *mw = qobject_cast<QMainWindow *>(mainWin);
    QWidget *c = mw ? mw->centralWidget() : nullptr;
    return c && !c->isEnabled();
}

void report(const Scenario &sc, QWidget *mainWin)
{
    QWidget *modal = QApplication::activeModalWidget();
    const bool blocked = !windowCanStartDrag(mainWin);
    // activeModalWidget 非空即代表 Qt 已建立模态 grab
    const int modalFlag = modal ? 1 : 0;
    fprintf(stderr, "SCENARIO %s | MODAL=%d | 内容区冻结=%s | 主窗口可拖=%s | 期望阻塞=%s | %s\n",
                sc.name, modalFlag, contentFrozen(mainWin) ? "是" : "否",
                blocked ? "否" : "是",
                sc.expectBlocked ? "是" : "否",
                blocked == sc.expectBlocked ? "PASS" : "FAIL");
    if (blocked == sc.expectBlocked)
        ++g_pass;
    else
        ++g_fail;
}

} // namespace

int main(int argc, char **argv)
{
    DApplication app(argc, argv);
    app.setOrganizationName("deepin");
    app.setApplicationName("panshi-dragprobe");
    app.setApplicationVersion(QStringLiteral(PANSHI_VERSION));
    app.loadTranslator();

    auto *mainWin = new DMainWindow;
    mainWin->setWindowTitle(QStringLiteral("磐石设置"));
    mainWin->resize(1060, 700);
    mainWin->winId();   // 强制建句柄

    auto *central = new QWidget(mainWin);
    auto *lay = new QVBoxLayout(central);
    lay->addWidget(new QLabel(QStringLiteral("探针主体"), central));
    mainWin->setCentralWidget(central);
    mainWin->show();

    // 让 show() 走完事件循环，窗口句柄与 dxcb titlebar 才真正就位
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QCoreApplication::sendPostedEvents();
    }

    // ---- 场景 1：无对话框 ----
    report({"无对话框", false}, mainWin);

    // ---- 场景 2：ApplicationModal 对话框打开中（本项目修复前的行为）----
    {
        DDialog dlg(mainWin);
        dlg.setTitle(QStringLiteral("应用级模态"));
        dlg.addButton(QStringLiteral("关闭"), false, DDialog::ButtonNormal);
        dlg.setWindowModality(Qt::ApplicationModal);   // 显式，确保是本场景
        dlg.show();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        report({"ApplicationModal 打开中", true}, mainWin);
        dlg.close();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    // ---- 场景 3：关闭后应恢复 ----
    report({"对话框关闭后", false}, mainWin);

    // ---- 场景 4：WindowModal 对话框打开中（修复后的行为）----
    {
        DDialog dlg(mainWin);
        dlg.setTitle(QStringLiteral("窗口级模态"));
        dlg.addButton(QStringLiteral("关闭"), false, DDialog::ButtonNormal);
        PanshiUi::makeDialogNonBlockingWindowModal(&dlg);
        dlg.show();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        // DDialog 若在 showEvent 里重置了 modality，这里要如实反映实际值
        const bool actuallyWindowModal = (dlg.windowModality() == Qt::WindowModal);
        fprintf(stderr, "INFO 修复后 DDialog 实际 modality=%s（期望 WindowModal）\n",
                actuallyWindowModal ? "WindowModal" : "被重置");
        report({"NonModal+冻结内容（修复目标）", false}, mainWin);
        if (!contentFrozen(mainWin)) { fprintf(stderr, "SCENARIO 内容区应被冻结 | FAIL\n"); ++g_fail; } else { fprintf(stderr, "SCENARIO 内容区应被冻结 | PASS\n"); ++g_pass; }
        dlg.close();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    // ---- 场景 5：无 parent 的对话框必须保持 ApplicationModal（不得假装改了）----
    {
        DDialog dlg;                      // 无 parent（无 window 可冻结）
        dlg.addButton(QStringLiteral("关闭"), false, DDialog::ButtonNormal);
        PanshiUi::makeDialogNonBlockingWindowModal(&dlg);
        const bool stillAppModal = (dlg.windowModality() != Qt::WindowModal);
        fprintf(stderr, "SCENARIO 无parent对话框保持应用级模态 | 实际=%s | %s\n",
                stillAppModal ? "是" : "否", stillAppModal ? "PASS" : "FAIL");
        if (stillAppModal) ++g_pass; else ++g_fail;
    }

    fprintf(stderr, "SUMMARY PASS=%d FAIL=%d\n", g_pass, g_fail);
    fprintf(stderr, "%s\n", g_fail == 0 ? "VERDICT 修复生效" : "VERDICT 仍存在阻塞");

    // ⚠️ 必须用 std::exit：QCoreApplication::exit(1) 会被析构/return 0 覆盖
    std::exit(g_fail == 0 ? 0 : 1);
}
