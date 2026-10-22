// SPDX-License-Identifier: GPL-3.0-or-later
//
// 英文界面残留中文 —— 离屏回归探针 panshi-langprobe（开发期专用，不安装）
//
// 目的（2026-10-22）：商店上架前用户反馈「切成英文后部分界面仍是中文」。
//   修复分三层：① 补 76 条 unfinished 译文 ② 头文件 tr() 进 TS（CMake 加 PANSHI_HEADERS）
//   ③ diskusagepage.cpp 3 处硬编码中文包 tr()。
//   ①②③ 里只有 ③ 的「监控设置」对话框两处 SpinBox 后缀（ 小时 / 天）在真机上
//   **点不到**（本机 xdotool 坐标点击在应用内容区不生效），故用离屏探针补上断言。
//
// 判据（黑盒，走控件树不碰成员变量）：
//   A1 磁盘页存在后缀为 " h" 的 SpinBox（tr(" 小时") 已生效）；
//   A2 磁盘页存在后缀为 " days" 的 SpinBox（tr(" 天") 已生效）；
//   A3 磁盘页**没有任何** SpinBox 后缀含 CJK（双向判据：回退成硬编码中文必 FAIL）；
//   A4 磁盘页可见标签里 CJK 占比为 0（统计输出，防止"A1/A2 单点通过、别处仍漏"）。
//
// 用法：QT_QPA_PLATFORM=offscreen ./panshi-langprobe
// 退出码 = 失败断言数 > 0 ? 1 : 0

#include <DApplication>
#include <QAbstractSpinBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTextStream>
#include <QTranslator>

#include "mainwindow.h"

#include <clocale>
#include <cstdlib>

// 注：MainWindow / PageId 都在**全局命名空间**（只有 PanshiUi / Panshi 才是命名空间）。
static int g_fail = 0;

static void check(bool ok, const QString &what)
{
    QTextStream o(stdout);
    o << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    o.flush();
    if (!ok)
        ++g_fail;
}

/** 含中日韩统一表意文字即视为「未翻译的中文残留」 */
static bool hasCjk(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("[\\x{4E00}-\\x{9FFF}]"));
    return re.match(s).hasMatch();
}

int main(int argc, char **argv)
{
    const QString dataHome = QDir::currentPath() + "/.tmp/langprobe-xdgdata";
    QDir().mkpath(dataHome);
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());
    qputenv("QT_QPA_PLATFORM", "offscreen");

    // ⚠ 语言必须在 QApplication 构造**之前**定下来：DTK 的 loadTranslator()
    //   走 QLocale::system()（进程启动时由 C 库缓存），运行期再改环境变量已太晚
    //   （2026-10-06 三轮实证，详见 src/main.cpp 注释）。
    qunsetenv("LC_ALL");
    setlocale(LC_ALL, "en_US.UTF-8");
    qputenv("LANGUAGE", "en_US");
    qputenv("LANG", "en_US.UTF-8");

    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-manager"));
    // 与主程序一致地注入版本号：关于页读 applicationVersion() 渲染版本行，
    // 这里不注入，A5 测的就是空串（2026-10-22 实测踩到）。
    app.setApplicationVersion(QStringLiteral(PANSHI_VERSION));
    app.loadTranslator();

    // 显式兜底装载（与主程序同一套搜索顺序，后装优先覆盖 DTK 那一份）
    static QTranslator g_tr;
    const QStringList dirs = {
        QDir::homePath() + QStringLiteral("/.local/share/panshi-manager/translations"),
        QStringLiteral("/usr/share/panshi-manager/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QCoreApplication::applicationDirPath(),
    };
    bool loaded = false;
    for (const QString &d : dirs) {
        if (g_tr.load(QStringLiteral("panshi-manager_en_US.qm"), d)) {
            QCoreApplication::installTranslator(&g_tr);
            loaded = true;
            break;
        }
    }
    QTextStream o(stdout);
    if (!loaded) {
        o << "FAIL: 未找到 panshi-manager_en_US.qm，无法验证英文界面\n";
        o.flush();
        return 1;
    }

    MainWindow w;
    w.resize(1080, 1100);
    w.show();
    for (int i = 0; i < 6; ++i)
        QCoreApplication::processEvents();
    w.gotoPage(PageId::DiskUsage);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();

    // ---- A1 / A2 / A3：SpinBox 后缀（含未显示的「监控设置」对话框，它是 page 的子对象）----
    // DSuffix 只存在于 QSpinBox / QDoubleSpinBox（QAbstractSpinBox 没有 suffix()），
    // DTK 的 DSpinBox 走 QDoubleSpinBox 一支 → 两类都扫，避免漏掉任何一方。
    QStringList suffixes;
    for (QDoubleSpinBox *s : w.findChildren<QDoubleSpinBox *>())
        suffixes << s->suffix();
    for (QSpinBox *s : w.findChildren<QSpinBox *>())
        suffixes << s->suffix();

    o << "\n== 磁盘页 SpinBox 后缀（共 " << suffixes.size() << " 个）==\n";
    for (const QString &s : suffixes)
        o << "  [" << s << "]\n";
    o.flush();

    check(suffixes.contains(QStringLiteral(" h")),
          QStringLiteral("A1 存在后缀 \" h\"（tr(\" 小时\") 已译为英文）"));
    check(suffixes.contains(QStringLiteral(" days")),
          QStringLiteral("A2 存在后缀 \" days\"（tr(\" 天\") 已译为英文）"));

    QStringList cjkSuffixes;
    for (const QString &s : suffixes)
        if (hasCjk(s))
            cjkSuffixes << s;
    check(cjkSuffixes.isEmpty(),
          QStringLiteral("A3 无任何 SpinBox 后缀含中文（当前：%1）")
              .arg(cjkSuffixes.isEmpty() ? QStringLiteral("无")
                                         : cjkSuffixes.join(QStringLiteral(" / "))));

    // ---- A4：可见标签残留统计（只报告不断言：路径/版本号等本就可能带非 ASCII）----
    int labels = 0, cjkLabels = 0;
    QStringList samples;
    for (QLabel *l : w.findChildren<QLabel *>()) {
        if (!l->isVisible())
            continue;
        ++labels;
        if (hasCjk(l->text())) {
            ++cjkLabels;
            if (samples.size() < 8)
                samples << l->text();
        }
    }
    o << "\nA4 可见标签 " << labels << " 个，其中含中文 " << cjkLabels << " 个\n";
    for (const QString &s : samples)
        o << "    中文残留: " << s << "\n";
    o.flush();

    // ---- A5：关于页版本号必须等于编译期注入的版本（1.0.0 硬编码漂移的回归）----
    // 背景（2026-10-22）：版本号曾五处各写一份字面量，包发到 1.0.5 界面仍显示 1.0.0。
    // 现改为 CMake project(VERSION) → PANSHI_VERSION 宏 → applicationVersion()。
    // 判据：英文界面下关于页必须出现含 PANSHI_VERSION 的可见标签
    //       （译文模板 "Version %1  ·  Deepin 25 immutable system management tool"）。
    w.gotoPage(PageId::About);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    QString aboutVerText;
    for (QLabel *l : w.findChildren<QLabel *>()) {
        if (l->isVisible() && l->text().contains(QStringLiteral(PANSHI_VERSION)))
            aboutVerText = l->text();
    }
    check(!aboutVerText.isEmpty(),
          QStringLiteral("A5 关于页版本行含当前版本 %1（实际：%2）")
              .arg(QStringLiteral(PANSHI_VERSION),
                   aboutVerText.isEmpty() ? QStringLiteral("未找到") : aboutVerText));

    o << "\n";
    if (g_fail == 0) {
        o << "PASS: 磁盘页（含监控设置对话框）无中文残留\n";
        o.flush();
        return 0;
    }
    o << "FAIL: " << g_fail << " 条断言不通过\n";
    o.flush();
    return 1;
}
