// SPDX-License-Identifier: GPL-3.0-or-later
//
// 暗色主题「深底黑字」根因探针 panshi-darkprobe（开发期专用，不安装）
//
// 现象（2026-10-05，用户截图 + 像素实测确认）：
//   切到暗色后，自绘卡片底变成深灰 #282828（正确），但**几乎所有 DLabel 文字
//   仍是近黑 #060606/#101010** —— 对比度 1.3，肉眼等于「字消失了」。
//   侧栏更严重：整条侧栏底是纯白 #ffffff（亮色残留），压在深色内容区旁边。
//
// 本探针要回答的问题（**不靠读代码猜，靠两条路径的像素差**）：
//   QApplication 的 QPalette（Qt 原生控件走这条）与 DGuiApplicationHelper 的
//   applicationPalette()（自绘组件走这条）—— 在「现行 main.cpp 的
//   applicationPaletteChanged 处理」之后，二者是否还一致？
//
// 两条被测路径：
//   路径 A「现行」= setStyleSheet("") → qApp->setPalette(appPalette) → 重设 QSS
//   路径 B「候选」= setStyleSheet("") → **不调 qApp->setPalette** → 重设 QSS
//
// 为什么怀疑 qApp->setPalette：DTK6 在 main.cpp:146 那行**明确报警告**
//   "DGuiApplicationHelper: Don't use QGuiApplication::setPalette on DTK application."
// 而且 DLabel::setForegroundRole(DPalette::ColorType) 记的是**角色**、在
// changeEvent(PaletteChange) 里向**控件自己的 QPalette** 取值 —— 一旦
// qApp->setPalette() 与 applicationPalette() 的状态出现偏差，角色就会取到
// 另一套主题的色，出现「底深字浅」或「底浅字深」的错配。
//
// 输出：stdout 纯 JSON；ok/FAIL 行走 stderr；退出码 = 失败数 > 0 ? 1 : 0。
// 用法：panshi-darkprobe [--theme light|dark] [--png .tmp/dark]

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
#include <QPushButton>
#include <QTextStream>
#include <QTreeWidget>
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

using PanshiUi::Semantic;
using PanshiUi::TextLevel;
namespace Space = PanshiUi::Space;

namespace {

struct Check {
    QString name;
    bool ok;
    QString detail;
};

QVector<Check> g_checks;

void check(const QString &name, bool ok, const QString &detail)
{
    g_checks.append({name, ok, detail});
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

/** 反复 processEvents + 丢弃式 grab，规避「首渲染遍」缺部件的假象 */
void settle(QWidget *w, int rounds = 8)
{
    w->show();
    for (int i = 0; i < rounds; ++i)
        QApplication::processEvents();
    (void)w->grab();
    for (int i = 0; i < rounds; ++i)
        QApplication::processEvents();
}

/** 在区域里找「与底色不同且占比最高」的颜色 = 文字实际渲染色 */
QColor textIn(const QImage &img, const QRect &r, const QColor &bg)
{
    QHash<QRgb, int> hist;
    for (int y = r.top(); y <= r.bottom() && y < img.height(); ++y) {
        for (int x = r.left(); x <= r.right() && x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c == bg)
                continue;
            // 忽略抗锯齿中间色：只取「够暗或够亮」的实心像素
            if (contrast(c, bg) < 1.6)
                continue;
            hist[c.rgb()]++;
        }
    }
    QRgb best = 0;
    int bestN = 0;
    for (auto it = hist.begin(); it != hist.end(); ++it) {
        if (it.value() > bestN) {
            bestN = it.value();
            best = it.key();
        }
    }
    return bestN > 0 ? QColor(best) : QColor(0, 0, 0, 0);
}

/** 取区域内出现最多的颜色 = 底色 */
QColor bgOf(const QImage &img, const QRect &r)
{
    QHash<QRgb, int> hist;
    for (int y = r.top(); y <= r.bottom() && y < img.height(); ++y) {
        for (int x = r.left(); x <= r.right() && x < img.width(); ++x)
            hist[img.pixel(x, y)]++;
    }
    QRgb best = 0;
    int bestN = 0;
    for (auto it = hist.begin(); it != hist.end(); ++it) {
        if (it.value() > bestN) {
            bestN = it.value();
            best = it.key();
        }
    }
    return QColor(best);
}

/**
 * 被测面板：刻意混入三类「取色路径不同」的控件 ——
 *   ① DLabel + setForegroundRole(DPalette::…)  ← 走控件 QPalette 的**角色**
 *   ② QLineEdit / QComboBox / QPushButton       ← 纯 Qt 原生，**只认 QApplication QPalette**
 *   ③ QTreeWidget（表格）                        ← 同上
 * 另外铺一个 #panshiCard 背景的 QFrame 模拟自绘卡片底。
 */
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

    // ① DLabel：三种语义，走 makeLabel 这条项目统一的入口
    auto *title = PanshiUi::makeLabel(QStringLiteral("卡片标题Title"), TextLevel::SectionTitle, Semantic::Normal);
    auto *desc  = PanshiUi::makeLabel(QStringLiteral("描述文字 Muted 灰"), TextLevel::Desc, Semantic::Muted);
    auto *ok    = PanshiUi::makeLabel(QStringLiteral("正常 Success 绿"), TextLevel::Value, Semantic::Success);
    cl->addWidget(title);
    cl->addWidget(desc);
    cl->addWidget(ok);
    lay->addWidget(card);

    // ② 纯 Qt 原生控件：它们**只**看 QApplication::palette()
    auto *edit = new QLineEdit(QStringLiteral("输入框内容"), host);
    edit->setPlaceholderText(QStringLiteral("占位提示Placeholder"));
    auto *combo = new QComboBox(host);
    combo->addItems({QStringLiteral("下拉一"), QStringLiteral("下拉二")});
    combo->setCurrentIndex(0);
    auto *btn = new QPushButton(QStringLiteral("按钮文字"), host);
    lay->addWidget(edit);
    lay->addWidget(combo);
    lay->addWidget(btn);

    // ③ 表格
    auto *tree = new QTreeWidget(host);
    tree->setColumnCount(2);
    tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("值")});
    for (int i = 0; i < 2; ++i) {
        auto *item = new QTreeWidgetItem(tree);
        item->setText(0, QStringLiteral("第%1行").arg(i + 1));
        item->setText(1, QStringLiteral("数值%1").arg(i + 1));
    }
    tree->setMinimumHeight(120);
    lay->addWidget(tree);

    return host;
}

/** 复刻 main.cpp 的 applicationPaletteChanged 处理；useSetPalette=false 即候选修法 */
void applyThemeRefresh(QWidget *root, bool useSetPalette)
{
    qApp->setStyleSheet(QString());
    if (useSetPalette)
        qApp->setPalette(DGuiApplicationHelper::instance()->applicationPalette());
    qApp->setStyleSheet(panshiStyleSheet());

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

QJsonObject probePath(const QString &pathName, bool useSetPalette,
                      const QString &theme, const QString &pngDir)
{
    auto *panel = buildPanel(nullptr);
    panel->setFixedSize(560, 460);
    settle(panel);

    applyThemeRefresh(panel, useSetPalette);

    const QImage img = panel->grab().toImage();
    if (!pngDir.isEmpty())
        img.save(QStringLiteral("%1/dark-%2-%3.png").arg(pngDir, pathName, theme));

    const DPalette std = DGuiApplicationHelper::standardPalette(
        DGuiApplicationHelper::instance()->paletteType());
    const QColor expectBase = std.color(DPalette::Base);

    QJsonObject o;
    o[QStringLiteral("path")] = pathName;
    o[QStringLiteral("theme")] = theme;
    o[QStringLiteral("uses_setPalette")] = useSetPalette;
    o[QStringLiteral("appPalette_Base")] = hx(expectBase);

    // 卡片底：QSS 的 #panshiCard 背景，应等于 Base
    const QRect cardRect(16, 16, 528, 120);
    const QColor cardBg = bgOf(img, cardRect);
    o[QStringLiteral("card_bg")] = hx(cardBg);

    // 卡片标题（DLabel/TextTitle）
    const QRect titleRect(32, 28, 260, 24);
    const QColor titleFg = textIn(img, titleRect, cardBg);
    o[QStringLiteral("card_title_fg")] = hx(titleFg);
    const double titleCr = contrast(titleFg, cardBg);
    o[QStringLiteral("card_title_contrast")] = QString::number(titleCr, 'f', 2);

    check(QStringLiteral("[%1/%2] 卡片标题 DLabel 文字对比度 ≥4.5")
              .arg(pathName, theme),
          titleCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 contrast=%3")
              .arg(hx(titleFg), hx(cardBg), QString::number(titleCr, 'f', 2)));

    // 描述（Muted）
    const QRect descRect(32, 58, 300, 22);
    const QColor descFg = textIn(img, descRect, cardBg);
    const double descCr = contrast(descFg, cardBg);
    o[QStringLiteral("card_desc_fg")] = hx(descFg);
    o[QStringLiteral("card_desc_contrast")] = QString::number(descCr, 'f', 2);
    check(QStringLiteral("[%1/%2] 卡片描述 Muted 文字对比度 ≥4.5")
              .arg(pathName, theme),
          descCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 contrast=%3")
              .arg(hx(descFg), hx(cardBg), QString::number(descCr, 'f', 2)));

    // Success 值（自备绿，applyExactColor 路径）
    const QRect okRect(32, 88, 260, 24);
    const QColor okFg = textIn(img, okRect, cardBg);
    const double okCr = contrast(okFg, cardBg);
    o[QStringLiteral("card_success_fg")] = hx(okFg);
    o[QStringLiteral("card_success_contrast")] = QString::number(okCr, 'f', 2);
    check(QStringLiteral("[%1/%2] Success 绿对比度 ≥4.5").arg(pathName, theme),
          okCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 contrast=%3")
              .arg(hx(okFg), hx(cardBg), QString::number(okCr, 'f', 2)));

    // QLineEdit 文本 —— 纯 Qt 原生，只认 QApplication QPalette
    const QRect editRect(16, 150, 528, 32);
    const QColor editBg = bgOf(img, editRect);
    const QColor editFg = textIn(img, editRect.adjusted(4, 4, -4, -4), editBg);
    const double editCr = contrast(editFg, editBg);
    o[QStringLiteral("lineedit_bg")] = hx(editBg);
    o[QStringLiteral("lineedit_fg")] = hx(editFg);
    o[QStringLiteral("lineedit_contrast")] = QString::number(editCr, 'f', 2);
    check(QStringLiteral("[%1/%2] QLineEdit 文本对比度 ≥4.5").arg(pathName, theme),
          editCr >= 4.5,
          QStringLiteral("fg=%1 bg=%2 contrast=%3")
              .arg(hx(editFg), hx(editBg), QString::number(editCr, 'f', 2)));

    // QPushButton 文本
    auto *btn = panel->findChild<QPushButton *>();
    if (btn) {
        const QRect br = btn->geometry().translated(panel->layout()->contentsMargins().left(),
                                                    panel->layout()->contentsMargins().top());
        const QColor btnBg = bgOf(img, br);
        const QColor btnFg = textIn(img, br.adjusted(4, 4, -4, -4), btnBg);
        const double btnCr = contrast(btnFg, btnBg);
        o[QStringLiteral("button_bg")] = hx(btnBg);
        o[QStringLiteral("button_fg")] = hx(btnFg);
        o[QStringLiteral("button_contrast")] = QString::number(btnCr, 'f', 2);
        check(QStringLiteral("[%1/%2] QPushButton 文本对比度 ≥4.5").arg(pathName, theme),
              btnCr >= 4.5,
              QStringLiteral("fg=%1 bg=%2 contrast=%3")
                  .arg(hx(btnFg), hx(btnBg), QString::number(btnCr, 'f', 2)));
    }

    // 面板整体底：不该是亮色残留
    const QColor panelBg = bgOf(img, QRect(0, 0, img.width(), 8));
    o[QStringLiteral("panel_top_bg")] = hx(panelBg);

    panel->deleteLater();
    return o;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-darkprobe"));

    QCommandLineParser p;
    p.addHelpOption();
    // ⚠️ 第 3 个参数 valueName 不能留空 —— 留空会把选项注册成布尔开关，
    // `--theme dark` 里的 dark 被当成位置参数，报
    // "option not expecting values: theme" 且 value() 恒为空（本探针第一次跑就栽在这）。
    QCommandLineOption themeOpt(QStringLiteral("theme"),
                                QStringLiteral("light | dark（默认两个都跑），用法 --theme=dark"),
                                QStringLiteral("NAME"));
    QCommandLineOption pngOpt(QStringLiteral("png"),
                              QStringLiteral("PNG 输出目录，用法 --png=DIR"),
                              QStringLiteral("DIR"));
    p.addOption(themeOpt);
    p.addOption(pngOpt);
    p.process(app);

    const QString onlyTheme = p.value(themeOpt);
    const QString pngDir = p.value(pngOpt);

    QJsonArray results;
    struct ThemeRun { const char *name; DGuiApplicationHelper::ColorType type; };
    const ThemeRun themes[] = {
        {"light", DGuiApplicationHelper::LightType},
        {"dark",  DGuiApplicationHelper::DarkType},
    };

    for (const ThemeRun &t : themes) {
        if (!onlyTheme.isEmpty() && onlyTheme != QLatin1String(t.name))
            continue;
        DGuiApplicationHelper::instance()->setPaletteType(t.type);
        QApplication::processEvents();
        results.append(probePath(QStringLiteral("A-current"), true, QLatin1String(t.name), pngDir));
        results.append(probePath(QStringLiteral("B-noSetPalette"), false, QLatin1String(t.name), pngDir));
    }

    QJsonArray checks;
    for (const Check &c : g_checks) {
        QJsonObject o;
        o[QStringLiteral("name")] = c.name;
        o[QStringLiteral("ok")] = c.ok;
        o[QStringLiteral("detail")] = c.detail;
        checks.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("probe")] = QStringLiteral("panshi-darkprobe");
    root[QStringLiteral("results")] = results;
    root[QStringLiteral("checks")] = checks;

    QTextStream out(stdout);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    out.flush();

    QTextStream err(stderr);
    int failed = 0;
    for (const Check &c : g_checks) {
        if (!c.ok)
            ++failed;
        err << (c.ok ? "ok   " : "FAIL ") << c.name << "  " << c.detail << "\n";
    }
    err.flush();
    return failed > 0 ? 1 : 0;
}
