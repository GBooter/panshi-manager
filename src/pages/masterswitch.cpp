// SPDX-License-Identifier: GPL-3.0-or-later
#include "masterswitch.h"
#include "common.h"
#include "../widgets/countdowndialog.h"

#include <DFontSizeManager>
#include <DPalette>
#include <DPushButton>

#include <QEasingCurve>
#include <QFutureWatcher>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QtConcurrent>
#include <QHBoxLayout>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

using namespace Panshi;

namespace PanshiSwitch {

PanshiSwitchRow::PanshiSwitchRow(BasePage *page, bool withDetail, QWidget *parent)
    : QWidget(parent)
    , m_page(page)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    lay->addWidget(Ui::iconBadge(QStringLiteral("security"), 40));

    auto *tl = new QVBoxLayout;
    tl->setSpacing(3);
    tl->addWidget(Ui::rowLabel(tr("磐石功能")));
    m_subtitle = new DLabel(this);
    m_subtitle->setWordWrap(true);
    DFontSizeManager::instance()->bind(m_subtitle, DFontSizeManager::T6);
    tl->addWidget(m_subtitle);
    lay->addLayout(tl, 1);

    if (withDetail) {
        // 阶段 4：不再依赖 QSS 的 #panshiLinkBtn（属性选择器），
        // 改为「平铺按钮 + 手型光标 + 主题色前景」——颜色取自 DPalette
        m_detail = new DPushButton(tr("详情"), this);
        m_detail->setFlat(true);
        m_detail->setCursor(Qt::PointingHandCursor);
        // 详情链接按钮：主题色前景。DPushButton 没有 DLabel 那样的
        // setForegroundRole(DPalette::ColorType) 重载（DTK 仅 DLabel/DTipLabel 提供），
        // 故直接设 palette 的 ButtonText——与被删掉的 #panshiLinkBtn QSS 等效。
        QPalette pa = m_detail->palette();
        pa.setColor(QPalette::ButtonText, PanshiUi::semanticColor(PanshiUi::Semantic::Accent));
        m_detail->setPalette(pa);
        connect(m_detail, &DPushButton::clicked, this, &PanshiSwitchRow::detailRequested);
        lay->addWidget(m_detail);
    }

    m_switch = new DSwitchButton(this);
    connect(m_switch, &DSwitchButton::toggled, this, &PanshiSwitchRow::onUserToggled);
    lay->addWidget(m_switch, 0, Qt::AlignVCenter);

    applyStatus(presentStatus(WritableStatus{}));
}

void PanshiSwitchRow::applyStatus(const Presentation &p)
{
    // 程序化同步状态：屏蔽信号，避免误触发切换流程
    m_switch->blockSignals(true);
    m_switch->setChecked(p.known && p.protectedOn);
    m_switch->setEnabled(p.known);
    m_switch->blockSignals(false);

    m_subtitle->setText(p.subtitle);
    // 阶段 4：语义色直接走 DPalette，不再 setProperty("stat") + QSS 属性选择器。
    // 颜色不是唯一载体——文案本身（保护中 / 已临时关闭 / 已完全关闭）已表达状态。
    PanshiUi::applySemantic(m_subtitle, p.known ? p.semantic : PanshiUi::Semantic::Muted);
}

void PanshiSwitchRow::onUserToggled(bool checked)
{
    // 执行期间禁用开关避免重复触发；结束后发出 switchChanged 由页面刷新回显真实状态
    m_switch->setEnabled(false);
    requestSwitch(m_page, checked, [this](bool) {
        m_switch->setEnabled(true);
        emit switchChanged();
    });
}

qreal PanshiSwitchRow::highlight() const
{
    return m_highlight;
}

void PanshiSwitchRow::setHighlight(qreal v)
{
    m_highlight = v;
    update();
}

void PanshiSwitchRow::flashHighlight()
{
    // 与 PanshiRow::flashHighlight() 同一实现意图（Accent 柔和底闪一下再淡出）。
    // 两处各自持有动画对象而不抽公共基类：PanshiRow 在组件层（无 moc），
    // 本类在页面层（有 Q_OBJECT），强行合并会把 moc 依赖引入组件层。
    const int peakMs = PanshiUi::Motion::HighlightHold;
    const int fadeMs = PanshiUi::Motion::HighlightFade;

    if (PanshiUi::reduceMotionRequested()) {
        if (m_highlightAnim)
            m_highlightAnim->stop();
        m_highlight = 1.0;
        update();
        QTimer::singleShot(peakMs, this, [this] {
            m_highlight = 0.0;
            update();
        });
        return;
    }

    if (!m_highlightAnim)
        m_highlightAnim = new QPropertyAnimation(this, "highlight", this);
    m_highlightAnim->stop();
    m_highlightAnim->setDuration(fadeMs);
    m_highlightAnim->setStartValue(1.0);
    m_highlightAnim->setEndValue(0.0);
    m_highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_highlight = 1.0;
    update();

    // 先保持峰值，再启动淡出（复用同一动画对象，避免叠加）。
    //
    // connect 必须只建一次，且**不能**用 Qt::UniqueConnection —— 它仅支持
    // 成员函数指针，对 lambda 会在运行时拒绝（"unique connections require a
    // pointer to member function"），于是 connect 静默失败、peak 后无人 start
    // 动画，高亮会卡在满亮永不淡出。这里改为「仅当未创建时 connect 一次」，
    // 与 components.cpp 的 PanshiRow::flashHighlight 保持一致（阶段 5 编译修正）。
    if (!m_highlightClear) {
        m_highlightClear = new QTimer(this);
        m_highlightClear->setSingleShot(true);
        QObject::connect(m_highlightClear, &QTimer::timeout, this,
                         [this] { m_highlightAnim->start(); });
    }
    m_highlightClear->stop();
    m_highlightClear->start(peakMs);
}

void PanshiSwitchRow::paintEvent(QPaintEvent *e)
{
    if (m_highlight > 0.0) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c = PanshiUi::softFill(PanshiUi::Semantic::Accent);
        c.setAlphaF(c.alphaF() * m_highlight);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(rect()).adjusted(-PanshiUi::Space::M, -PanshiUi::Space::XS,
                                                  PanshiUi::Space::M, PanshiUi::Space::XS),
                          PanshiUi::Radius::Frame, PanshiUi::Radius::Frame);
    }
    QWidget::paintEvent(e);
}

void requestSwitch(BasePage *page, bool protectOn, const std::function<void(bool)> &onDone)
{
    if (!page) {
        onDone(false);
        return;
    }

    bool clearAfterReboot = false;

    // -------------------------------------------------------- 1. 决策确认
    // 阶段 4：两处裸 DDialog 收敛到全应用唯一的 ConfirmRequest / askConfirm。
    PanshiUi::ConfirmRequest req;
    req.cancelText = QObject::tr("取消");

    if (protectOn) {
        // 开关拨到 ON：恢复只读保护，普通确认即可
        req.level = PanshiUi::DialogLevel::Normal;
        req.title = QObject::tr("开启磐石保护");
        req.message = QObject::tr("确定要恢复核心目录只读保护吗？此操作需要重启后生效。");
        req.confirmText = QObject::tr("恢复保护");
        if (!PanshiUi::askConfirm(page, req)) {
            onDone(false);
            return;
        }
    } else {
        // 开关拨到 OFF：在**一次**决策里选定关闭方式。
        // 原先的三个等权按钮把"临时/完全"的差别藏在括号里，且按次序表达
        // 风险等级，可读性差；改为单选 + 明确后果清单。
        req.level = PanshiUi::DialogLevel::Warning;
        req.title = QObject::tr("关闭磐石保护");
        req.message = QObject::tr("关闭后核心目录将解除只读保护，恶意软件与误操作可能直接破坏系统。");
        req.consequences = {
            QObject::tr("核心系统目录将变为可写"),
            QObject::tr("恶意软件或误操作可直接破坏系统文件"),
            QObject::tr("重启后按所选方式决定是否恢复保护"),
        };
        req.options = {
            QObject::tr("临时关闭（重启后自动恢复）"),
            QObject::tr("完全关闭（重启后仍保持关闭）"),
        };
        req.optionDefault = 0;   // 默认落在低风险的「临时关闭」
        int choice = 0;
        req.optionResult = &choice;
        req.confirmText = QObject::tr("关闭保护");

        if (!PanshiUi::askConfirm(page, req)) {
            onDone(false);
            return;
        }
        clearAfterReboot = (choice == 0);

        if (!clearAfterReboot) {
            // 完全关闭：30 秒倒计时危险确认（自带风险清单与「我已知晓」确认）
            PanshiCountdownDialog cd(page);
            // 降为 WindowModal：30 秒倒计时期间主窗口仍可拖动
            // （根因见 components.h::makeDialogNonBlockingWindowModal）
            PanshiUi::makeDialogNonBlockingWindowModal(&cd);
            if (cd.exec() != QDialog::Accepted) {
                onDone(false);
                return;
            }
        }
        // 临时关闭属低风险路径，不再二次确认
    }

    // -------------------------------------------------------- 2. 后台执行
    // 阶段 4：不再盖整页遮罩（showLoading），执行期由页面自带的 L2 局部加载表达；
    // 这里只负责把结果写回页面的反馈 Banner。
    auto *watcher = new QFutureWatcher<CommandResult>(page);
    QObject::connect(watcher, &QFutureWatcher<CommandResult>::finished, page,
                     [watcher, page, onDone] {
        watcher->deleteLater();
        const CommandResult r = watcher->result();
        if (r.ok) {
            page->showFeedback(PanshiUi::Semantic::Success,
                               QObject::tr("设置成功，请重启系统使设置生效。"), true);
        } else {
            // 原始输出收进「详情」，正文只留可行动说明（方案 §4.4 / O11）
            page->showFeedbackWithDetails(PanshiUi::Semantic::Danger,
                                          QObject::tr("设置失败，请查看详情了解具体原因。"),
                                          QObject::tr("完整输出"),
                                          r.errorMessage);
        }
        onDone(r.ok);
    });
    watcher->setFuture(QtConcurrent::run([protectOn, clearAfterReboot]() -> CommandResult {
        return protectOn ? disableWritable() : enableWritable(clearAfterReboot, true);
    }));
}

} // namespace PanshiSwitch
