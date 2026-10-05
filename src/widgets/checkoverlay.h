// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QElapsedTimer>

class RadarWidget;
namespace Dtk { namespace Widget { class DLabel; } }

/**
 * 启动检查动态面板：雷达动画 + 检查步骤列表（每步耗时）+ 总耗时。
 * 用于应用/页面启动检查系统信息时展示具体进度。
 */
class CheckOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit CheckOverlay(QWidget *parent = nullptr);

    /** 开始检查：重置步骤列表并显示 */
    void start(const QString &title);

    /** 添加一个已完成步骤（✓）或进行中步骤（⟳，elapsedMs<0） */
    void addStep(const QString &name, bool ok, qint64 elapsedMs);

    /** 全部完成：显示总耗时，短暂停留后自动隐藏 */
    void finish(qint64 totalMs);

    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    RadarWidget *m_radar = nullptr;
    Dtk::Widget::DLabel *m_title = nullptr;
    Dtk::Widget::DLabel *m_steps = nullptr;
    Dtk::Widget::DLabel *m_total = nullptr;
    QStringList m_lines;
    QElapsedTimer m_timer;
    bool m_active = false;
};
