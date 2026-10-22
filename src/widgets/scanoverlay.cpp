// SPDX-License-Identifier: GPL-3.0-or-later
#include "scanoverlay.h"
#include "radarwidget.h"

#include <DProgressBar>
#include <DLabel>
#include <DFontSizeManager>
#include <DPalette>
#include <DGuiApplicationHelper>

#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace {
/** 浮层面板底色：取 DPalette ItemBackground，半透明覆盖在图表上 */
QColor panelBackground()
{
    const bool dark = DGuiApplicationHelper::instance()->themeType()
                      == DGuiApplicationHelper::DarkType;
    QColor c = DGuiApplicationHelper::instance()->applicationPalette()
                   .color(DPalette::ItemBackground);
    c.setAlpha(dark ? 18 : 8);
    return c;
}
} // namespace

ScanOverlay::ScanOverlay(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(28, 22, 28, 22);
    lay->setSpacing(10);

    m_radar = new RadarWidget(this);
    lay->addWidget(m_radar, 0, Qt::AlignHCenter);

    m_title = new DLabel(tr("正在扫描…"), this);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(m_title, DFontSizeManager::T5);
    lay->addWidget(m_title);

    m_bar = new DProgressBar(this);
    m_bar->setRange(0, 0);          // busy 模式：流动动画，不误导百分比
    m_bar->setTextVisible(false);
    m_bar->setFixedWidth(340);
    m_bar->setFixedHeight(8);
    lay->addWidget(m_bar, 0, Qt::AlignHCenter);

    m_stat = new DLabel(this);
    m_stat->setAlignment(Qt::AlignCenter);
    m_stat->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(m_stat, DFontSizeManager::T7);
    lay->addWidget(m_stat);
}

void ScanOverlay::setTitleText(const QString &title)
{
    if (m_title)
        m_title->setText(title);
}

void ScanOverlay::setBusy(bool on)
{
    if (!m_bar)
        return;
    if (on) {
        m_bar->setRange(0, 0);          // 流动动画
        m_bar->setValue(0);
    } else {
        m_bar->setRange(0, 100);        // 确定进度
        m_bar->setValue(0);
        m_bar->setTextVisible(true);
    }
}

void ScanOverlay::setProgress(int done, int total, const QString &text)
{
    if (m_bar) {
        if (total > 0) {
            m_bar->setRange(0, total);
            m_bar->setValue(qBound(0, done, total));
            m_bar->setTextVisible(true);
        } else {
            m_bar->setRange(0, 0);
            m_bar->setValue(0);
            m_bar->setTextVisible(false);
        }
    }
    if (m_stat)
        m_stat->setText(text);
}

void ScanOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // 圆角面板底（浅色浮层）
    p.setPen(Qt::NoPen);
    p.setBrush(panelBackground());
    p.drawRoundedRect(rect(), 12, 12);
}
