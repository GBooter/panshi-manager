// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "../backend.h"

#include "ui/components.h"

#include <DSwitchButton>
#include <DLabel>

#include <QObject>
#include <QString>
#include <functional>

DWIDGET_USE_NAMESPACE

/**
 * 「磐石功能」总开关共享逻辑：总览页与系统保护页共用，
 * 保证两处开关的状态文案、切换确认流程与执行行为完全一致。
 */
namespace PanshiSwitch {

/** 开关行展示状态（由 writable.json 状态映射，见设计文档 §2.4） */
struct Presentation
{
    bool known = false;                       // 状态是否可读（writable.json 读取失败时为 false，开关置灰）
    bool protectedOn = false;                 // 开关选中态：true=保护开启
    QString subtitle;                         // 状态副标题文案
    PanshiUi::Semantic semantic = PanshiUi::Semantic::Muted;  // 副标题语义色
};

inline Presentation presentStatus(const Panshi::WritableStatus &w)
{
    Presentation p;
    if (w.raw.isEmpty()) {
        p.subtitle = QObject::tr("未知状态 · 无法读取磐石配置");
        return p;   // semantic 保持 Muted
    }
    p.known = true;
    if (!w.enable) {
        p.protectedOn = true;
        p.subtitle = QObject::tr("保护中 · 核心目录只读");
        p.semantic = PanshiUi::Semantic::Success;
    } else if (w.clearAfterReboot) {
        p.subtitle = QObject::tr("已临时关闭 · 重启后自动恢复");
        p.semantic = PanshiUi::Semantic::Warning;
    } else {
        p.subtitle = QObject::tr("已完全关闭 · 重启后仍保持关闭");
        p.semantic = PanshiUi::Semantic::Danger;
    }
    return p;
}

/**
 * 切换流程：确认/选择对话框 → 后台执行(L2 局部加载) → 结果提示 → onDone(成功与否)
 *
 * 反馈去向（阶段 4）：执行结果不再弹模态 info/error 对话框，而是写入
 * page 的反馈 Banner —— 由 BasePage::setFeedbackBanner() 注入的宿主承接。
 * 未注入宿主的页面自动跳过（onDone 仍会被调用）。
 */
void requestSwitch(BasePage *page, bool protectOn, const std::function<void(bool)> &onDone);

/**
 * 「磐石功能」开关行：图标徽章 + 标题 + 状态副标题 + DSwitchButton。
 * withDetail=true 时附带"详情"按钮（总览页样式，点击发出 detailRequested）。
 */
class PanshiSwitchRow : public QWidget
{
    Q_OBJECT
    /** 高亮强度 0..1（设置搜索落点提示；QPropertyAnimation 驱动） */
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight)
public:
    explicit PanshiSwitchRow(BasePage *page, bool withDetail = false, QWidget *parent = nullptr);

    /** 按系统实际状态更新展示（安全：不会触发切换流程） */
    void applyStatus(const Presentation &p);

    /**
     * 设置搜索命中后的落点提示：Accent 柔和底闪一下再淡出（阶段 4）。
     * Q_INVOKABLE：BasePage::highlightSetting 所在文件不能包含本头文件
     * （本头文件反向包含 basepage.h），故经元对象调用。
     */
    Q_INVOKABLE void flashHighlight();

    /** Q_PROPERTY 访问器（动画驱动用，业务侧请调 flashHighlight()） */
    qreal highlight() const;
    void setHighlight(qreal v);

signals:
    /** 切换流程结束（无论成败/取消），页面应刷新以回显真实状态 */
    void switchChanged();
    /** "详情"按钮被点击 */
    void detailRequested();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void onUserToggled(bool checked);

    BasePage *m_page = nullptr;
    DSwitchButton *m_switch = nullptr;
    DLabel *m_subtitle = nullptr;
    /** "详情"按钮（withDetail=false 时为 nullptr） */
    DPushButton *m_detail = nullptr;
    /** 高亮强度 0..1；>0 时绘制 Accent 柔和底 */
    qreal m_highlight = 0.0;
    class QPropertyAnimation *m_highlightAnim = nullptr;
    class QTimer *m_highlightClear = nullptr;
};

} // namespace PanshiSwitch
