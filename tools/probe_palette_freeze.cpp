// SPDX-License-Identifier: GPL-3.0-or-later
//
// 诊断：DTK6 下 qApp->setPalette() 是否会**永久破坏**后续主题切换
//
// 探针 v1（probe_dark_theme.cpp）的实测结果反直觉但极有信息量：
//   「带 setPalette」与「不带 setPalette」两条路径，亮/暗四个组合的像素**完全相同**，
//   且暗色下卡片底仍是 #ffffff（= 没切主题）。
//   → 说明 setPalette() 不是「把界面钉住」那么简单，而是让 setPaletteType()
//     **整个失效**。这正对应日志里第二条警告：
//     "DGuiApplicationHelper: Plase check 'QGuiApplication::setPalette',
//      Don't use it on DTK application."   ← 由 setPaletteType() 发出
//
// 本探针把这条因果链一步步量出来：
//   步骤 1  全新进程，未碰过 qApp->setPalette
//           记录 QApplication::palette().Base/WindowText 与 applicationPalette() 的对应值
//   步骤 2  setPaletteType(DarkType)
//           再记录同一组值 → 若 QApplication 侧不变，即坐实「QGuiApplication 侧冻结」
//   步骤 3  调 qApp->setPalette(暗色 applicationPalette())   ← 复刻 main.cpp 的做法
//   步骤 4  再 setPaletteType(LightType) 回去
//           看 QApplication 侧是否**再也回不到亮色** → 坐实「不可逆」
//
// 判据（对比度 + 角色一致性，两条腿）：
//   A. appQPalette.color(Base) 是否随 paletteType 变化
//   B. appQPalette.color(WindowText) 是否随 paletteType 变化
//   C. appQPalette 与 applicationPalette 的 Base 是否**始终**一致
//
// 用法：panshi-palettetest            # 输出 JSON + stderr 断言
//       panshi-palettetest --recover  # 额外演示「不调 setPalette 时能否正常来回切」

#include <QApplication>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DPalette>

#include <cmath>

// ⚠️ Dtk::Gui 是**普通命名空间**（非 inline）：新 TU 必须自己写这一句。
// 其它文件能裸写 DGuiApplicationHelper 是因为它们间接包含了
// dplatformhandle.h —— 依赖间接 include 带来的 using 声明很脆弱。
DGUI_USE_NAMESPACE

namespace {

QString hx(const QColor &c)
{
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(), 2, 16, QLatin1Char('0'));
}

double relLum(const QColor &c)
{
    auto f = [](int v) {
        double x = v / 255.0;
        return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * f(c.red()) + 0.7152 * f(c.green()) + 0.0722 * f(c.blue());
}

double contrast(const QColor &a, const QColor &b)
{
    const double la = relLum(a), lb = relLum(b);
    const double hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

int g_failed = 0;

void check(const QString &name, bool ok, const QString &detail)
{
    if (!ok)
        ++g_failed;
    QTextStream err(stderr);
    err << (ok ? "ok   " : "FAIL ") << name << "  " << detail << "\n";
    err.flush();
}

QJsonObject snap(const QString &step)
{
    const DPalette appPal = DGuiApplicationHelper::instance()->applicationPalette();
    const QPalette qPal = qApp->palette();

    QJsonObject o;
    o[QStringLiteral("step")] = step;
    o[QStringLiteral("paletteType")] =
        DGuiApplicationHelper::instance()->paletteType() == DGuiApplicationHelper::DarkType
            ? QStringLiteral("dark") : QStringLiteral("light");
    // applicationPalette（自绘组件走这条）
    o[QStringLiteral("dpal_Base")] = hx(appPal.color(DPalette::Base));
    o[QStringLiteral("dpal_Window")] = hx(appPal.color(DPalette::Window));
    o[QStringLiteral("dpal_TextTitle")] = hx(appPal.color(DPalette::TextTitle));
    o[QStringLiteral("dpal_TextTips")] = hx(appPal.color(DPalette::TextTips));
    // QApplication::palette（Qt 原生控件 + DLabel 角色走这条）
    o[QStringLiteral("qpal_Base")] = hx(qPal.color(QPalette::Base));
    o[QStringLiteral("qpal_Window")] = hx(qPal.color(QPalette::Window));
    o[QStringLiteral("qpal_WindowText")] = hx(qPal.color(QPalette::WindowText));
    o[QStringLiteral("qpal_Text")] = hx(qPal.color(QPalette::Text));
    // 两侧是否一致
    o[QStringLiteral("base_match")] = (appPal.color(DPalette::Base) == qPal.color(QPalette::Base));
    o[QStringLiteral("windowText_contrast_in_app")] =
        QString::number(contrast(qPal.color(QPalette::WindowText), qPal.color(QPalette::Base)), 'f', 2);
    return o;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-palettetest"));

    QCommandLineParser p;
    p.addHelpOption();
    p.process(app);

    QJsonObject root;
    QJsonArray steps;

    auto flush = [&] { QApplication::processEvents(); QApplication::processEvents(); };

    // ⚠️⚠️ 本探针**必须在真实会话下跑**（2026-10-05 实测，踩过坑）：
    //   offscreen 没有平台主题插件（/usr/lib/.../qt6/plugins/platformthemes/libqdeepin.so），
    //   DTK 不会把主题变化同步进 QApplication::palette()。实测同一探针：
    //     真实 X11：setPaletteType(Dark) 后 qpal.Base 跟随变 #282828（match=True）
    //     offscreen：setPaletteType(Dark) 后 qpal.Base **仍是 #ffffff**（match=False）
    //   而步骤 2 的断言恰恰是「qpal 应跟随」—— offscreen 下它**必然 FAIL**，
    //   会让人误判成「DTK 调色板不同步」并据此写出错误的修复。
    //   故这里显式拦截：offscreen 直接退出并说明，避免拿假结论当证据。
    if (qgetenv("QT_QPA_PLATFORM") == QByteArray("offscreen")) {
        QTextStream err(stderr);
        err << "本探针必须在真实会话下运行（QT_QPA_PLATFORM=offscreen 会产生假结论）。\n"
               "  实测：offscreen 下 setPaletteType() 不同步 QApplication::palette()，\n"
               "        真实 X11 下则正常同步 —— 差异来自平台主题插件 libqdeepin.so。\n"
               "  请直接运行：./build/panshi-palettetest\n";
        err.flush();
        return 2;
    }

    // ---------- 步骤 1：全新进程，默认主题 ----------
    flush();
    steps.append(snap(QStringLiteral("1-initial")));
    const QJsonObject s1 = steps.last().toObject();
    const QString initialBase = s1[QStringLiteral("qpal_Base")].toString();

    // ---------- 步骤 2：setPaletteType(Dark) ----------
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    flush();
    steps.append(snap(QStringLiteral("2-after-setDark")));
    const QJsonObject s2 = steps.last().toObject();
    const QString darkBase = s2[QStringLiteral("qpal_Base")].toString();
    const QString darkDpalBase = s2[QStringLiteral("dpal_Base")].toString();

    check(QStringLiteral("步骤2 setPaletteType(Dark) 后 applicationPalette().Base 变暗"),
          darkDpalBase != initialBase,
          QStringLiteral("dpal.Base: %1 → %2").arg(initialBase, darkDpalBase));
    check(QStringLiteral("步骤2 QApplication::palette().Base 跟随变暗"),
          darkBase != initialBase,
          QStringLiteral("qpal.Base: %1 → %2").arg(initialBase, darkBase));
    check(QStringLiteral("步骤2 两侧 Base 一致"),
          s2[QStringLiteral("base_match")].toBool(),
          QStringLiteral("dpal=%1 qpal=%2").arg(darkDpalBase, darkBase));

    // ---------- 步骤 3：复刻 main.cpp —— qApp::setPalette(暗色) ----------
    qApp->setPalette(DGuiApplicationHelper::instance()->applicationPalette());
    flush();
    steps.append(snap(QStringLiteral("3-after-manual-setPalette-dark")));

    // ---------- 步骤 4：切回亮色，看 QApplication 侧能否恢复 ----------
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    flush();
    steps.append(snap(QStringLiteral("4-back-to-light")));
    const QJsonObject s4 = steps.last().toObject();
    const QString backBase = s4[QStringLiteral("qpal_Base")].toString();
    const QString backDpalBase = s4[QStringLiteral("dpal_Base")].toString();

    check(QStringLiteral("步骤4 切回亮色后 applicationPalette().Base 已恢复"),
          backDpalBase == initialBase,
          QStringLiteral("dpal.Base: %1（期望 %2）").arg(backDpalBase, initialBase));
    check(QStringLiteral("步骤4 QApplication::palette().Base 也已恢复（未卡在暗色）"),
          backBase == initialBase,
          QStringLiteral("qpal.Base: %1（期望 %2）").arg(backBase, initialBase));
    check(QStringLiteral("步骤4 两侧 Base 重新一致"),
          s4[QStringLiteral("base_match")].toBool(),
          QStringLiteral("dpal=%1 qpal=%2").arg(backDpalBase, backBase));

    // ---------- 步骤 5：再切一次暗色，验证「双向」而非单向 ----------
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    flush();
    steps.append(snap(QStringLiteral("5-dark-again")));
    const QJsonObject s5 = steps.last().toObject();
    check(QStringLiteral("步骤5 二次切暗，QApplication::palette().Base 仍跟随"),
          s5[QStringLiteral("qpal_Base")].toString() == darkBase,
          QStringLiteral("qpal.Base=%1（期望 %2）")
              .arg(s5[QStringLiteral("qpal_Base")].toString(), darkBase));

    // ---------- 步骤 6：setApplicationPalette 是否是「受支持的替代品」 ----------
    // 关键在**调用时机**：在 paletteType 已切到 Dark 之后再把当前 applicationPalette
    // 灌回去。若这一步能让 QApplication 侧跟上且**后续仍可再来回切**，
    // 它就是 qApp::setPalette 的合法替代；否则两条路都不可用。
    {
        DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
        flush();
        DGuiApplicationHelper::instance()->setApplicationPalette(
            DGuiApplicationHelper::instance()->applicationPalette());
        flush();
        steps.append(snap(QStringLiteral("6-setAppPalette-in-light")));
    }
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    flush();
    steps.append(snap(QStringLiteral("6b-after-setAppPalette-then-dark")));
    const QJsonObject s6 = steps.last().toObject();
    check(QStringLiteral("步骤6 setApplicationPalette 后再切暗，dpal.Base 变暗"),
          s6[QStringLiteral("dpal_Base")].toString() != backDpalBase,
          QStringLiteral("dpal.Base=%1（亮色时为 %2）")
              .arg(s6[QStringLiteral("dpal_Base")].toString(), backDpalBase));
    check(QStringLiteral("步骤6 setApplicationPalette 后再切暗，qpal.Base 跟着变暗"),
          s6[QStringLiteral("qpal_Base")].toString() == s6[QStringLiteral("dpal_Base")].toString(),
          QStringLiteral("qpal=%1 dpal=%2")
              .arg(s6[QStringLiteral("qpal_Base")].toString(),
                   s6[QStringLiteral("dpal_Base")].toString()));

    // 再切回亮色，检验可逆性
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    flush();
    steps.append(snap(QStringLiteral("6c-setAppPalette-back-to-light")));
    const QJsonObject s6c = steps.last().toObject();
    check(QStringLiteral("步骤6c setApplicationPalette 方案仍可逆（切回亮色不卡住）"),
          s6c[QStringLiteral("dpal_Base")].toString() == initialBase,
          QStringLiteral("dpal.Base=%1（期望 %2）")
              .arg(s6c[QStringLiteral("dpal_Base")].toString(), initialBase));

    root[QStringLiteral("probe")] = QStringLiteral("panshi-palettetest");
    root[QStringLiteral("steps")] = steps;
    root[QStringLiteral("failed")] = g_failed;

    QTextStream out(stdout);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    out.flush();
    return g_failed > 0 ? 1 : 0;
}
