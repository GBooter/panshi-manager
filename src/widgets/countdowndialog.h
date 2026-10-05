// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <DAbstractDialog>
#include <DWarningButton>

#include <QTimer>
#include <QShowEvent>
#include <QPainter>

DWIDGET_USE_NAMESPACE

/**
 * 圆环倒计时表盘：红色弧线随剩余秒数递减，环心显示剩余秒数。
 * 仅供 PanshiCountdownDialog 使用。
 */
class CountdownRing : public QWidget
{
public:
    explicit CountdownRing(QWidget *parent = nullptr);

    void setRemain(int remain);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_remain = 30;
    static constexpr int kTotalSeconds = 30;
};

/**
 * 30 秒倒计时危险确认对话框（「完全关闭磐石保护」专用）。
 * - 顶部圆环倒计时 + 标题区红色警告徽章；
 * - 倒计时期间确认按钮禁用并显示剩余秒数，归零后启用；
 * - 取消按钮任何时刻可点，返回 QDialog::Rejected；
 * - 亮暗主题自适应（DPalette / 应用 QSS），字体不硬编码绝对字号。
 */
class PanshiCountdownDialog : public DAbstractDialog
{
    Q_OBJECT
public:
    explicit PanshiCountdownDialog(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onTick();

    CountdownRing *m_ring = nullptr;
    DWarningButton *m_btnConfirm = nullptr;
    QTimer *m_timer = nullptr;
    int m_remain = 30;
};
