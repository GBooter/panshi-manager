// SPDX-License-Identifier: GPL-3.0-or-later
#include "basepage.h"

#include <DSpinner>
#include <DLabel>
#include <DFontSizeManager>
#include <DPalette>
#include <DGuiApplicationHelper>

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

BasePage::BasePage(QWidget *parent)
    : QWidget(parent)
{
    m_mask = new QWidget(this);
    m_mask->setObjectName(QStringLiteral("panshiMask"));
    auto *lay = new QVBoxLayout(m_mask);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addStretch();

    auto *center = new QWidget(m_mask);
    auto *cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(10);

    m_spinner = new DSpinner(center);
    m_spinner->setFixedSize(32, 32);
    m_spinner->start();
    cl->addWidget(m_spinner, 0, Qt::AlignHCenter);

    m_maskLabel = new DLabel(center);
    m_maskLabel->setObjectName(QStringLiteral("panshiMaskText"));
    DFontSizeManager::instance()->bind(m_maskLabel, DFontSizeManager::T6);
    m_maskLabel->setAlignment(Qt::AlignCenter);
    cl->addWidget(m_maskLabel);

    lay->addWidget(center);
    lay->addStretch();
    m_mask->hide();
}

void BasePage::showLoading(const QString &text)
{
    if (!text.isEmpty()) {
        m_loadingText = text;
        m_maskLabel->setText(text);
    }
    m_mask->setGeometry(rect());
    m_mask->raise();
    m_mask->show();
}

void BasePage::hideLoading()
{
    m_mask->hide();
}

void BasePage::showFeedback(PanshiUi::Semantic s, const QString &text, bool autoHide)
{
    // 未注册 Banner 的页面（如启动早期或纯展示页）不因反馈而中断流程
    if (!m_feedback || text.isEmpty())
        return;
    m_feedback->setSemantic(s);
    m_feedback->setClosable(true);
    m_feedback->setText(text);
    // 成功类短反馈自动收尾；失败/警告常驻，直到用户关闭
    if (autoHide)
        m_feedback->autoDismiss(5000);
}

void BasePage::showFeedbackWithDetails(PanshiUi::Semantic s, const QString &text,
                                       const QString &detailsTitle, const QString &rawText,
                                       bool autoHide)
{
    // 未注册 Banner 或正文为空时，退化为无详情的普通反馈
    if (!m_feedback || text.isEmpty()) {
        return;
    }
    // 先清掉上一条可能残留的详情（Banner 是复用的），
    // 再走 showFeedback 统一设置语义/可关闭/自动消失 —— 避免两处逻辑分叉。
    m_feedback->setDetails(QString(), QString());
    showFeedback(s, text, autoHide);
    if (!rawText.isEmpty())
        m_feedback->setDetails(detailsTitle, rawText);
}

void BasePage::registerSearchTarget(const QString &key, QWidget *row, QScrollArea *scrollArea)
{
    if (key.isEmpty() || !row)
        return;
    SearchTarget t;
    t.row = row;
    t.scroll = scrollArea;
    m_searchTargets.insert(key, t);
}

bool BasePage::highlightSetting(const QString &key)
{
    const auto it = m_searchTargets.constFind(key);
    if (it == m_searchTargets.constEnd() || !it->row)
        return false;

    QWidget *row = it->row;
    // 先滚动到该行（未登记滚动区则跳过）
    if (it->scroll && it->scroll->viewport()) {
        // ensureWidgetVisible 的锚点直接用行本身：QScrollArea 会沿父链
        // 累加偏移，对嵌套布局同样成立，无需手算坐标。
        it->scroll->ensureWidgetVisible(row, PanshiUi::Space::M, PanshiUi::Space::M);
    }

    // 触发高亮。两条路径：
    //  1. PanshiRow 属组件层（basepage.h 已含 ui/components.h），可直接 cast；
    //  2. PanshiSwitchRow 在页面层且反向包含本头文件，基类不能包含它
    //     （会形成循环包含）——改用它已带 Q_OBJECT 的元对象调用。
    if (auto *r = qobject_cast<PanshiUi::PanshiRow *>(row))
        r->flashHighlight();
    else if (row->inherits("PanshiSwitch::PanshiSwitchRow"))
        QMetaObject::invokeMethod(row, "flashHighlight", Qt::DirectConnection);
    return true;
}

void BasePage::beginTask()
{
    ++m_inFlight;
}

void BasePage::endTask()
{
    if (m_inFlight > 0)
        --m_inFlight;
    // 归零才算"空闲"：页面可能连续发起多段任务（如总览页的快速状态 + 慢速统计），
    // 中途归零会诱使「到达后动作」在第二段数据尚未落地时就执行。
    if (m_inFlight == 0)
        emit idleReached();
}

void BasePage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_mask)
        m_mask->setGeometry(rect());
    feedResponsiveCardRow();
}

void BasePage::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // 隐藏页在 QStackedWidget 里不跟随窗口 resize，首次 show 时必须补一次：
    // 此时布局已就绪，viewport 宽度才是真实值（见 basepage.h 的说明）。
    feedResponsiveCardRow();
}

bool BasePage::eventFilter(QObject *watched, QEvent *e)
{
    if (m_cardRowScroll && watched == m_cardRowScroll->viewport()
        && e->type() == QEvent::Resize) {
        feedResponsiveCardRow();
    }
    return QWidget::eventFilter(watched, e);
}

void BasePage::registerResponsiveCardRow(PanshiUi::PanshiResponsiveCardRow *row,
                                         QScrollArea *scrollArea, int hPadding)
{
    m_cardRow = row;
    m_cardRowScroll = scrollArea;
    m_cardRowPadding = hPadding;
    if (m_cardRowScroll && m_cardRowScroll->viewport())
        m_cardRowScroll->viewport()->installEventFilter(this);
    feedResponsiveCardRow();
}

void BasePage::feedResponsiveCardRow()
{
    if (!m_cardRow)
        return;
    // 优先用 viewport 宽度：它已扣除侧边栏与竖向滚动条，是内容区真实可用宽度。
    int avail = width();
    if (m_cardRowScroll && m_cardRowScroll->viewport())
        avail = m_cardRowScroll->viewport()->width();
    m_cardRow->setAvailableWidth(avail - m_cardRowPadding);
}
