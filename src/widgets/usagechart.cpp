// SPDX-License-Identifier: GPL-3.0-or-later
#include "usagechart.h"
#include "scanoverlay.h"
#include "checkoverlay.h"
#include "disk/disktypes.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QProcess>
#include <QVBoxLayout>
#include <DFontSizeManager>
#include <DPalette>
#include <DGuiApplicationHelper>
#include <DSpinner>
#include <DLabel>
#include <cmath>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace {
constexpr double kGB = 1024.0 * 1024.0 * 1024.0;

QString gbText(double bytes)
{
    return QString::number(bytes / kGB, 'f', bytes / kGB >= 100 ? 0 : 1) + QStringLiteral(" GB");
}

double niceCeil(double v)
{
    if (v <= 0)
        return 1.0;
    const double exp = std::pow(10.0, std::floor(std::log10(v)));
    const double f = v / exp;
    double nice;
    if (f <= 1.0) nice = 1.0;
    else if (f <= 2.0) nice = 2.0;
    else if (f <= 5.0) nice = 5.0;
    else nice = 10.0;
    return nice * exp;
}

QString sizeText(qint64 bytes)
{
    double num = static_cast<double>(bytes);
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    while (std::abs(num) >= 1024.0 && idx < 4) {
        num /= 1024.0;
        ++idx;
    }
    if (idx == 0)
        return QString::number(static_cast<qint64>(num)) + QStringLiteral(" B");
    return QString::number(num, 'f', 2) + QStringLiteral(" ") + QString::fromLatin1(units[idx]);
}
} // namespace

UsageChart::UsageChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(220);
    m_animEnabled = !reduceMotionRequested();
    ensureAnimations();   // 提前创建动画，setLoading 可能在 setRecords 之前被调用

    // 加载指示：居中 DSpinner + 文字（静态占位，不做闪动动画）
    m_loadingWidget = new QWidget(this);
    auto *ll = new QVBoxLayout(m_loadingWidget);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);
    ll->addStretch();   // 垂直居中，避免 spinner 偏顶
    m_spinner = new DSpinner(m_loadingWidget);
    m_spinner->setFixedSize(28, 28);
    m_spinner->start();
    ll->addWidget(m_spinner, 0, Qt::AlignHCenter);
    m_loadingLabel = new DLabel(m_loadingWidget);
    m_loadingLabel->setForegroundRole(DPalette::TextTips);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setWordWrap(true);
    DFontSizeManager::instance()->bind(m_loadingLabel, DFontSizeManager::T7);
    ll->addWidget(m_loadingLabel);
    ll->addStretch();
    m_loadingWidget->hide();

    // 扫描动态面板（默认隐藏，立即扫描时显示）
    m_scanOverlay = new ScanOverlay(this);
    m_scanOverlay->hide();

    // 启动检查面板（默认隐藏）
    m_checkOverlay = new CheckOverlay(this);
    m_checkOverlay->hide();
}

bool UsageChart::reduceMotionRequested()
{
    static const bool reduced = [] {
        QProcess p;
        p.start(QStringLiteral("gsettings"),
                {QStringLiteral("get"), QStringLiteral("org.deepin.dde.appearance"),
                 QStringLiteral("enable-animations")});
        if (p.waitForFinished(1000)) {
            const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            if (out == QLatin1String("false"))
                return true;
        }
        return false;
    }();
    return reduced;
}

void UsageChart::setRecords(const QList<Panshi::DiskDayRecord> &records)
{
    m_records = records;
    m_hoverIndex = -1;
    ensureAnimations();
    if (m_animEnabled && !m_loading) {
        m_reveal = 0.0;
        m_fade = 0.0;
        m_revealAnim->stop();
        m_fadeAnim->stop();
        m_revealAnim->setStartValue(0.0);
        m_revealAnim->setEndValue(1.0);
        m_revealAnim->start();
        m_fadeAnim->setStartValue(0.0);
        m_fadeAnim->setEndValue(1.0);
        m_fadeAnim->start();
    } else {
        m_reveal = 1.0;
        m_fade = 1.0;
    }
    update();
}

void UsageChart::setSpikeDays(const QSet<QDate> &days)
{
    m_spikeDays = days;
    update();
}

void UsageChart::setSelectedDate(const QDate &d)
{
    m_selected = d;
    update();
}

void UsageChart::setScanning(bool on, const QString &title)
{
    m_scanning = on;
    if (on) {
        if (m_loadingWidget)
            m_loadingWidget->hide();
        if (m_spinner)
            m_spinner->stop();
        if (title.isEmpty())
            m_scanOverlay->setTitleText(tr("正在扫描系统盘目录…"));
        else
            m_scanOverlay->setTitleText(title);
        m_scanOverlay->setBusy(true);
        m_scanOverlay->setProgress(0, 0, tr("正在准备…"));
        m_scanOverlay->setGeometry(rect());
        m_scanOverlay->raise();
        m_scanOverlay->show();
    } else {
        m_scanOverlay->hide();
    }
    update();
}

void UsageChart::setScanProgress(int done, const QString &path)
{
    if (m_scanOverlay && m_scanning) {
        if (path.isEmpty())
            m_scanOverlay->setProgress(done, 0, tr("已扫描 %1 项").arg(done));
        else
            m_scanOverlay->setProgress(done, 0, tr("已扫描 %1 项 · %2").arg(done).arg(path));
    }
}

void UsageChart::setLoadProgress(int done, int total)
{
    if (m_scanOverlay && m_loading)
        m_scanOverlay->setProgress(done, total, tr("已加载 %1/%2 天").arg(done).arg(total));
}

void UsageChart::setChecking(bool on, const QString &title)
{
    if (!m_checkOverlay)
        return;
    if (on) {
        // 检查面板优先于加载/扫描面板
        if (m_scanOverlay)
            m_scanOverlay->hide();
        if (m_loadingWidget)
            m_loadingWidget->hide();
        if (m_spinner)
            m_spinner->stop();
        m_checkOverlay->setGeometry(rect());
        m_checkOverlay->start(title.isEmpty() ? tr("正在检查系统信息…") : title);
        m_checkOverlay->raise();
    } else {
        m_checkOverlay->hide();
    }
    update();
}

void UsageChart::addCheckStep(const QString &name, bool ok, qint64 elapsedMs)
{
    if (m_checkOverlay)
        m_checkOverlay->addStep(name, ok, elapsedMs);
}

void UsageChart::finishCheck(qint64 totalMs)
{
    if (m_checkOverlay)
        m_checkOverlay->finish(totalMs);
}

void UsageChart::setLoading(bool loading, const QString &text)
{
    m_loading = loading;
    if (loading) {
        m_loadingText = text;
        // 加载态保持完全可见，不做透明度动画
        m_fade = 1.0;
        m_reveal = 1.0;
        // 复用动态面板：雷达动画 + 标题 + 确定进度条
        if (m_loadingWidget)
            m_loadingWidget->hide();
        if (m_spinner)
            m_spinner->stop();
        // 检查面板优先：启动检查期间不叠加加载面板
        const bool checking = m_checkOverlay && m_checkOverlay->isActive();
        if (m_scanOverlay && !checking) {
            m_scanOverlay->setTitleText(text.isEmpty() ? tr("正在加载历史数据…") : text);
            m_scanOverlay->setBusy(true);
            m_scanOverlay->setProgress(0, 0, tr("正在读取…"));
            m_scanOverlay->setGeometry(rect());
            m_scanOverlay->raise();
            m_scanOverlay->show();
        }
    } else {
        if (m_scanOverlay && !m_scanning)
            m_scanOverlay->hide();
    }
    update();
}

void UsageChart::ensureAnimations()
{
    if (!m_revealAnim) {
        m_revealAnim = new QPropertyAnimation(this, "reveal", this);
        m_revealAnim->setDuration(700);
        m_revealAnim->setEasingCurve(QEasingCurve::OutCubic);
    }
    if (!m_fadeAnim) {
        m_fadeAnim = new QPropertyAnimation(this, "fade", this);
        m_fadeAnim->setDuration(250);
        m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
    }
}

QRect UsageChart::chartArea() const
{
    return rect().adjusted(46, 10, -8, -24);
}

int UsageChart::indexAtX(int x) const
{
    if (m_records.isEmpty())
        return -1;
    const QRect area = chartArea();
    if (area.width() <= 0)
        return -1;
    int i = (x - area.left()) * m_records.size() / area.width();
    i = qBound(0, i, m_records.size() - 1);
    return (x >= area.left() && x <= area.right()) ? i : -1;
}

void UsageChart::mouseMoveEvent(QMouseEvent *e)
{
    const int idx = indexAtX(e->pos().x());
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        update();
    }
    if (idx >= 0) {
        const Panshi::DiskDayRecord &r = m_records.at(idx);
        const qint64 growth = (idx > 0) ? (r.total - m_records.at(idx - 1).total) : 0;
        QString txt = tr("%1\n已用 %2").arg(r.date.toString(Qt::ISODate), sizeText(r.total));
        if (idx > 0)
            txt += QStringLiteral("\n") + tr("当日增长 %1").arg(sizeText(growth));
        if (m_spikeDays.contains(r.date))
            txt += QStringLiteral("\n") + tr("⚠ 异常增长");
        QToolTip::showText(e->globalPosition().toPoint(), txt, this);
    } else {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(e);
}

void UsageChart::mousePressEvent(QMouseEvent *e)
{
    const int idx = indexAtX(e->pos().x());
    if (idx >= 0 && e->button() == Qt::LeftButton) {
        m_selected = m_records.at(idx).date;
        emit dateSelected(m_selected);
        update();
    }
    QWidget::mousePressEvent(e);
}

void UsageChart::leaveEvent(QEvent *e)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(e);
}

void UsageChart::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_loadingWidget)
        m_loadingWidget->setGeometry(rect());
    if (m_scanOverlay && m_scanning)
        m_scanOverlay->setGeometry(rect());
    if (m_checkOverlay && m_checkOverlay->isActive())
        m_checkOverlay->setGeometry(rect());
    update();
}

// ----------------------------------------------------------------------
// 绘制
// ----------------------------------------------------------------------
void UsageChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_fade);

    // 颜色一律取自 DPalette，跟随系统主题与用户主题色（重构方案 §2.2）
    const bool dark = (DGuiApplicationHelper::instance()->themeType()
                       == DGuiApplicationHelper::DarkType);
    const DPalette pa = DGuiApplicationHelper::instance()->applicationPalette();
    const QColor accent = pa.color(DPalette::Highlight);
    // V-23：异常增长标记曾取 TextLively（实测默认主题下是中性黑，环标记几乎不可见，
    // 且与图例 Semantic::Danger 的橙红色自相矛盾）。统一到 TextWarning ——
    // 与 components.cpp 的 Semantic::Danger 中央映射保持同一取色源。
    const QColor danger = pa.color(DPalette::TextWarning);
    const QColor warn = pa.color(DPalette::TextWarning);
    const QColor textSub = pa.color(DPalette::TextTips);
    QColor grid = pa.color(DPalette::FrameBorder);
    grid.setAlpha(dark ? 70 : 120);
    const QFont font = DFontSizeManager::instance()->get(DFontSizeManager::T8);
    p.setFont(font);

    // ---- 加载 / 空数据：静态占位（加载中由 DSpinner 指示，不再闪动骨架柱） ----
    if (m_loading || m_records.isEmpty()) {
        const QRect area = chartArea();
        const QColor placeholder = pa.color(DPalette::ItemBackground);
        p.setPen(Qt::NoPen);
        p.setBrush(placeholder);
        p.drawRoundedRect(area, 8, 8);
        if (!m_loading && m_records.isEmpty()) {
            p.setPen(textSub);
            p.drawText(area, Qt::AlignCenter,
                       tr("暂无监控数据：请开启监控，或点击“立即扫描”"));
        }
        return;
    }

    const QRect area = chartArea();
    const int n = m_records.size();
    const double maxTotal = niceCeil(double(m_records.at(0).total));
    double maxAll = maxTotal;
    for (const Panshi::DiskDayRecord &r : m_records)
        maxAll = qMax(maxAll, double(r.total));
    const double yMax = niceCeil(maxAll);
    const double scale = area.height() / yMax;

    // ---- 网格与 Y 轴 ----
    p.setPen(grid);
    const int gridLines = 4;
    for (int i = 0; i <= gridLines; ++i) {
        const double v = yMax * i / gridLines;
        const int y = area.bottom() - qRound(v * scale);
        p.drawLine(area.left(), y, area.right(), y);
        p.setPen(textSub);
        p.drawText(QRect(2, y - 8, 40, 16), Qt::AlignRight | Qt::AlignVCenter,
                   gbText(v));
        p.setPen(grid);
    }

    // ---- X 轴日期（首/中/尾） ----
    p.setPen(textSub);
    const QList<int> labelIdx = {0, n / 2, n - 1};
    for (int i : labelIdx) {
        const int x = area.left() + qRound((i + 0.5) * area.width() / n);
        p.drawText(QRect(x - 40, area.bottom() + 4, 80, 16), Qt::AlignHCenter | Qt::AlignTop,
                   m_records.at(i).date.toString(QStringLiteral("MM-dd")));
    }

    // ---- 面积 + 折线（始终折线图，无柱子）----
    const double slotW = double(area.width()) / n;

    // 按 reveal 动画截断：折线从左到右生长
    const int drawn = qBound(0, int(std::ceil(m_reveal * n)), n);
    QPainterPath trendPath;
    for (int i = 0; i < drawn; ++i) {
        const double x = area.left() + (i + 0.5) * slotW;
        const double y = area.bottom() - double(m_records.at(i).total) * scale;
        if (i == 0)
            trendPath.moveTo(x, y);
        else
            trendPath.lineTo(x, y);
    }

    if (drawn > 0) {
        // 面积填充（截断到已绘制部分）
        QPainterPath fill = trendPath;
        const double lastX = area.left() + (drawn - 0.5) * slotW;
        fill.lineTo(lastX, area.bottom());
        fill.lineTo(area.left() + 0.5 * slotW, area.bottom());
        fill.closeSubpath();
        QColor fillC = accent;
        fillC.setAlpha(dark ? 60 : 40);
        p.setPen(Qt::NoPen);
        p.setBrush(fillC);
        p.drawPath(fill);

        // 折线
        p.setPen(QPen(accent, 2));
        p.drawPath(trendPath);
    }

    // 数据点：正常=蓝点 / 飙升=红点 / 选中=白圈+黄点
    for (int i = 0; i < drawn; ++i) {
        const double x = area.left() + (i + 0.5) * slotW;
        const double y = area.bottom() - double(m_records.at(i).total) * scale;
        const QPointF pt(x, y);
        const bool spike = m_spikeDays.contains(m_records.at(i).date);
        const bool sel = (m_records.at(i).date == m_selected);
        if (sel) {
            // 选中点：主题底色描边圈 + 警告色实心点
            p.setPen(QPen(pa.color(DPalette::Base), 2.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(pt, 7.0, 7.0);
            p.setPen(Qt::NoPen);
            p.setBrush(warn);
            p.drawEllipse(pt, 3.6, 3.6);
        } else if (spike) {
            p.setPen(QPen(danger, 1.5));
            p.setBrush(QColor(danger.red(), danger.green(), danger.blue(), 40));
            p.drawEllipse(pt, 5.8, 5.8);
            p.setPen(Qt::NoPen);
            p.setBrush(danger);
            p.drawEllipse(pt, 3.2, 3.2);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(accent);
            p.drawEllipse(pt, 3.2, 3.2);
        }
    }

    // ---- 悬停高亮 ----
    if (m_hoverIndex >= 0 && m_hoverIndex < n) {
        const double x = area.left() + (m_hoverIndex + 0.5) * slotW;
        QColor hl = dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 14);
        p.setPen(Qt::NoPen);
        p.setBrush(hl);
        p.drawRect(QRectF(x - slotW / 2, area.top(), slotW, area.height()));
        p.setPen(QPen(textSub, 1));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        // hover 点放大
        const double hy = area.bottom() - double(m_records.at(m_hoverIndex).total) * scale;
        p.setPen(QPen(textSub, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(x, hy), 7.5, 7.5);
    }
}
