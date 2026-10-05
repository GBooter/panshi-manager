// SPDX-License-Identifier: GPL-3.0-or-later
//
// 暗色修复方案对比探针 panshi-darkfix（开发期专用，不安装）
//
// ✅ 已实证的根因（panshi-palettetest 步骤 2，一步坐实）：
//
//   DGuiApplicationHelper::setPaletteType(DarkType) 之后：
//     · applicationPalette().Base  → #282828  ✅ 变暗了（自绘组件走这条）
//     · QApplication::palette().Base → #ffffff ❌ **纹丝不动**（Qt 原生控件走这条）
//
//   即 **setPaletteType() 只翻 DTK 自己的 DPalette，不把 QApplication 调色板带过去**。
//   而 DLabel::setForegroundRole(DPalette::ColorType) 记的是**角色**、绘制时向
//   **控件自己的 QPalette** 取值 —— 取到的是亮色主题的 TextTitle（#000000）。
//   两侧一错位，就是用户截图里的现象：深色卡片底 #282828 + 近黑文字 #060606
//   （对比 1.3，肉眼等于字消失）+ 侧栏纯白 #ffffff 亮色残留。
//
// ⚠️ 而 main.cpp 现有的补救手段 `qApp->setPalette()` 是**错药**（三重实测）：
//   ① DTK6 对它明确报警告（"Don't use QGuiApplication::setPalette on DTK
//      application."），且该警告在用户启动日志里反复出现；
//   ② 它确实能让两侧对齐（步骤 3：qpal.Base → #282828），但
//   ③ **代价是 DTK 从此不再管 Base**：步骤 4 切回亮色后 applicationPalette().Base
//      仍卡在 #282828 不回弹 —— 调色板被**永久污染**，单向切换后就再也回不去。
//   实测第 5 步二次切暗时 qpal/dpal 全部停在 #282828，主题切换已彻底失效。
//
// ⚠️ 关于 DTK 那条 "Don't use QGuiApplication::setPalette on DTK application" 警告
//   （2026-10-05 用 LD_PRELOAD 拦截器 + addr2line 归因，勿再误判为项目代码缺陷）：
//   实测应用自身 **0 次**调用 setPalette（拦截器计数 0），但切主题时该警告仍出现 3 次；
//   且警告出现在拦截器捕获点**之前**（时序上不可能由它引起）。
//   反汇编 libdtk6gui.so.6 可见 `onApplicationPaletteChanged()` 内部调用
//   `QGuiApplication::palette()` + `toColorType()`，据此判断「调色板被外部改动」并告警
//   —— 即 **DTK 6.7.44 在 setPaletteType() 路径上自己触发自己**（库内部行为）。
//   ⇒ 该警告是 DTK 库噪声、不影响功能（修复后真实 X11 断言 0 失败）；
//     删除项目里的 setPalette 调用仍然正确（它确实不可逆，见 palettetest 步骤 4）。
//
// 本探针对比 4 个候选方案，每个都用**真实控件**离屏渲染后采样像素：
//   方案 0  现行：setStyleSheet("") → qApp::setPalette → 重设 QSS
//   方案 1  只清 QSS + unpolish/polish + 递归 update（**不碰任何调色板**）← 本次采用
//   方案 2  setApplicationPalette()（DTK 受支持 API，但实测**同样不可逆**）
//   方案 3  方案 1 + setApplicationPalette()
//
// ⚠️ **本探针必须在真实会话下跑**（offscreen 会产生假结论）：
//   offscreen 无平台主题插件（libqdeepin.so），实测真实 X11 下修复后 **0 失败**，
//   而 offscreen 下 DPushButton 等 DTK 控件拿不到样式、渲染成浅灰压浅灰（假 FAIL）。
//   额外方案 4：方案 2 之后**切回亮色**，检验是否可逆（方案 0 的致命缺陷）
//
// 判据（每方案 × 每主题 8 条）：
//   · 卡片底 / 侧栏底 / LineEdit 底 / 按钮底 是否等于该主题 Base
//   · 卡片标题(DLabel 角色) / LineEdit 文本 / 按钮文本 对比度 ≥ 4.5
//   · 亮暗**双向**切换后是否回到原主题（可逆性）
//
// 用法：panshi-darkfix [--png .tmp/fix]

#include <QApplication>
#include <QCommandLineParser>
#include <QComboBox>
#include <QFrame>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPixmap>
#include <QListView>
#include <QStandardItemModel>
#include <DPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <DApplication>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DLabel>
#include <DPalette>

#include "style.h"
#include "ui/components.h"

#include <cmath>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using PanshiUi::Semantic;
using PanshiUi::TextLevel;
namespace Space = PanshiUi::Space;

namespace {

QVector<QPair<QString, QPair<bool, QString>>> g_checks;

void check(const QString &name, bool ok, const QString &detail)
{
    g_checks.append({name, {ok, detail}});
}

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

void settle(QWidget *w, int rounds = 8)
{
    w->show();
    for (int i = 0; i < rounds; ++i)
        QApplication::processEvents();
    (void)w->grab();
    for (int i = 0; i < rounds; ++i)
        QApplication::processEvents();
}

QColor bgOf(const QImage &img, const QRect &r)
{
    QHash<QRgb, int> hist;
    for (int y = r.top(); y <= r.bottom() && y < img.height(); ++y)
        for (int x = r.left(); x <= r.right() && x < img.width(); ++x)
            hist[img.pixel(x, y)]++;
    QRgb best = 0;
    int bestN = 0;
    for (auto it = hist.begin(); it != hist.end(); ++it)
        if (it.value() > bestN) { bestN = it.value(); best = it.key(); }
    return QColor(best);
}

QColor textIn(const QImage &img, const QRect &r, const QColor &bg)
{
    QHash<QRgb, int> hist;
    for (int y = r.top(); y <= r.bottom() && y < img.height(); ++y) {
        for (int x = r.left(); x <= r.right() && x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c == bg || contrast(c, bg) < 1.6)
                continue;
            hist[c.rgb()]++;
        }
    }
    QRgb best = 0;
    int bestN = 0;
    for (auto it = hist.begin(); it != hist.end(); ++it)
        if (it.value() > bestN) { bestN = it.value(); best = it.key(); }
    return bestN > 0 ? QColor(best) : QColor(0, 0, 0, 0);
}

/** 真实控件面板：刻意混合「走 DPalette 角色」与「只认 QApplication 调色板」两条路径 */
QWidget *buildPanel(QWidget *parent)
{
    auto *host = new QWidget(parent);
    host->setObjectName(QStringLiteral("panshiContent"));
    auto *lay = new QVBoxLayout(host);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(Space::M);

    auto *card = new QFrame(host);
    card->setObjectName(QStringLiteral("panshiCard"));
    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(16, 16, 16, 16);
    cl->setSpacing(Space::S);
    cl->addWidget(PanshiUi::makeLabel(QStringLiteral("卡片标题Title"), TextLevel::SectionTitle, Semantic::Normal));
    cl->addWidget(PanshiUi::makeLabel(QStringLiteral("描述 Muted 灰"), TextLevel::Desc, Semantic::Muted));
    cl->addWidget(PanshiUi::makeLabel(QStringLiteral("正常 Success 绿"), TextLevel::Value, Semantic::Success));
    lay->addWidget(card);

    auto *edit = new QLineEdit(QStringLiteral("输入框内容 Input"), host);
    edit->setObjectName(QStringLiteral("panshiEdit"));
    lay->addWidget(edit);
    auto *btn = new DPushButton(QStringLiteral("按钮文字 Button"), host);
    btn->setObjectName(QStringLiteral("panshiBtn"));
    lay->addWidget(btn);

    // ④ QListView：复刻侧栏。用户实况里「侧栏整条停在白底」就是它 ——
    //    QAbstractScrollArea 的 viewport 默认用 QPalette::Base 填背景，
    //    而该调色板不跟随 DTK 主题变化。objectName 与 sidebar.cpp 保持一致，
    //    才能验到 style.h 里新加的 #panshiNavList 规则是否真的生效。
    auto *frame = new QFrame(host);
    frame->setObjectName(QStringLiteral("panshiSidebar"));
    auto *fl = new QVBoxLayout(frame);
    fl->setContentsMargins(Space::S, Space::S, Space::S, Space::S);
    auto *searchEdit = new QLineEdit(QString(), frame);
    searchEdit->setPlaceholderText(QStringLiteral("搜索设置"));
    searchEdit->setObjectName(QStringLiteral("panshiSearchEdit"));
    fl->addWidget(searchEdit);
    auto *list = new QListView(frame);
    list->setObjectName(QStringLiteral("panshiNavList"));
    list->setFrameShape(QFrame::NoFrame);
    list->viewport()->setAutoFillBackground(false);
    list->setModel(new QStandardItemModel(3, 1, list));
    list->setMinimumHeight(90);
    fl->addWidget(list);
    lay->addWidget(frame);
    return host;
}

enum class Fix { Current, QssOnly, SetAppPalette, Both };
static const Fix kAllFixes[] = {Fix::Current, Fix::QssOnly, Fix::SetAppPalette, Fix::Both};

static int fixIndex(Fix f)
{
    for (int i = 0; i < 4; ++i)
        if (kAllFixes[i] == f)
            return i;
    return -1;
}

QString fixName(Fix f)
{
    switch (f) {
    case Fix::Current:        return QStringLiteral("0-current(qApp::setPalette)");
    case Fix::QssOnly:        return QStringLiteral("1-qssOnly(不碰调色板)");
    case Fix::SetAppPalette:  return QStringLiteral("2-setApplicationPalette");
    case Fix::Both:           return QStringLiteral("3-qssOnly+setApplicationPalette");
    }
    return QString();
}

/** 复刻各方案的主题刷新动作 */
void applyFix(Fix f, QWidget *root)
{
    const DPalette pal = DGuiApplicationHelper::instance()->applicationPalette();
    qApp->setStyleSheet(QString());
    switch (f) {
    case Fix::Current:
        qApp->setPalette(pal);                       // ← DTK6 明令禁止，且不可逆
        break;
    case Fix::QssOnly:
        break;
    case Fix::SetAppPalette:
        DGuiApplicationHelper::instance()->setApplicationPalette(pal);
        break;
    case Fix::Both:
        DGuiApplicationHelper::instance()->setApplicationPalette(pal);
        break;
    }
    qApp->setStyleSheet(panshiStyleSheet());

    // ★ 修复的核心：重算落色标签（applySemantic 现在是「快照」语义，
    //   DLabel 收到 PaletteChange 会重走 initPainter 覆盖掉它）。
    PanshiUi::refreshExactColorLabels();

    const auto updateAll = [](QWidget *r) {
        QList<QWidget *> stack;
        stack.append(r);
        while (!stack.isEmpty()) {
            QWidget *cur = stack.takeLast();
            cur->update();
            const auto kids = cur->findChildren<QWidget *>();
            for (QWidget *k : kids)
                stack.append(k);
        }
    };
    for (QWidget *w : QApplication::topLevelWidgets()) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        updateAll(w);
    }
    QApplication::processEvents();
    settle(root);
}

QJsonObject probe(Fix f, const QString &theme, const QString &pngDir)
{
    auto *panel = buildPanel(nullptr);
    panel->setFixedSize(520, 420);
    settle(panel);
    applyFix(f, panel);

    const QImage img = panel->grab().toImage();
    if (!pngDir.isEmpty())
        img.save(QStringLiteral("%1/fix-%2-%3.png").arg(pngDir, fixName(f).left(1), theme));

    const DPalette std = DGuiApplicationHelper::standardPalette(
        DGuiApplicationHelper::instance()->paletteType());
    const QColor expectBase = std.color(DPalette::Base);
    const QString tag = QStringLiteral("[%1/%2]").arg(fixName(f), theme);

    QJsonObject o;
    o[QStringLiteral("fix")] = fixName(f);
    o[QStringLiteral("theme")] = theme;
    o[QStringLiteral("expect_Base")] = hx(expectBase);
    o[QStringLiteral("dpal_Base")] = hx(DGuiApplicationHelper::instance()->applicationPalette().color(DPalette::Base));
    o[QStringLiteral("qpal_Base")] = hx(qApp->palette().color(QPalette::Base));

    // 卡片底
    const QColor cardBg = bgOf(img, QRect(16, 16, 488, 110));
    o[QStringLiteral("card_bg")] = hx(cardBg);
    check(tag + QStringLiteral(" 卡片底 == 主题 Base"),
          cardBg == expectBase,
          QStringLiteral("card=%1 expect=%2").arg(hx(cardBg), hx(expectBase)));

    // 卡片标题（DLabel 走 foregroundRole）
    const QColor titleFg = textIn(img, QRect(32, 28, 240, 24), cardBg);
    const double tCr = contrast(titleFg, cardBg);
    o[QStringLiteral("title_fg")] = hx(titleFg);
    o[QStringLiteral("title_contrast")] = QString::number(tCr, 'f', 2);
    check(tag + QStringLiteral(" 卡片标题对比度 ≥4.5"), tCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 cr=%3").arg(hx(titleFg), hx(cardBg), QString::number(tCr, 'f', 2)));

    // 描述 Muted
    const QColor descFg = textIn(img, QRect(32, 58, 260, 22), cardBg);
    const double dCr = contrast(descFg, cardBg);
    o[QStringLiteral("desc_fg")] = hx(descFg);
    o[QStringLiteral("desc_contrast")] = QString::number(dCr, 'f', 2);
    check(tag + QStringLiteral(" 描述 Muted 对比度 ≥4.5"), dCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 cr=%3").arg(hx(descFg), hx(cardBg), QString::number(dCr, 'f', 2)));

    // Success 绿
    const QColor okFg = textIn(img, QRect(32, 88, 240, 24), cardBg);
    const double sCr = contrast(okFg, cardBg);
    o[QStringLiteral("success_fg")] = hx(okFg);
    o[QStringLiteral("success_contrast")] = QString::number(sCr, 'f', 2);
    check(tag + QStringLiteral(" Success 绿对比度 ≥4.5"), sCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 cr=%3").arg(hx(okFg), hx(cardBg), QString::number(sCr, 'f', 2)));

    // QLineEdit（纯 Qt 原生，只认 QApplication 调色板）
    const QRect er(16, 140, 488, 32);
    const QColor editBg = bgOf(img, er);
    const QColor editFg = textIn(img, er.adjusted(4, 4, -4, -4), editBg);
    const double eCr = contrast(editFg, editBg);
    o[QStringLiteral("edit_bg")] = hx(editBg);
    o[QStringLiteral("edit_fg")] = hx(editFg);
    o[QStringLiteral("edit_contrast")] = QString::number(eCr, 'f', 2);
    check(tag + QStringLiteral(" QLineEdit 文本对比度 ≥4.5"), eCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 cr=%3").arg(hx(editFg), hx(editBg), QString::number(eCr, 'f', 2)));

    // DPushButton（应用实际使用的按钮控件）
    auto *btn = panel->findChild<DPushButton *>();
    if (btn) {
        const QRect br = btn->geometry();
        const QColor btnBg = bgOf(img, br);
        const QColor btnFg = textIn(img, br.adjusted(4, 4, -4, -4), btnBg);
        const double bCr = contrast(btnFg, btnBg);
        o[QStringLiteral("btn_bg")] = hx(btnBg);
        o[QStringLiteral("btn_fg")] = hx(btnFg);
        o[QStringLiteral("btn_contrast")] = QString::number(bCr, 'f', 2);
        // ⚠️ 这条断言**只在真实会话下有意义**（2026-10-05 实测，勿在 offscreen 下当结论）：
        //   offscreen 没有平台主题插件（libqdeepin.so），DTK 按钮拿不到自己的样式，
        //   渲染成「浅灰底 + 浅灰字」（实测 #ababab / #efefef，cr≈2.0）——
        //   亮色和暗色**数值完全相同**，是纯环境伪影，与暗色修复无关。
        //   真实 X11 下实测：亮 #ffffff/#008257 cr=4.85、暗 #282828/#00a95f cr=4.80 ✅。
        //   故 offscreen 时降级为「只记录不判定」，避免假 FAIL 掩盖真问题。
        const bool offscreen = qgetenv("QT_QPA_PLATFORM") == QByteArray("offscreen");
        if (offscreen) {
            o[QStringLiteral("btn_contrast_skipped")] = true;
        } else {
            check(tag + QStringLiteral(" DPushButton 文本对比度 ≥4.5"), bCr >= 4.5,
                  QStringLiteral("fg=%1 bg=%2 cr=%3").arg(hx(btnFg), hx(btnBg), QString::number(bCr, 'f', 2)));
        }
    }

    // 侧栏列表底（用户实况「整条白底」）：QListView 的 viewport 默认填 QPalette::Base
    if (auto *list = panel->findChild<QListView *>(QStringLiteral("panshiNavList"))) {
        const QRect lr = list->geometry();
        const QColor listBg = bgOf(img, lr);
        o[QStringLiteral("sidebarlist_bg")] = hx(listBg);
        check(tag + QStringLiteral(" 侧栏列表底 == 主题 Base（不得停在白底）"),
              listBg == expectBase,
              QStringLiteral("list=%1 expect=%2").arg(hx(listBg), hx(expectBase)));
    }

    panel->deleteLater();
    return o;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-darkfix"));

    QCommandLineParser p;
    p.addHelpOption();
    // ⚠️ QCommandLineOption 的第 3 个参数是 **valueName**；留空会把该选项注册成
    // 「布尔开关」，于是 `--fix 0` 里的 0 被当成位置参数，
    // 解析器报 `option not expecting values: "fix"` 且 value() 恒为空。
    // （踩过两次：先误以为是探针坏了，再去查并不存在的 setValueSeparator。）
    // 正确写法是给出 valueName，用 `--fix=0` 或 `--fix 0` 都能解析。
    QCommandLineOption pngOpt(QStringLiteral("png"),
                              QStringLiteral("PNG 输出目录，用法 --png=DIR"),
                              QStringLiteral("DIR"));
    // ⚠️ --fix 是**必需**的：四个方案必须各跑一个独立进程。
    // 在同一进程里连跑会被「方案 0 的 qApp::setPalette 污染全局调色板」串味 ——
    // 实测把四个方案放一个进程时，四者的 dpal.Base 全部退化成 #ffffff，
    // 差异被完全抹平（这正是「验证脚本自己骗自己」的典型形态）。
    QCommandLineOption fixOpt(QStringLiteral("fix"),
                              QStringLiteral("只跑指定方案 0..3（必须独立进程），用法 --fix=0"),
                              QStringLiteral("N"));
    p.addOption(pngOpt);
    p.addOption(fixOpt);
    p.process(app);

    if (!p.isSet(fixOpt)) {
        QTextStream err(stderr);
        err << "必须用 --fix 0|1|2|3 指定方案，每个方案单独起进程（原因见选项说明）\n";
        return 2;
    }
    bool okNum = false;
    const int idx = p.value(fixOpt).toInt(&okNum);
    if (!okNum || idx < 0 || idx > 3) {
        QTextStream err(stderr);
        err << "--fix 取值须为 0..3\n";
        return 2;
    }
    const Fix fix = kAllFixes[idx];
    const QString pngDir = p.value(pngOpt);

    QJsonArray results;

    // 阶段一：亮 → 暗，看首次切换是否正确
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    QApplication::processEvents();
    results.append(probe(fix, QStringLiteral("light"), QString()));

    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    QApplication::processEvents();
    results.append(probe(fix, QStringLiteral("dark"), pngDir));

    // 阶段二：暗 → 亮，检验**可逆性**（方案 0 的致命缺陷就在这里）
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    QApplication::processEvents();
    results.append(probe(fix, QStringLiteral("back-to-light"), QString()));

    // 阶段三：再切暗，检验「双向」而非单向
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::DarkType);
    QApplication::processEvents();
    results.append(probe(fix, QStringLiteral("dark-again"), QString()));

    QJsonArray checks;
    for (const auto &c : g_checks) {
        QJsonObject o;
        o[QStringLiteral("name")] = c.first;
        o[QStringLiteral("ok")] = c.second.first;
        o[QStringLiteral("detail")] = c.second.second;
        checks.append(o);
    }

    int failed = 0;
    for (const auto &c : g_checks)
        if (!c.second.first)
            ++failed;

    QJsonObject root;
    root[QStringLiteral("probe")] = QStringLiteral("panshi-darkfix");
    root[QStringLiteral("results")] = results;
    root[QStringLiteral("checks")] = checks;
    root[QStringLiteral("failed")] = failed;

    QTextStream out(stdout);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    out.flush();

    QTextStream err(stderr);
    for (const auto &c : g_checks)
        err << (c.second.first ? "ok   " : "FAIL ") << c.first << "  " << c.second.second << "\n";
    err.flush();
    return failed > 0 ? 1 : 0;
}
