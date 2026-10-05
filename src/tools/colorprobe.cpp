// SPDX: 配色探针 panshi-colorprobe
//
// 目的：把 C7「使用率分档颜色」从**肉眼观察**变成**可断言的客观值**。
//
// 做法：离屏（offscreen）构造与磁盘页概览**同字号、同语义通道**的 DLabel，
//       grab() 出真实位图，采样「文字核心像素」的实际颜色，输出 JSON。
//       —— 断言的是**渲染结果**，不是「我以为它会是这个色」。
//       这一点很关键：本项目踩过 `TextLively` 名为警示色、实测是中性黑的坑（V-23），
//       只断言「调用了 setForegroundRole」是抓不到那种失效的。
//
// 亮暗两套都跑：进程内 `DGuiApplicationHelper::setThemeType()` 强制应用内主题，
//       绕开「本机系统级暗色不可达（DTK6 走 treeland 协议，不读 gsettings）」的限制。
//       即 C5/C7 的暗色复核也随之下沉为自动断言。
//
// 判据与页面同源：`PanshiUi::usageSemanticForPct()` + 常量 `kUsageWarnPct`，
//       阈值改一处两边同时生效，不会出现「页面已改、断言按旧阈值」的假绿。
//
// 用法：
//   panshi-colorprobe                        # 亮+暗，默认档位 40/74/75/78/92/100
//   panshi-colorprobe --pct 40,75,92
//   panshi-colorprobe --theme light|dark|both
//
// 输出（stdout，纯 JSON；诊断走 stderr）：
//   {"usageWarnPct":75,"themes":[{"theme":"light","bg":"#…","samples":[
//      {"target":"big","pct":40,"fontPx":40,"semantic":"normal","color":"#…"}, …]}]}

#include <QApplication>
#include <QCommandLineParser>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <DApplication>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DLabel>

#include "ui/components.h"

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace PanshiUi;

namespace {

constexpr const char *kTargetBig = "big";   // 「已用 xxx」大数值：DFontSizeManager::T2
constexpr const char *kTargetPct = "pct";   // 百分比外置标签：DFontSizeManager::T4

QString hexOf(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

/**
 * 采样文字核心色：取与背景色距离最大的那一批像素求平均。
 * 抗锯齿边缘像素是「文字色↔背景色」的混合，直接取众数会偏淡；
 * 取最远 10% 的像素（即笔画内部），得到的才是不失真的文字本色。
 */
QString sampleTextColor(const QImage &img, const QColor &bg, const QString &fallback)
{
    if (img.isNull())
        return fallback;

    // 参照色用**已知的底色**而非图像角落：容器有圆角，四角可能在圆角之外，
    // 转 RGB32 后透明区会变成黑色，取坐标不可靠。
    const int bgr = bg.red(), bgg = bg.green(), bgb = bg.blue();

    QVector<QPair<int, QRgb>> px;
    px.reserve(img.width() * img.height());
    int maxD = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb v = img.pixel(x, y);
            const int dr = qRed(v) - bgr, dg = qGreen(v) - bgg, db = qBlue(v) - bgb;
            const int d = dr * dr + dg * dg + db * db;
            if (d > maxD)
                maxD = d;
            px.append(qMakePair(d, v));
        }
    }
    if (maxD == 0)
        return fallback;                          // 整幅同色：没渲染出文字（断言会判 FAIL）

    const int cut = int(maxD * 0.9);
    qint64 sr = 0, sg = 0, sb = 0, n = 0;
    for (const auto &p : px) {
        if (p.first < cut)
            continue;
        sr += qRed(p.second);
        sg += qGreen(p.second);
        sb += qBlue(p.second);
        ++n;
    }
    if (n == 0)
        return fallback;
    return hexOf(QColor(int(sr / n), int(sg / n), int(sb / n)));
}

/** 背景色 = 像素众数。用于**回读校验**：容器是否真的按主题铺了底
 *  （否则后面的对比度断言就是拿错误的底色在算）。 */
QString sampleBgColor(const QImage &img, const QString &fallback)
{
    if (img.isNull())
        return fallback;
    QHash<QRgb, int> hist;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            hist[img.pixel(x, y)]++;

    QRgb best = 0;
    int bestN = -1;
    for (auto it = hist.constBegin(); it != hist.constEnd(); ++it) {
        if (it.value() > bestN) {
            bestN = it.value();
            best = it.key();
        }
    }
    return bestN > 0 ? hexOf(QColor(best)) : fallback;
}

/**
 * 纯色底容器。⚠️ 不用 PanshiPanel：它的自绘取 `applicationPalette()`，
 * 而 DTK6 里 applicationPalette 由 **themeType（系统主题）** 驱动、
 * 文字由 **paletteType** 驱动，二者分离 —— 探针只能改后者，
 * 于是出现「深主题 + 浅容器底」的假象（实测暗色 bg 仍 #f8f8f8）。
 * 这里直接用 `standardPalette(type)` 的 Base 铺底：它就是真实暗色会话里
 * 卡片底色的同一份数据。顺带也避开了 widget 级 setPalette 触发的 DTK 告警。
 */
class FlatHost : public QWidget
{
public:
    explicit FlatHost(const QColor &bg, QWidget *parent = nullptr)
        : QWidget(parent), m_bg(bg) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), m_bg);
    }

private:
    QColor m_bg;
};

/** 渲染一个与页面同构的标签（同字号、同语义通道）并采样它的真实颜色 */
QJsonObject renderSample(double pct, bool isBig, const QColor &bg)
{
    FlatHost host(bg);
    host.setFixedSize(360, isBig ? 72 : 48);
    auto *lay = new QVBoxLayout(&host);
    lay->setContentsMargins(8, 8, 8, 8);

    auto *label = new DLabel(&host);
    DFontSizeManager::instance()->bind(label, isBig ? DFontSizeManager::T2
                                                    : DFontSizeManager::T4);
    label->setText(isBig ? QStringLiteral("已用 512 GB")
                         : QString::number(pct, 'f', 1) + QStringLiteral("%"));
    // 与磁盘页同契约：大数值（28px）按使用率分档取语义；百分比（18px）恒正常色。
    // 18px 小字的 TextWarning 对比度不足 WCAG AA 正文门槛，警示色留给大数值与进度条。
    const Semantic sem = isBig ? usageSemanticForPct(pct) : Semantic::Normal;
    applySemantic(label, sem);
    lay->addWidget(label);

    host.show();
    QApplication::processEvents();               // 等布局与字体绑定生效
    QApplication::processEvents();

    const QImage img = host.grab().toImage().convertToFormat(QImage::Format_RGB32);

    // 真实像素字号：WCAG 的「大文本」判据（≥18.66px bold / ≥24px 常规）要看它，
    // 不能看 fontMetrics().height() —— 后者含行距，会把 14px 的小字放大成 26 而误判为大文本。
    const int fontSizePx = QFontInfo(label->font()).pixelSize();

    QJsonObject o;
    o.insert(QStringLiteral("target"), QString(isBig ? kTargetBig : kTargetPct));
    o.insert(QStringLiteral("pct"), pct);
    o.insert(QStringLiteral("fontPx"), fontSizePx);
    o.insert(QStringLiteral("lineHeight"), label->fontMetrics().height());
    o.insert(QStringLiteral("semantic"),
             QString(sem == Semantic::Warning ? "warning" : "normal"));
    o.insert(QStringLiteral("bg"), hexOf(bg));
    o.insert(QStringLiteral("bgSampled"), sampleBgColor(img, hexOf(bg)));
    o.insert(QStringLiteral("color"), sampleTextColor(img, bg, hexOf(semanticColor(sem))));
    host.close();
    return o;
}

QJsonObject runTheme(const QString &name, DGuiApplicationHelper::ColorType type,
                     const QList<double> &pcts)
{
    // ⚠️ DTK 6 已移除 setThemeType()（头文件里它被 `#if DTK_VERSION < 6` 包掉，
    //    写上去是编译错误，不是警告）；DTK6 用 **setPaletteType()** 切换亮暗。
    DGuiApplicationHelper::instance()->setPaletteType(type);
    QApplication::processEvents();
    QApplication::processEvents();

    // 底色取同一主题的 standardPalette：真实会话里卡片底就是这份数据
    const QColor bg = DGuiApplicationHelper::standardPalette(type).color(DPalette::Base);

    QJsonArray samples;
    for (double p : pcts) {
        samples.append(renderSample(p, true, bg));
        samples.append(renderSample(p, false, bg));
    }

    QJsonObject o;
    o.insert(QStringLiteral("theme"), name);
    o.insert(QStringLiteral("bg"), hexOf(bg));
    o.insert(QStringLiteral("samples"), samples);
    return o;
}

} // namespace

int main(int argc, char *argv[])
{
    // 必须在 DApplication 构造前：本机是 GUI 会话但探针不需要真窗口，
    // offscreen 可避免抢焦点、也不需要 DISPLAY。
    qputenv("QT_QPA_PLATFORM", "offscreen");

    DApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("panshi-manager"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("使用率配色探针（C7 自动断言）"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("pct"),
                      QStringLiteral("逗号分隔的使用率档位，默认 40,74,75,78,92,100"),
                      QStringLiteral("list")});
    parser.addOption({QStringLiteral("theme"),
                      QStringLiteral("light / dark / both，默认 both"),
                      QStringLiteral("name"), QStringLiteral("both")});
    parser.process(app);

    QList<double> pcts;
    const QString pctArg = parser.value(QStringLiteral("pct"));
    const QStringList parts = pctArg.isEmpty() ? QStringList{"40", "74", "75", "78", "92", "100"}
                                               : pctArg.split(QLatin1Char(','));
    for (const QString &s : parts) {
        bool ok = false;
        const double v = s.trimmed().toDouble(&ok);
        if (ok)
            pcts.append(v);
        else
            qWarning("忽略无法解析的档位: %s", qPrintable(s));
    }
    if (pcts.isEmpty()) {
        qWarning("没有可用档位");
        return 2;
    }

    const QString theme = parser.value(QStringLiteral("theme"));
    QJsonArray themes;
    if (theme == QLatin1String("light") || theme == QLatin1String("both"))
        themes.append(runTheme(QStringLiteral("light"),
                               DGuiApplicationHelper::LightType, pcts));
    if (theme == QLatin1String("dark") || theme == QLatin1String("both"))
        themes.append(runTheme(QStringLiteral("dark"),
                               DGuiApplicationHelper::DarkType, pcts));

    QJsonObject root;
    root.insert(QStringLiteral("usageWarnPct"), kUsageWarnPct);
    root.insert(QStringLiteral("qt"), QStringLiteral(QT_VERSION_STR));
    root.insert(QStringLiteral("themes"), themes);

    QJsonDocument doc(root);
    fputs(doc.toJson(QJsonDocument::Indented).toStdString().c_str(), stdout);
    return 0;
}
