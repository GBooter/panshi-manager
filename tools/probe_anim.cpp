// SPDX-License-Identifier: GPL-3.0-or-later
// 动画开关探针 panshi-animprobe（开发期专用，不安装）
//
// 用途：验证「减少动态效果」到底能不能被应用读到 —— 测的是**被测对象本身**
//       （PanshiUi::reduceMotionRequested），不是探针里复制的逻辑。
//
// 背景（缺陷 A2-3）：src/ui/components.cpp 的 reduceMotionRequested()
// 原先用 DConfig 读 `com.deepin.wrap.gnome.desktop.interface /
// enable-animations`，但该键属于 **GSettings**（glib gschema），DConfig 目录
// /usr/share/dsg/configs/ 下根本没有它 → dde-dconfig 实测报 `not exist
// resouce` → 功能一直静默失效。
//
// 双向验证（关键：只跑一次说明不了问题）：
//   1. 读 gsettings 真值；
//   2. 调被测函数，比对两者是否一致；
//   3. 用 PANSHI_ANIMPROBE_EXPECT=0|1 声明期望值，供自动化断言。
//   配合「改系统设置 → 重跑 → 恢复」即可证明是真读到了系统状态，
//   而不是碰巧返回了某个默认值。
//
// 用法：
//   ./panshi-animprobe
//   PANSHI_ANIMPROBE_EXPECT=0 ./panshi-animprobe   # 期望系统当前为「减少动效」

#include <QApplication>
#include <QProcess>
#include <QTextStream>

#include "ui/components.h"

static QString gsettingsAnimations()
{
    QProcess p;
    p.start(QStringLiteral("gsettings"),
            {QStringLiteral("get"),
             QStringLiteral("com.deepin.wrap.gnome.desktop.interface"),
             QStringLiteral("enable-animations")});
    if (!p.waitForFinished(3000))
        return QString();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    const QString gs = gsettingsAnimations();
    const bool gsHasAnimations = (gs == QLatin1String("true"));
    const bool reduce = PanshiUi::reduceMotionRequested();

    out << "gsettings  enable-animations = "
        << (gs.isEmpty() ? QStringLiteral("<读取失败>") : gs) << "\n";
    out << "被测函数  reduceMotionRequested() = " << (reduce ? "true" : "false") << "\n";

    if (gs.isEmpty()) {
        out << "SKIP  无法读取 gsettings，本轮不判定\n";
        return 0;
    }

    const bool consistent = (reduce == !gsHasAnimations);
    out << (consistent ? "PASS" : "FAIL") << "  "
        << (consistent ? "被测函数与系统真值一致"
                        : "被测函数与系统真值**不一致** —— 读的不是系统设置") << "\n";

    const QByteArray expect = qgetenv("PANSHI_ANIMPROBE_EXPECT");
    if (!expect.isEmpty()) {
        // EXPECT 语义：0 = 期望系统处于「减少动效」，1 = 期望系统处于「动效开启」
        const bool wantReduce = (expect == "0");
        const bool ok = consistent && (reduce == wantReduce);
        out << (ok ? "PASS" : "FAIL") << "  期望 reduce=" << (wantReduce ? "true" : "false")
            << "，实际 " << (reduce ? "true" : "false") << "\n";
        return ok ? 0 : 1;
    }
    return consistent ? 0 : 1;
}
