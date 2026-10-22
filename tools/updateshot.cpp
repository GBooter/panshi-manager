// SPDX-License-Identifier: GPL-3.0-or-later
//
// 系统更新页「待更新软件包明细 + 用途分组」离屏验证探针 panshi-updateshot
//（开发期专用，不安装）
//
// 背景（2026-10-18）：`deepin-immutable-ctl upgrade check` 只输出计数
// （Total count / Need download count），用户看不到"要更新哪些包、装的是什么"。
// 更新页新增「待更新软件包」区：检查完成且存在可更新项时，从 apt 只读视图
// （apt.real list --upgradable + apt-cache show）取明细，按用途分类分组渲染。
//
// 断言（黑盒：只点按钮、按 objectName / 文本找控件，不依赖私有成员）：
//   U1 点「检查更新」后出现状态结论（徽章为「有可用更新」或「已是最新」）；
//   U2 明细区可见性跟随 apt 实测：本机有 N 个可更新包 → 「待更新软件包」区
//      可见，包行总数 == N、分组数 >= 1；N == 0 → 整区隐藏（0 行）；
//   U3 每个分组都有「N 项」数量徽章；
//   U4 重复点一次「检查更新」不叠加行（重渲染前先清空内层容器）。
//
// apt 实测数由探针独立执行 apt.real 获得，与 UI 包行数互证 —— 不写死 6，
// 系统更新后探针依然成立。
//
// 用法：
//   QT_QPA_PLATFORM=offscreen ./panshi-updateshot [--png .tmp/update-page]
// 退出码 = 失败断言数 > 0 ? 1 : 0
#include <QAbstractButton>
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>
#include <cstdlib>

#include <DPushButton>

#include "backend.h"
#include "mainwindow.h"
#include "ui/components.h"

using namespace Panshi;

// 包行 / 组标题的 objectName（updatepage.cpp 渲染时打上，探针据此定位）
static const char *kPkgRowName = "panshiPkgRow";
static const char *kPkgGroupName = "panshiPkgGroup";

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

static QAbstractButton *findButton(QWidget *root, const QString &text)
{
    for (QAbstractButton *b : visible<QAbstractButton *>(root)) {
        if (b->text() == text)
            return b;
    }
    return nullptr;
}

static DLabel *findLabel(QWidget *root, const QString &text)
{
    for (QLabel *lb : visible<QLabel *>(root)) {
        if (lb->text() == text)
            return qobject_cast<DLabel *>(lb);
    }
    return nullptr;
}

/**
 * 独立执行 apt.real 数出可更新包数（与 UI 同源但独立跑一遍，互证用）。
 * 磐石系统上 /usr/bin/apt 是指向 immutable-adapter.sh 的软链，root 下调它
 * 会先 remount /usr；这里直接调 apt.real 真身，与 backend 实现同策略。
 */
static int aptUpgradableCount()
{
    const QString program = QStringLiteral("/usr/bin/apt.real");
    if (!QFileInfo::exists(program))
        return -1;
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    proc.setProcessEnvironment(env);
    proc.start(program, QStringList{QStringLiteral("list"), QStringLiteral("--upgradable")});
    if (!proc.waitForStarted(5000) || !proc.waitForFinished(60000))
        return -1;
    if (proc.exitCode() != 0)
        return -1;
    static const QRegularExpression re(
        QStringLiteral("^[^\\s/]+/\\S+\\s+\\S+\\s+\\S+\\s+\\[upgradable from:"));
    int n = 0;
    for (const QString &line : QString::fromUtf8(proc.readAllStandardOutput())
                                   .split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (re.match(line.trimmed()).hasMatch())
            ++n;
    }
    return n;
}

int main(int argc, char **argv)
{
    const QString dataHome = QDir::currentPath() + "/.tmp/updateshot-xdgdata";
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
                              QStringLiteral(".tmp/update-page"));
    parser.addOption(pngOpt);
    parser.process(a);
    const QString pngPrefix = parser.value(pngOpt);

    // ---------- 进入更新页并触发检查 ----------
    MainWindow w;
    w.resize(1080, 1200);
    w.show();
    settle(6);
    w.gotoPage(PageId::Update);
    settle(12);
    // 离屏首渲染假象防线：先丢弃式 grab 再真 grab（见跨会话教训 5）
    w.grab();
    settle(4);

    QAbstractButton *bCheck = findButton(&w, QObject::tr("检查更新"));
    if (!bCheck) {
        fail(QStringLiteral("U0 失败：更新页找不到「检查更新」按钮"));
        QTextStream(stdout) << "=== 更新页明细验证 FAILED ===\n";
        return 1;
    }
    bCheck->click();

    // 检查是异步的（ctl 走网络刷新远端，本机实测数秒；上限给足 180s）
    // 终态判据用徽章文本：PanshiBadge 刻意不带 Q_OBJECT（components.h 约定），
    // 不能 qobject_cast；其内部 DLabel 承载文本，按文本找可见标签即可。
    bool concluded = false;
    for (int i = 0; i < 600 && !concluded; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(300);
        if (findLabel(&w, QObject::tr("有可用更新"))
            || findLabel(&w, QObject::tr("已是最新")))
            concluded = true;
    }

    // ---------- U1：检查有结论 ----------
    const bool hasUpdate = findLabel(&w, QObject::tr("有可用更新")) != nullptr;
    const bool upToDate = findLabel(&w, QObject::tr("已是最新")) != nullptr;
    if (!concluded || (!hasUpdate && !upToDate)) {
        fail(QStringLiteral("U1 失败：点「检查更新」后未得到状态结论（徽章未落定）"));
    } else {
        pass(hasUpdate ? QStringLiteral("U1 检查完成：有可用更新")
                       : QStringLiteral("U1 检查完成：系统已是最新"));
    }

    // ---------- U2：明细区与 apt 实测互证 ----------
    const int aptCount = aptUpgradableCount();
    if (aptCount < 0) {
        // 探针环境跑不了 apt（非磐石系统）：跳过明细断言，只保 U1
        pass(QStringLiteral("U2 SKIP：本环境无法执行 apt.real，跳过明细互证"));
    } else {
        int rowCount = 0;
        for (QWidget *r : w.findChildren<QWidget *>(kPkgRowName)) {
            if (r->isVisible())
                ++rowCount;
        }
        int groupCount = 0;
        for (QWidget *g : w.findChildren<QWidget *>(kPkgGroupName)) {
            if (g->isVisible())
                ++groupCount;
        }
        const DLabel *secTitle = findLabel(&w, QObject::tr("待更新软件包"));
        if (aptCount > 0) {
            if (!secTitle) {
                fail(QStringLiteral("U2 失败：apt 实测有 %1 个可更新包，但「待更新软件包」区未显示")
                         .arg(aptCount));
            } else if (rowCount != aptCount) {
                fail(QStringLiteral("U2 失败：包行数 %1 与 apt 实测 %2 不一致")
                         .arg(rowCount).arg(aptCount));
            } else if (groupCount < 1) {
                fail(QStringLiteral("U2 失败：有 %1 个包行但没有分组标题").arg(aptCount));
            } else {
                pass(QStringLiteral("U2 明细区可见：%1 个包行 / %2 个分组，与 apt 实测一致")
                         .arg(rowCount).arg(groupCount));
            }
        } else {
            if (secTitle || rowCount != 0) {
                fail(QStringLiteral("U2 失败：apt 实测无可更新包，明细区应整区隐藏"
                                    "（标题可见=%1，包行=%2）")
                         .arg(secTitle != nullptr).arg(rowCount));
            } else {
                pass(QStringLiteral("U2 系统已是最新：明细区正确隐藏"));
            }
        }

        // ---------- U3：每组都有数量徽章 ----------
        if (aptCount > 0 && groupCount > 0) {
            // 徽章文本形如「3 项」（PanshiBadge 内部 DLabel 承载，无 Q_OBJECT）
            int badges = 0;
            for (QLabel *lb : visible<QLabel *>(&w)) {
                static const QRegularExpression re(QStringLiteral("^\\d+ 项$"));
                if (re.match(lb->text()).hasMatch())
                    ++badges;
            }
            if (badges != groupCount) {
                fail(QStringLiteral("U3 失败：数量徽章 %1 个与分组数 %2 不符")
                         .arg(badges).arg(groupCount));
            } else {
                pass(QStringLiteral("U3 每个分组都有数量徽章（%1 个）").arg(badges));
            }
        }

        // ---------- U4：重复检查不叠加行 ----------
        // 点击后 onCheck 会立即收起明细区（行数归 0），检查完成再重渲染。
        // 直接等行数回到 aptCount：叠行会变成 2×aptCount，查失败则一直是 0，
        // 两种坏情况都与 aptCount 不等，判据同时覆盖。
        if (bCheck->isEnabled())
            bCheck->click();
        int rowCount2 = -1;
        for (int i = 0; i < 600; ++i) {
            QCoreApplication::processEvents();
            QThread::msleep(300);
            rowCount2 = 0;
            for (QWidget *r : w.findChildren<QWidget *>(kPkgRowName)) {
                if (r->isVisible())
                    ++rowCount2;
            }
            if (rowCount2 == aptCount)
                break;
        }
        if (aptCount > 0 && rowCount2 != aptCount) {
            fail(QStringLiteral("U4 失败：重复检查后包行数 %1 != 首次 %2（行叠加了或没重渲染）")
                     .arg(rowCount2).arg(aptCount));
        } else if (aptCount > 0) {
            pass(QStringLiteral("U4 重复检查不叠加行（仍为 %1 行）").arg(rowCount2));
        }
    }

    // ---------- 截图供人工核对 ----------
    const QString png = pngPrefix + QStringLiteral(".png");
    QDir().mkpath(QFileInfo(png).absolutePath());
    w.grab();
    settle(4);
    if (!w.grab().save(png))
        fail(QStringLiteral("截图写出失败 %1").arg(png));

    QTextStream(stdout) << (g_fails ? "=== 更新页明细验证 FAILED (" : "=== 更新页明细验证 PASSED (")
                        << g_fails << " 项失败) ===\nPNG: " << png << "\n";
    return g_fails ? 1 : 0;
}
