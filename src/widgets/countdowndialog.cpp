// SPDX-License-Identifier: GPL-3.0-or-later
#include "countdowndialog.h"

#include "ui/components.h"

#include <DFontSizeManager>
#include <DPalette>
#include <DGuiApplicationHelper>
#include <DPushButton>
#include <DLabel>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QStringList>

CountdownRing::CountdownRing(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(84, 84);
}

void CountdownRing::setRemain(int remain)
{
    m_remain = remain;
    update();
}

void CountdownRing::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int s = qMin(width(), height());
    const QRectF ring(width() / 2.0 - s / 2.0 + 7, height() / 2.0 - s / 2.0 + 7, s - 14.0, s - 14.0);

    // 底环（半透明灰，两主题通用）
    QPen pen(QColor(127, 127, 127, 60), 6, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawArc(ring, 0, 360 * 16);

    // 红色进度弧：随 30→0 递减。V-23：危险色统一取 TextWarning
    // （TextLively 在默认主题实测为中性黑，与 Semantic::Danger 中央映射不一致）
    const int span = int(360.0 * m_remain / kTotalSeconds);
    if (span > 0) {
        const QColor danger = DGuiApplicationHelper::instance()->applicationPalette()
                                  .color(DPalette::TextWarning);
        pen.setColor(danger);
        p.setPen(pen);
        p.drawArc(ring, 90 * 16, -span * 16);
    }

    // 环心剩余秒数
    QFont f = p.font();
    f.setPixelSize(qMax(16, int(s * 0.32)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(ring, Qt::AlignCenter, QString::number(m_remain));
}

PanshiCountdownDialog::PanshiCountdownDialog(QWidget *parent)
    : DAbstractDialog(parent)
{
    setFixedWidth(560);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(32, 28, 32, 24);
    lay->setSpacing(16);

    // 顶部：圆环倒计时 + 红色警告徽章与标题
    auto *top = new QHBoxLayout;
    top->setSpacing(18);
    m_ring = new CountdownRing(this);
    top->addWidget(m_ring, 0, Qt::AlignTop);

    auto *tl = new QVBoxLayout;
    tl->setSpacing(6);
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    auto *badge = new DLabel(this);
    badge->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(24, 24));
    titleRow->addWidget(badge, 0, Qt::AlignVCenter);
    auto *t = new DLabel(tr("即将完全关闭磐石保护"), this);
    t->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(t, DFontSizeManager::T3);
    titleRow->addWidget(t, 0, Qt::AlignVCenter);
    titleRow->addStretch();
    tl->addLayout(titleRow);

    auto *sub = new DLabel(tr("此为高风险操作，请务必确认后再继续"), this);
    sub->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(sub, DFontSizeManager::T6);
    tl->addWidget(sub);
    top->addLayout(tl, 1);
    lay->addLayout(top);

    // 风险清单
    auto *box = new QFrame(this);
    box->setObjectName(QStringLiteral("panshiWarnBox"));
    auto *bl = new QVBoxLayout(box);
    bl->setContentsMargins(16, 12, 16, 12);
    bl->setSpacing(8);
    const QStringList risks = {
        tr("核心目录(/usr 等)将失去只读保护"),
        tr("恶意软件与误操作可直接破坏系统"),
        tr("重启后仍保持关闭，需手动重新开启保护"),
    };
    for (const QString &r : risks) {
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        auto *ic = new DLabel(box);
        ic->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(16, 16));
        row->addWidget(ic, 0, Qt::AlignTop);
        auto *l = new DLabel(r, box);
        l->setObjectName(QStringLiteral("panshiWarnText"));
        l->setWordWrap(true);
        DFontSizeManager::instance()->bind(l, DFontSizeManager::T6);
        row->addWidget(l, 1);
        bl->addLayout(row);
    }
    lay->addWidget(box);

    lay->addStretch();

    // 底部按钮：取消 | 确认（倒计时期间禁用）
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    auto *btnCancel = new DPushButton(tr("取消"), this);
    connect(btnCancel, &DPushButton::clicked, this, &PanshiCountdownDialog::reject);
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);

    m_btnConfirm = new DWarningButton(this);
    m_btnConfirm->setEnabled(false);
    m_btnConfirm->setText(tr("我已知晓风险，完全关闭（%1s）").arg(m_remain));
    connect(m_btnConfirm, &DPushButton::clicked, this, &PanshiCountdownDialog::accept);
    btnRow->addWidget(m_btnConfirm);
    lay->addLayout(btnRow);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &PanshiCountdownDialog::onTick);
}

void PanshiCountdownDialog::showEvent(QShowEvent *event)
{
    DAbstractDialog::showEvent(event);
    PanshiUi::disableDialogTransparency(this);   // 关闭 DTK 弹窗默认的模糊/透明效果
    // 居中于父窗口
    if (parentWidget()) {
        const QRect g = parentWidget()->window()->geometry();
        move(g.center() - rect().center());
    }
    if (!m_timer->isActive())
        m_timer->start();
}

void PanshiCountdownDialog::onTick()
{
    m_remain--;
    m_ring->setRemain(m_remain);
    if (m_remain <= 0) {
        m_timer->stop();
        m_btnConfirm->setEnabled(true);
        m_btnConfirm->setText(tr("我已知晓风险，完全关闭"));
    } else {
        m_btnConfirm->setText(tr("我已知晓风险，完全关闭（%1s）").arg(m_remain));
    }
}
