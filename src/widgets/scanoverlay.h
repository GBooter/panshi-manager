// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QColor>

class RadarWidget;
namespace Dtk { namespace Widget { class DProgressBar; class DLabel; } }

/**
 * 扫描动态进度面板（雷达动画 + 实时路径 + 流动进度条）。
 * 用于"立即扫描"期间在图表区展示生动、统一的扫描进度。
 */
class ScanOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ScanOverlay(QWidget *parent = nullptr);

    /** 更新进度：已处理项数与当前信息（total<=0 时用流动动画） */
    void setProgress(int done, int total, const QString &text);

    /** busy 流动动画（true）或确定进度（false） */
    void setBusy(bool on);

    /** 面板标题（如"正在扫描系统盘目录…"） */
    void setTitleText(const QString &title);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    RadarWidget *m_radar = nullptr;
    Dtk::Widget::DProgressBar *m_bar = nullptr;
    Dtk::Widget::DLabel *m_title = nullptr;
    Dtk::Widget::DLabel *m_stat = nullptr;
};
