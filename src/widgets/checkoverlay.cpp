// SPDX-License-Identifier: GPL-3.0-or-later
#include "checkoverlay.h"
#include "radarwidget.h"

#include <DLabel>
#include <DFontSizeManager>
#include <DPalette>
#include <DGuiApplicationHelper>

#include <QVBoxLayout>
#include <QPainter>
#include <QTimer>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace {
QString fmtMs(qint64 ms)
{
    if (ms < 1000)
        return QStringLiteral("%1 ms").arg(ms);
    return QString::number(ms / 1000.0, 'f', 2) + QStringLiteral(" s");
}
} // namespace

CheckOverlay::CheckOverlay(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(28, 22, 28, 22);
    lay->setSpacing(8);

    m_radar = new RadarWidget(this);
    lay->addWidget(m_radar, 0, Qt::AlignHCenter);

    m_title = new DLabel(this);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(m_title, DFontSizeManager::T5);
    lay->addWidget(m_title);

    m_steps = new DLabel(this);
    m_steps->setAlignment(Qt::AlignCenter);
    m_steps->setForegroundRole(DPalette::TextTips);
    m_steps->setWordWrap(true);
    DFontSizeManager::instance()->bind(m_steps, DFontSizeManager::T7);
    lay->addWidget(m_steps);

    m_total = new DLabel(this);
    m_total->setAlignment(Qt::AlignCenter);
    m_total->setForegroundRole(DPalette::TextTips);
    m_total->setObjectName(QStringLiteral("panshiTipText"));
    DFontSizeManager::instance()->bind(m_total, DFontSizeManager::T7);
    lay->addWidget(m_total);

    hide();
}

void CheckOverlay::start(const QString &title)
{
    m_active = true;
    m_lines.clear();
    if (m_title)
        m_title->setText(title);
    if (m_steps)
        m_steps->setText(QString());
    if (m_total)
        m_total->setText(QString());
    m_timer.restart();
    show();
    raise();
}

void CheckOverlay::addStep(const QString &name, bool ok, qint64 elapsedMs)
{
    if (!m_active)
        return;
    QString line;
    if (ok)
        line = tr("✓ %1    %2").arg(name, fmtMs(elapsedMs));
    else if (elapsedMs < 0)
        line = tr("⟳ %1    …").arg(name);
    else
        line = tr("✗ %1    %2").arg(name, fmtMs(elapsedMs));
    m_lines.append(line);
    if (m_steps)
        m_steps->setText(m_lines.join(QLatin1Char('\n')));
}

void CheckOverlay::finish(qint64 totalMs)
{
    if (!m_active)
        return;
    if (m_total)
        m_total->setText(tr("检查完成 · 共 %1").arg(fmtMs(totalMs)));
    // 短暂停留后自动隐藏
    QTimer::singleShot(1400, this, [this] {
        m_active = false;
        hide();
    });
}

void CheckOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = DGuiApplicationHelper::instance()->themeType()
                      == DGuiApplicationHelper::DarkType;
    QColor bg = DGuiApplicationHelper::instance()->applicationPalette()
                    .color(DPalette::ItemBackground);
    bg.setAlpha(dark ? 18 : 8);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 12, 12);
}
