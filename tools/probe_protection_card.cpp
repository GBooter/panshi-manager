// SPDX-License-Identifier: GPL-3.0-or-later
//
// 「重启后状态」卡探针 panshi-protectionprobe（开发期专用，不安装）
//
// 目的：把系统保护页第 3 张状态卡的**文案与语义色**从肉眼观察变成可断言的值。
//
// 背景（2026-10-05）：该卡原为「重启后处理」，常态（enable=false，即绝大多数用户）
// 填 "无(保护已开启)"，四个问题：
//   ① 与卡 1「只读保护 = 已开启」信息重复；
//   ② 括号在中文语境里默认被读成「包含关系」，信息量为零；
//   ③ 更糟：被扫读成 "无保护 · 已开启"，把**安全状态读成危险状态**；
//   ④ 用 Muted（灰）渲染 —— 灰在状态卡里等于「未生效 / 空卡」，与事实相反。
// 现改为「重启后状态」+ 常态 Semantic::Success。
//
// ⚠️ 为什么必须实测而不是读代码：components.h 的语义→颜色映射是
//      Success → Highlight（跟随主题强调色，**不是绿色**）、Muted → TextTips、
//      Warning / Danger → TextWarning（两者**刻意同色**，WCAG 1.4.1 靠图标+文案区分）。
//      项目已踩过 `TextLively` 名为警示色、实测是中性黑的坑（V-23）——
//      只断言「调用了 applySemantic」抓不到那类失效，必须采样渲染后的像素。
//
// 两条轨道，各司其职：
//   轨 A（色值，严格）：已知纯色底 + 真实 DLabel + applySemantic，离屏 grab 后采样
//          **文字核心像素**，亮/暗双主题各跑一遍。断言的是渲染结果。
//   轨 B（结构）：真实 PanshiStateCard + **页面同一份映射**
//          （src/pages/protectioncards.h），验证三分支的文本真的落到了控件上。
//
// 用法：
//   panshi-protectionprobe                       # 亮+暗
//   panshi-protectionprobe --theme light
//   panshi-protectionprobe --png .tmp/card       # 额外写出三分支真实卡片 PNG
//
// 输出：stdout 为纯 JSON（含 checks 数组）；ok/FAIL 行走 stderr。
//       退出码 = 失败断言数 > 0 ? 1 : 0

#include <QApplication>
#include <QCommandLineParser>
#include <QFontInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QHBoxLayout>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <DApplication>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DLabel>
#include <DPalette>

#include "pages/protectioncards.h"
#include "ui/components.h"

#include <cmath>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace PanshiUi;

namespace {

int g_fail = 0;
QJsonArray g_checks;

void check(bool ok, const QString &what)
{
    QTextStream o(stderr);
    o << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    o.flush();
    if (!ok)
        ++g_fail;
    g_checks.append(QJsonObject{{QStringLiteral("name"), what},
                                {QStringLiteral("ok"), ok}});
}

QString hexOf(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

int colorDist(const QColor &a, const QColor &b)
{
    const double dr = a.red() - b.red();
    const double dg = a.green() - b.green();
    const double db = a.blue() - b.blue();
    return int(std::sqrt(dr * dr + dg * dg + db * db));
}

/** WCAG 2.1 相对亮度（sRGB → 线性） */
double relLuminance(const QColor &c)
{
    auto f = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * f(c.red()) + 0.7152 * f(c.green()) + 0.0722 * f(c.blue());
}

/** WCAG 2.1 对比度 */
double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relLuminance(a), lb = relLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QString nameOf(Semantic s)
{
    switch (s) {
    case Semantic::Muted:   return QStringLiteral("muted");
    case Semantic::Normal:  return QStringLiteral("normal");
    case Semantic::Accent:  return QStringLiteral("accent");
    case Semantic::Success: return QStringLiteral("success");
    case Semantic::Warning: return QStringLiteral("warning");
    case Semantic::Danger:  return QStringLiteral("danger");
    }
    return QStringLiteral("?");
}

/** 背景色 = 像素众数（回读校验：底色确实是本主题的 Base） */
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
 * 采样文字核心色：取与底色距离最大的那 10% 像素求平均。
 * 抗锯齿边缘是「文字色↔底色」的混合，取众数会偏淡；取笔画内部才是不失真的本色。
 * 同 colorprobe 的取法 —— 两处保持一致，避免同一件事两套判据。
 */
QString sampleTextColor(const QImage &img, const QColor &bg, const QString &fallback)
{
    if (img.isNull())
        return fallback;

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
        return fallback;   // 整幅同色 → 没渲染出文字（断言会判 FAIL）

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

/** 纯色底容器（同 colorprobe：不走 PanshiPanel 的自绘底，避免主题错配） */
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

struct ColorSample
{
    Semantic sem;
    QString sampled;
    QString expected;
    int distToExpected = -1;
    int fontPx = 0;
};

/** 轨 A：与状态卡主值同级别（TextLevel::Value → T2）的标签，采样实际文字色 */
ColorSample probeSemantic(Semantic sem, const QColor &bg)
{
    FlatHost host(bg);
    host.setFixedSize(360, 84);
    auto *lay = new QVBoxLayout(&host);
    lay->setContentsMargins(12, 12, 12, 12);

    auto *l = new DLabel(&host);
    DFontSizeManager::instance()->bind(l, DFontSizeManager::T2);
    l->setText(QStringLiteral("仍为只读"));
    applySemantic(l, sem);
    lay->addWidget(l);

    host.show();
    QApplication::processEvents();
    QApplication::processEvents();

    const QImage img = host.grab().toImage().convertToFormat(QImage::Format_RGB32);

    ColorSample s;
    s.sem = sem;
    s.fontPx = QFontInfo(l->font()).pixelSize();
    s.expected = hexOf(semanticColor(sem));
    s.sampled = sampleTextColor(img, bg, s.expected);
    s.distToExpected = colorDist(QColor(s.sampled), QColor(s.expected));
    host.close();
    return s;
}

QJsonObject sampleToJson(const ColorSample &s, const QColor &bg)
{
    QJsonObject o;
    o.insert(QStringLiteral("semantic"), nameOf(s.sem));
    o.insert(QStringLiteral("fontPx"), s.fontPx);
    o.insert(QStringLiteral("bg"), hexOf(bg));
    o.insert(QStringLiteral("sampled"), s.sampled);       // 实测像素色
    o.insert(QStringLiteral("expected"), s.expected);     // semanticColor() 的理论值
    o.insert(QStringLiteral("distToExpected"), s.distToExpected);
    return o;
}

struct CardTexts
{
    QString title;
    QString value;
    QString status;
    /** 渲染图里命中「徽标药丸底色 softFill(statusSem)」的像素数：
        文本落地 ≠ 徽标真的画出来了（离屏首渲染曾整块缺徽标而文本仍在控件树里），
        故用像素计数补上这条静默失效的防线。 */
    int pillPixels = 0;
    /** 标签 / 值 / 徽标三者的实际 geometry().top()。
        2026-10-05 修掉了「值在顶、功能名在中间」的层级倒置，这里留作回归防线：
        文本落地断言无法发现顺序被改回去，只有比对三者的纵坐标才能。 */
    int titleTop = -1;
    int valueTop = -1;
    int statusTop = -1;
    /** 在**真实卡片渲染图**上采样的主值文字色 + 其所在矩形。
        ⚠️ 这条是「单一指标会骗人」的教训：改 Success 为绿色时，轨 A（独立 DLabel）
        采样是绿的、34 条断言全绿，而真实状态卡里那个标签其实渲染成 #272727 近黑
        —— 因为 makeLabel 先给 Normal 态设了 TextTitle 角色，DLabel 收到
        PaletteChange 会重新套用旧角色，把直接写进调色板的绿色覆盖掉。
        只有直接采样卡片上那个标签的像素才抓得到。 */
    QString valueSampled;
    QRect valueRect;
};

/** 轨 B：真实 PanshiStateCard + 页面同一份映射，验证文本真的落到控件上 */
CardTexts probeRealCard(const Panshi::WritableStatus &w, const QString &pngPath)
{
    auto *card = new PanshiStateCard(PanshiProtection::rebootCardTitle());
    const PanshiProtection::CardView v = PanshiProtection::rebootCard(w);
    card->setValue(v.value, v.valueSem);
    card->setStatus(v.status, v.statusSem);

    card->resize(280, 170);
    card->show();
    // 离屏下「首张卡」的徽标（Muted 语义）会因组件首次 polish/渲染遍未完成而
    // 缺席第一次 grab（实测：常态卡徽标带一片纯白，而临时/永久卡正常）——
    // 这是探针的假阴性，不是产品缺陷（真机有完整事件循环）。故先跑几轮事件、
    // 再做一次丢弃式 grab 强制完成首个完整渲染遍，随后才取正式画面。
    for (int i = 0; i < 6; ++i)
        QApplication::processEvents();
    (void)card->grab();
    QApplication::processEvents();

    CardTexts out;
    // 不靠 geometry() 挑「谁是最靠上的那个」：离屏下布局未必给出可靠坐标（实测徽标
    // 内部 DLabel 的 top 反而最小，会把主值误判成徽标）。改为按「预期文案逐一命中」——
    // 既验证文案真的进了控件树，又不依赖顺序。
    // 徽标类刻意不带 Q_OBJECT（组件层约定），故不 findChildren<PanshiBadge*>，
    // 而是直接在其内部 DLabel 的文案上命中（无需改生产代码）。
    //
    // 但**层级顺序**必须用坐标验证：文案落地断言发现不了「顺序被改回值在顶」。
    // 坐标统一用 mapTo(card) 换算到卡片坐标系——徽标内标签的 geometry() 是相对
    // 徽标自身的，直接取 top 会得到一个小数值（这正是当年误判的根因）。
    const QString titleText = PanshiProtection::rebootCardTitle();
    for (DLabel *l : card->findChildren<DLabel *>()) {
        if (!l->isVisible() || l->text().isEmpty())
            continue;
        const QPoint tl = l->mapTo(card, QPoint(0, 0));
        const int top = tl.y();
        if (l->text() == v.value) {
            out.value = l->text();
            out.valueTop = top;
            out.valueRect = QRect(tl, l->size());
        } else if (l->text() == v.status) {
            out.status = l->text();
            out.statusTop = top;
        } else if (l->text() == titleText) {
            out.title = l->text();
            out.titleTop = top;
        }
    }

    const QImage img = card->grab().toImage().convertToFormat(QImage::Format_RGB32);
    if (!pngPath.isEmpty())
        img.save(pngPath);

    // 徽标药丸底色 = softFill(statusSem)：数命中像素，防「药丸没画出来」的静默回归
    const QColor pill = softFill(v.statusSem);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (colorDist(QColor(img.pixel(x, y)), pill) <= 10)
                ++out.pillPixels;

    // 真实卡片上的主值文字色（裁该标签所在矩形采样）—— 见 CardTexts::valueSampled 的
    // 「单一指标会骗人」说明：轨 A 绿而真实卡片黑时，轨 A 的断言全绿却抓不到。
    if (out.valueRect.isValid()) {
        const QImage sub = img.copy(out.valueRect).convertToFormat(QImage::Format_RGB32);
        out.valueSampled = sampleTextColor(sub, sub.pixelColor(0, 0), QString());
    }

    card->close();
    delete card;
    return out;
}

struct RowCard
{
    QString title;
    QString value;
    Semantic valueSem;
    QString status;
    Semantic statusSem;
};

/** 由共享映射直接构造整行渲染用的卡片 —— 探针**不另抄**文案与语义色 */
static RowCard makeRowCard(const QString &title, const PanshiProtection::CardView &v)
{
    return RowCard{title, v.value, v.valueSem, v.status, v.statusSem};
}

/** 轨 C：把系统保护页并排的三张卡整体渲出来，供肉眼复核「三卡横排」时的层级与配色。
    单卡好看 ≠ 一行好看：横排时三张卡的「谁更显眼」是否成立，只有并排才看得出来。
    三张卡全部取自 protectioncards.h 的共享映射，故本轨渲染的就是页面真正会渲染的内容。
    ⚠️ 只读保护那个布尔按**系统保护页**口径给（保护生效 = !enable）；
    概览页用 isImmutable()，是另一个信号（详见 protectioncards.h 头注释的职责切分表）。 */
void renderRow(const QVector<RowCard> &cards, const QColor &bg, const QString &pngPath)
{
    FlatHost host(bg);
    host.setFixedSize(3 * 190 + 4 * Space::M, 148);
    auto *lay = new QHBoxLayout(&host);
    lay->setContentsMargins(Space::M, Space::M, Space::M, Space::M);
    lay->setSpacing(Space::M);

    for (const RowCard &rc : cards) {
        auto *card = new PanshiStateCard(rc.title, &host);
        card->setValue(rc.value, rc.valueSem);
        card->setStatus(rc.status, rc.statusSem);
        lay->addWidget(card, 1);
    }

    host.show();
    for (int i = 0; i < 6; ++i)
        QApplication::processEvents();
    (void)host.grab();          // 丢弃式首渲染（见 probeRealCard 同款说明）
    QApplication::processEvents();

    if (!pngPath.isEmpty())
        host.grab().save(pngPath);
    host.close();
}

QJsonArray runTheme(const QString &name, DGuiApplicationHelper::ColorType type,
                    const QString &pngPrefix)
{
    // ⚠️ DTK6 已移除 setThemeType()（DTK5 遗留），切换亮暗用 setPaletteType()
    DGuiApplicationHelper::instance()->setPaletteType(type);
    QApplication::processEvents();
    QApplication::processEvents();

    // ⚠️ setPaletteType() 只翻转 DTK 的**主题判定**（semanticColor/applySemantic 用它），
    //    并不会重建 QApplication 的 QPalette —— 实测离屏下：暗色主题里
    //    文字色已是暗色值（#057ffd），但 baseColor()/控件自绘底仍是亮色 #FFFFFF，
    //    于是暗色下 softFill(Muted)（浅灰叠在**白**底上）≈ 白，与卡底同色 →
    //    徽标「隐身」，pillPixels 还会退化成整卡像素（实测 46355/47600），
    //    这条断言在暗色下等于空转。故显式把本主题的标准调色板装上，
    //    让卡底、baseColor()、softFill() 三者都在**同一主题**下解析。
    qApp->setPalette(DGuiApplicationHelper::standardPalette(type));
    QApplication::processEvents();

    // 底色取同主题的 standardPalette::Base —— 真实会话里卡片底就是这份数据
    const QColor bg = DGuiApplicationHelper::standardPalette(type).color(DPalette::Base);

    const ColorSample sNormal  = probeSemantic(Semantic::Success, bg);
    const ColorSample sMuted   = probeSemantic(Semantic::Muted, bg);
    const ColorSample sWarn    = probeSemantic(Semantic::Warning, bg);
    const ColorSample sDanger  = probeSemantic(Semantic::Danger, bg);
    const ColorSample sAccent  = probeSemantic(Semantic::Accent, bg);

    const int distNormalMuted = colorDist(QColor(sNormal.sampled), QColor(sMuted.sampled));
    const int distNormalWarn  = colorDist(QColor(sNormal.sampled), QColor(sWarn.sampled));

    // ---- 三个分支：文案来自页面同一份映射 ----
    Panshi::WritableStatus normal;                                   // 保护中（默认）
    Panshi::WritableStatus tmp;   tmp.enable  = true; tmp.clearAfterReboot = true;
    Panshi::WritableStatus perm;  perm.enable = true; perm.clearAfterReboot = false;

    const QString t = name + QStringLiteral("/");
    const PanshiProtection::CardView vNormal = PanshiProtection::rebootCard(normal);
    const PanshiProtection::CardView vTmp    = PanshiProtection::rebootCard(tmp);
    const PanshiProtection::CardView vPerm   = PanshiProtection::rebootCard(perm);

    // PNG 名带主题名，避免亮/暗两次渲染互相覆盖（此前两主题共用前缀，暗色会盖掉亮色）
    const QString pfx = pngPrefix.isEmpty() ? QString() : pngPrefix + QStringLiteral("-") + name;
    const CardTexts cNormal = probeRealCard(normal, pfx.isEmpty() ? QString()
                                          : pfx + QStringLiteral("-normal.png"));
    const CardTexts cTmp    = probeRealCard(tmp, pfx.isEmpty() ? QString()
                                          : pfx + QStringLiteral("-temporary.png"));
    const CardTexts cPerm   = probeRealCard(perm, pfx.isEmpty() ? QString()
                                          : pfx + QStringLiteral("-permanent.png"));

    // 轨 C：整行渲染。三张卡全部取自 protectioncards.h 的共享映射 —— 探针不再
    // 镜像页面规则，故「探针看到的」与「页面渲染的」不可能分叉。
    if (!pfx.isEmpty()) {
        const auto pNormal = PanshiProtection::protectCard(true);    // 保护生效
        const auto pDanger = PanshiProtection::protectCard(false);   // 保护失效
        const auto wNormal = PanshiProtection::writableCard(false);  // 可写层未启用
        const auto wDanger = PanshiProtection::writableCard(true);   // 可写层已解锁

        renderRow(QVector<RowCard>{
                      makeRowCard(PanshiProtection::protectCardTitle(), pNormal),
                      makeRowCard(PanshiProtection::writableCardTitle(), wNormal),
                      makeRowCard(PanshiProtection::rebootCardTitle(), vNormal),
                  },
                  bg, pfx + QStringLiteral("-row-normal.png"));

        renderRow(QVector<RowCard>{
                      makeRowCard(PanshiProtection::protectCardTitle(), pDanger),
                      makeRowCard(PanshiProtection::writableCardTitle(), wDanger),
                      makeRowCard(PanshiProtection::rebootCardTitle(), vPerm),
                  },
                  bg, pfx + QStringLiteral("-row-danger.png"));
    }

    // ---- 断言 ----
    // 1. 核心：常态主值色必须与 Muted 灰明显不同（防「灰卡」回归）
    check(distNormalMuted >= 40,
          t + QStringLiteral("常态主值色 ≠ Muted 灰 (dist=%1, success=%2 muted=%3)")
                  .arg(distNormalMuted).arg(sNormal.sampled, sMuted.sampled));

    // 2. 常态主值色 = semanticColor(Success) 的实际渲染色（映射没被 QSS 之类吃掉）
    check(sNormal.distToExpected <= 24,
          t + QStringLiteral("常态主值色 ≈ semanticColor(Success) (dist=%1)")
                  .arg(sNormal.distToExpected));

    // 3. 「正常」与「警告」必须可区分 —— 若主题色恰好撞成同色，这里会暴露
    check(distNormalWarn >= 40,
          t + QStringLiteral("Success 色 ≠ Warning 色 (dist=%1)").arg(distNormalWarn));

    // 4. Warning/Danger 的实际渲染色与理论值一致（两者刻意同色，只各自校验）
    check(sWarn.distToExpected <= 24,
          t + QStringLiteral("Warning 采样 ≈ 理论值 (dist=%1)").arg(sWarn.distToExpected));
    check(sDanger.distToExpected <= 24,
          t + QStringLiteral("Danger 采样 ≈ 理论值 (dist=%1)").arg(sDanger.distToExpected));

    // 4b. WCAG 1.4.3 对比度：状态卡主值是 TextLevel::Value（T2 大字号）⇒ 门槛 3.0。
    //     这条断言是「常态主值用绿色」的**回归防线**，也是当初发现暗色缺陷的判据：
    //     主题蓝在暗色下只有 2.01:1（低于 3.0），正是它挡住了那次错误配色。
    //     断言打在**渲染采样色**上，不是理论值 —— 免得「理论达标、实际被 QSS 吃掉」。
    const double crSuccess = contrastRatio(QColor(sNormal.sampled), bg);
    check(crSuccess >= 3.0,
          t + QStringLiteral("常态主值色 WCAG 对比度 ≥3.0 (实测 %1:1, %2)")
                  .arg(crSuccess, 0, 'f', 2).arg(sNormal.sampled));
    const double crWarn = contrastRatio(QColor(sWarn.sampled), bg);
    check(crWarn >= 3.0,
          t + QStringLiteral("Warning 采样 WCAG 对比度 ≥3.0 (实测 %1:1, %2)")
                  .arg(crWarn, 0, 'f', 2).arg(sWarn.sampled));

    // 4c. Accent（强调色）门槛取 **4.5**（较严的一档）—— 它同时出现在 T2 大字号数值
    //     （磁盘页 4 张汇总卡）与 `TextLevel::Desc` 小字号（aboutdialog.cpp:397），
    //     两处都硬性要求达标。实测默认主题暗色原始 Highlight 只有 2.03:1、亮色 3.76:1
    //     都**不达标** ⇒ semanticColor() 里对它做了「保色相、只调明度」的处理。
    const double crAccent = contrastRatio(QColor(sAccent.sampled), bg);
    check(crAccent >= 4.5,
          t + QStringLiteral("Accent WCAG 对比度 ≥4.5 (实测 %1:1, %2)")
                  .arg(crAccent, 0, 'f', 2).arg(sAccent.sampled));

    // 4d. 调明度**不得改变色相** —— Accent 的全部意义就是「这是用户自己选的主题色」，
    //     一旦色相被换掉，这条约束就白做了。用 HSV 色相环距离守住。
    {
        const QColor raw = DGuiApplicationHelper::standardPalette(type).color(DPalette::Highlight);
        int h1 = 0, s1 = 0, v1 = 0, h2 = 0, s2 = 0, v2 = 0;
        raw.getHsv(&h1, &s1, &v1);          // Qt6 是 getHsv，toHsv(int*,...) 已移除
        QColor(sAccent.sampled).getHsv(&h2, &s2, &v2);
        int dh = std::abs(h1 - h2);
        dh = std::min(dh, 360 - dh);
        check(dh <= 6,
              t + QStringLiteral("Accent 调明度后色相未偏移 (原始 %1 → 实测 %2, Δh=%3)")
                      .arg(raw.name(), sAccent.sampled).arg(dh));
    }

    // 5. 轨 B：三分支文本真的落到了控件上（与页面同一份映射同源）
    check(cNormal.value == vNormal.value && cNormal.status == vNormal.status
              && !cNormal.value.isEmpty() && !cNormal.status.isEmpty(),
          t + QStringLiteral("常态卡文本落地 [%1 / %2]")
                  .arg(cNormal.value, cNormal.status));
    check(cTmp.value == vTmp.value && cTmp.status == vTmp.status,
          t + QStringLiteral("临时关闭卡文本落地 [%1 / %2]").arg(cTmp.value, cTmp.status));
    check(cPerm.value == vPerm.value && cPerm.status == vPerm.status,
          t + QStringLiteral("永久关闭卡文本落地 [%1 / %2]").arg(cPerm.value, cPerm.status));

    // 5b. 徽标药丸真的画出来了：文本在控件树里 isVisible()=true 不代表被绘制，
    //     离屏「首张卡首渲染」就曾整块缺徽标而文本断言照样通过（静默假绿）。
    check(cNormal.pillPixels >= 100,
          t + QStringLiteral("常态卡徽标已绘制 (药丸像素=%1)").arg(cNormal.pillPixels));
    check(cTmp.pillPixels >= 100,
          t + QStringLiteral("临时关闭卡徽标已绘制 (药丸像素=%1)").arg(cTmp.pillPixels));
    check(cPerm.pillPixels >= 100,
          t + QStringLiteral("永久关闭卡徽标已绘制 (药丸像素=%1)").arg(cPerm.pillPixels));

    // 5c. 信息层级顺序：标签(功能名) 在上 → 状态值 居中 → 徽标 在下。
    //     2026-10-05 修正了「值在顶、功能名被压到中间」的层级倒置。文案落地断言
    //     **发现不了顺序被改回去**，只有比对三者的纵坐标才能，故单列这三条。
    const auto orderOk = [](const CardTexts &c) {
        return c.titleTop >= 0 && c.valueTop > c.titleTop && c.statusTop > c.valueTop;
    };
    check(orderOk(cNormal),
          t + QStringLiteral("常态卡层级 标签<值<徽标 (top=%1/%2/%3)")
                  .arg(cNormal.titleTop).arg(cNormal.valueTop).arg(cNormal.statusTop));
    check(orderOk(cTmp),
          t + QStringLiteral("临时关闭卡层级 标签<值<徽标 (top=%1/%2/%3)")
                  .arg(cTmp.titleTop).arg(cTmp.valueTop).arg(cTmp.statusTop));
    check(orderOk(cPerm),
          t + QStringLiteral("永久关闭卡层级 标签<值<徽标 (top=%1/%2/%3)")
                  .arg(cPerm.titleTop).arg(cPerm.valueTop).arg(cPerm.statusTop));

    // 6. 三个分支互不重复（旧的「无(保护已开启)」曾与卡 1 语义重复）
    check(cNormal.value != cTmp.value && cTmp.value != cPerm.value
              && cNormal.value != cPerm.value,
          t + QStringLiteral("三分支主值互不重复"));

    // 6b. **真实卡片**上的主值色 == semanticColor(该分支语义)。
    //     轨 A 的等价断言抓不到「轨 A 绿、真实卡片黑」——改 Success 为绿色时
    //     实测就是这个形态：34 条断言全绿，而状态卡里那个标签渲染成 #272727 近黑。
    //     必须在卡片像素上直接采样才算数。
    const auto realCardColorOk = [](const CardTexts &c, Semantic sem) {
        if (c.valueSampled.isEmpty())
            return false;
        return colorDist(QColor(c.valueSampled), semanticColor(sem)) <= 24;
    };
    check(realCardColorOk(cNormal, Semantic::Success),
          t + QStringLiteral("常态卡主值实际渲染色 = Success (实测 %1 / 理论 %2)")
                  .arg(cNormal.valueSampled, hexOf(semanticColor(Semantic::Success))));
    check(realCardColorOk(cTmp, Semantic::Warning),
          t + QStringLiteral("临时关闭卡主值实际渲染色 = Warning (实测 %1)")
                  .arg(cTmp.valueSampled));
    check(realCardColorOk(cPerm, Semantic::Danger),
          t + QStringLiteral("永久关闭卡主值实际渲染色 = Danger (实测 %1)")
                  .arg(cPerm.valueSampled));

    QJsonObject o;
    o.insert(QStringLiteral("theme"), name);
    o.insert(QStringLiteral("bg"), hexOf(bg));
    o.insert(QStringLiteral("distNormalMuted"), distNormalMuted);
    o.insert(QStringLiteral("distSuccessWarning"), distNormalWarn);
    o.insert(QStringLiteral("wcagSuccess"), crSuccess);
    o.insert(QStringLiteral("wcagWarning"), crWarn);
    o.insert(QStringLiteral("wcagAccent"), crAccent);
    o.insert(QStringLiteral("accentSampled"), sAccent.sampled);
    o.insert(QStringLiteral("samples"),
             QJsonArray{sampleToJson(sNormal, bg), sampleToJson(sMuted, bg),
                        sampleToJson(sWarn, bg), sampleToJson(sDanger, bg),
                        sampleToJson(sAccent, bg)});
    o.insert(QStringLiteral("cards"),
             QJsonArray{QJsonObject{{QStringLiteral("branch"), QStringLiteral("normal")},
                                    {QStringLiteral("value"), cNormal.value},
                                    {QStringLiteral("status"), cNormal.status},
                                    {QStringLiteral("pillPixels"), cNormal.pillPixels},
                                    {QStringLiteral("layout"),
                                     QStringLiteral("%1/%2/%3").arg(cNormal.titleTop)
                                             .arg(cNormal.valueTop).arg(cNormal.statusTop)},
                                    {QStringLiteral("semantic"), QStringLiteral("success")}},
                        QJsonObject{{QStringLiteral("branch"), QStringLiteral("temporary")},
                                    {QStringLiteral("value"), cTmp.value},
                                    {QStringLiteral("status"), cTmp.status},
                                    {QStringLiteral("pillPixels"), cTmp.pillPixels},
                                    {QStringLiteral("layout"),
                                     QStringLiteral("%1/%2/%3").arg(cTmp.titleTop)
                                             .arg(cTmp.valueTop).arg(cTmp.statusTop)},
                                    {QStringLiteral("semantic"), QStringLiteral("warning")}},
                        QJsonObject{{QStringLiteral("branch"), QStringLiteral("permanent")},
                                    {QStringLiteral("value"), cPerm.value},
                                    {QStringLiteral("status"), cPerm.status},
                                    {QStringLiteral("pillPixels"), cPerm.pillPixels},
                                    {QStringLiteral("layout"),
                                     QStringLiteral("%1/%2/%3").arg(cPerm.titleTop)
                                             .arg(cPerm.valueTop).arg(cPerm.statusTop)},
                                    {QStringLiteral("semantic"), QStringLiteral("danger")}}});
    return QJsonArray{o};
}

} // namespace

int main(int argc, char *argv[])
{
    // 必须在 DApplication 构造前：探针不需要真窗口，offscreen 免 DISPLAY、不抢焦点
    qputenv("QT_QPA_PLATFORM", "offscreen");

    DApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("panshi-manager"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("「重启后状态」卡探针：文案与语义色的自动断言"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("theme"),
                      QStringLiteral("light / dark / both，默认 both"),
                      QStringLiteral("name"), QStringLiteral("both")});
    parser.addOption({QStringLiteral("png"),
                      QStringLiteral("写出三分支真实卡片 PNG 的路径前缀（可选）"),
                      QStringLiteral("prefix"), QString()});
    parser.addOption({QStringLiteral("green"),
                      QStringLiteral("开发期取色：覆盖 Success 绿（如 #00875a），"
                                     "用于用真实控件渲染对比不同绿"),
                      QStringLiteral("hex"), QString()});
    parser.process(app);

    if (parser.isSet(QStringLiteral("green")))
        qputenv("PANSHI_SUCCESS_GREEN", parser.value(QStringLiteral("green")).toLatin1());

    const QString pngPrefix = parser.value(QStringLiteral("png"));
    const QString theme = parser.value(QStringLiteral("theme"));

    QJsonArray themes;
    if (theme == QLatin1String("light") || theme == QLatin1String("both"))
        themes += runTheme(QStringLiteral("light"),
                           DGuiApplicationHelper::LightType, pngPrefix);
    if (theme == QLatin1String("dark") || theme == QLatin1String("both"))
        themes += runTheme(QStringLiteral("dark"),
                           DGuiApplicationHelper::DarkType, pngPrefix);

    QJsonObject root;
    root.insert(QStringLiteral("probe"), QStringLiteral("panshi-protectionprobe"));
    root.insert(QStringLiteral("qt"), QStringLiteral(QT_VERSION_STR));
    root.insert(QStringLiteral("themes"), themes);
    root.insert(QStringLiteral("failCount"), g_fail);
    root.insert(QStringLiteral("checks"), g_checks);

    fputs(QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString().c_str(), stdout);

    QTextStream(stderr) << (g_fail ? "\n探针失败 " + QString::number(g_fail) + " 项\n"
                                   : "\n全部断言通过\n");

    return g_fail > 0 ? 1 : 0;
}
