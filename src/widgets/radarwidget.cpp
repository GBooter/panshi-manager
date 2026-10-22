// SPDX-License-Identifier: GPL-3.0-or-later
#include "radarwidget.h"

#include <DGuiApplicationHelper>
#include <DPalette>
#include <dtkwidget_global.h>

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

QColor RadarWidget::accentColor()
{
    // 强调色取 DPalette::Highlight，跟随系统主题与用户主题色（重构方案 §2.2）
    return Dtk::Gui::DGuiApplicationHelper::instance()->applicationPalette()
        .color(Dtk::Gui::DPalette::Highlight);
}

RadarWidget::RadarWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(96, 96);
    m_timer.start(50);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_angle += 5.0;
        if (m_angle >= 360.0)
            m_angle -= 360.0;
        update();
    });
}

void RadarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor accent = accentColor();
    const QRectF r = rect().adjusted(4, 4, -4, -4);
    const QPointF c = r.center();

    // 圆底
    p.setPen(Qt::NoPen);
    const bool dark = Dtk::Gui::DGuiApplicationHelper::instance()->themeType()
                      == Dtk::Gui::DGuiApplicationHelper::DarkType;
    p.setBrush(dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 8));
    p.drawEllipse(r);

    // 同心环 + 十字线
    QColor ring = accent;
    ring.setAlpha(90);
    p.setPen(QPen(ring, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r.adjusted(14, 14, -14, -14));
    p.drawEllipse(r.adjusted(28, 28, -28, -28));
    p.drawLine(QPointF(r.left() + 4, c.y()), QPointF(r.right() - 4, c.y()));
    p.drawLine(QPointF(c.x(), r.top() + 4), QPointF(c.x(), r.bottom() - 4));

    // 旋转扫描扇形
    QPainterPath fan;
    fan.moveTo(c);
    fan.arcTo(r, m_angle - 40.0, 40.0);
    fan.closeSubpath();
    QColor fanC = accent;
    fanC.setAlpha(55);
    p.setPen(Qt::NoPen);
    p.setBrush(fanC);
    p.drawPath(fan);

    // 扫描线
    const double rad = qDegreesToRadians(m_angle);
    const double lineLen = r.width() / 2.0 - 6.0;
    p.setPen(QPen(accent, 2.0));
    p.drawLine(c, QPointF(c.x() + lineLen * qCos(rad), c.y() + lineLen * qSin(rad)));

    // 中心点
    p.setBrush(accent);
    p.setPen(Qt::NoPen);
    p.drawEllipse(c, 4.5, 4.5);
}
