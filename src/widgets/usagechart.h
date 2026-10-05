// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QDate>
#include <QSet>
#include <QList>

namespace Panshi { struct DiskDayRecord; }

/**
 * 磁盘占用时间线自绘控件。
 * - 跨度 ≤180 天：柱状图（飙升日 danger 色、选中高亮、顶部趋势线）；
 * - 跨度 >180 天：自动切换为折线/面积图（柱过密不可读）；
 * - 悬停显示 Tooltip（日期/已用/当日增长），点击发出 dateSelected；
 * - 加载态显示骨架占位 + 加载文字；
 * - 数据到达时逐柱平滑生长、切换时淡入淡出（遵循系统"减少动态效果"时关闭动画）。
 */
namespace Dtk { namespace Widget { class DSpinner; class DLabel; } }

class UsageChart : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double reveal READ reveal WRITE setReveal)
    Q_PROPERTY(double fade READ fade WRITE setFade)
public:
    explicit UsageChart(QWidget *parent = nullptr);

    void setRecords(const QList<Panshi::DiskDayRecord> &records);
    void setSpikeDays(const QSet<QDate> &days);
    void setSelectedDate(const QDate &d);
    void setLoading(bool loading, const QString &text = QString());

    /** 扫描模式：显示扫描动态面板（雷达动画 + 实时路径 + 流动进度） */
    void setScanning(bool on, const QString &title = QString());
    void setScanProgress(int done, const QString &path);

    /** 数据加载进度（确定进度模式）：已加载 done/total */
    void setLoadProgress(int done, int total);

    /** 启动检查面板：雷达动画 + 检查步骤列表 */
    void setChecking(bool on, const QString &title = QString());
    void addCheckStep(const QString &name, bool ok, qint64 elapsedMs);
    void finishCheck(qint64 totalMs);

    void setAnimationsEnabled(bool on) { m_animEnabled = on; if (!on) { m_reveal = 1.0; m_fade = 1.0; } update(); }

    /** 加载态是否由内部 DSpinner 指示（无数据时显示提示文字） */
    bool isLoading() const { return m_loading; }

    QDate selectedDate() const { return m_selected; }

    /** 系统"减少动态效果"是否开启（gsettings 探测，结果缓存） */
    static bool reduceMotionRequested();

signals:
    void dateSelected(const QDate &date);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    double reveal() const { return m_reveal; }
    void setReveal(double v) { m_reveal = v; update(); }
    double fade() const { return m_fade; }
    void setFade(double v) { m_fade = v; update(); }
    QRect chartArea() const;
    int  indexAtX(int x) const;
    void ensureAnimations();

    Dtk::Widget::DSpinner *m_spinner = nullptr;
    Dtk::Widget::DLabel *m_loadingLabel = nullptr;
    QWidget *m_loadingWidget = nullptr;
    class ScanOverlay *m_scanOverlay = nullptr;
    class CheckOverlay *m_checkOverlay = nullptr;
    bool m_scanning = false;

    QList<Panshi::DiskDayRecord> m_records;
    QSet<QDate> m_spikeDays;
    QDate m_selected;
    bool m_loading = false;
    QString m_loadingText;
    double m_reveal = 1.0;
    double m_fade = 1.0;
    bool m_animEnabled = true;
    int m_hoverIndex = -1;
    class QPropertyAnimation *m_revealAnim = nullptr;
    class QPropertyAnimation *m_fadeAnim = nullptr;
};
