// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTimer>
#include <QColor>

/**
 * 自绘雷达扫描动画（同心环 + 旋转扫描扇形 + 中心点），
 * 供扫描/加载/检查等动态面板复用。
 */
class RadarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RadarWidget(QWidget *parent = nullptr);

    /** 当前主题下的雷达主色 */
    static QColor accentColor();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QTimer m_timer;
    double m_angle = 0.0;
};
