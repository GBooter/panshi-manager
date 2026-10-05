// SPDX-License-Identifier: GPL-3.0-or-later

#include "confirmdialog.h"

#include "../widgets/countdowndialog.h"

#include <DDialog>
#include <DRadioButton>

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QList>
#include <QVBoxLayout>

namespace PanshiUi {

namespace {

QString iconNameFor(DialogLevel level)
{
    return level == DialogLevel::Normal ? QStringLiteral("dialog-question")
                                        : QStringLiteral("dialog-warning");
}

QStyle::StandardPixmap fallbackFor(DialogLevel level)
{
    return level == DialogLevel::Normal ? QStyle::SP_MessageBoxQuestion
                                        : QStyle::SP_MessageBoxWarning;
}

} // namespace

bool askConfirm(QWidget *parent, const ConfirmRequest &req)
{
    // 不可逆高危操作：保留倒计时安全等待（方案 §5.2 注意 11）
    if (req.countdownSeconds > 0) {
        PanshiCountdownDialog dlg(parent);
        return dlg.exec() == QDialog::Accepted;
    }

    const bool danger = (req.level != DialogLevel::Normal);

    DDialog dlg(parent);
    dlg.setTitle(req.title);
    dlg.setIcon(themedIcon(iconNameFor(req.level), fallbackFor(req.level)));
    if (!req.message.isEmpty())
        dlg.setMessage(req.message);

    QList<DRadioButton *> radios;
    bool hasContent = false;
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::S);

    // 摘要块：受操作对象的关键信息
    if (!req.facts.isEmpty()) {
        auto *box = new PanshiPanel(nullptr, PanshiPanel::Level::Subtle);
        auto *bl = box->body();
        bl->setContentsMargins(Space::M, Space::S, Space::M, Space::S);
        bl->setSpacing(Space::XS);
        for (const QString &line : req.facts)
            bl->addWidget(makeLabel(line, TextLevel::Desc, Semantic::Normal));
        lay->addWidget(box);
        hasContent = true;
    }

    // 后果清单：逐条列出，用警告色
    if (!req.consequences.isEmpty()) {
        for (const QString &c : req.consequences)
            lay->addWidget(makeLabel(QStringLiteral("· ") + c, TextLevel::Desc,
                                     Semantic::Warning));
        hasContent = true;
    }

    // 高级选项：默认收起语义由调用方控制（此处平铺，仅用于还原策略等少量场景）
    if (!req.options.isEmpty()) {
        auto *opts = new QWidget(content);
        auto *ol = new QVBoxLayout(opts);
        ol->setContentsMargins(0, 0, 0, 0);
        ol->setSpacing(Space::XS);
        for (int i = 0; i < req.options.size(); ++i) {
            auto *rb = new DRadioButton(req.options.at(i), opts);
            rb->setChecked(i == req.optionDefault);
            ol->addWidget(rb);
            radios.append(rb);
        }
        lay->addWidget(opts);
        hasContent = true;
    }

    if (hasContent)
        dlg.addContent(content);
    else
        delete content;

    dlg.addButton(req.cancelText.isEmpty() ? QObject::tr("取消") : req.cancelText,
                  false, DDialog::ButtonNormal);
    dlg.addButton(req.confirmText.isEmpty() ? QObject::tr("确定") : req.confirmText,
                  false, danger ? DDialog::ButtonWarning : DDialog::ButtonRecommend);

    // 默认焦点落在「取消」：DDialog 内按钮按加入顺序创建，首个即取消
    const QList<QAbstractButton *> buttons = dlg.findChildren<QAbstractButton *>();
    if (!buttons.isEmpty())
        buttons.first()->setFocus();

    if (dlg.exec() != 1)
        return false;

    if (req.optionResult) {
        for (int i = 0; i < radios.size(); ++i) {
            if (radios.at(i)->isChecked()) {
                *req.optionResult = i;
                break;
            }
        }
    }
    return true;
}

bool askConfirm(QWidget *parent, const QString &title, const QString &message,
                const QString &confirmText)
{
    ConfirmRequest req;
    req.title = title;
    req.message = message;
    req.confirmText = confirmText;
    req.level = DialogLevel::Normal;
    return askConfirm(parent, req);
}

bool askDanger(QWidget *parent, const QString &title, const QString &message,
               const QString &confirmText, const QStringList &consequences)
{
    ConfirmRequest req;
    req.title = title;
    req.message = message;
    req.confirmText = confirmText;
    req.consequences = consequences;
    req.level = DialogLevel::Warning;
    return askConfirm(parent, req);
}

QWidget *createDetailsBox(const QString &title, const QString &rawText, QWidget *parent)
{
    auto *box = new QWidget(parent);
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::XS);

    auto *toggle = new DPushButton(QStringLiteral("▸ ") + title);
    toggle->setFlat(true);
    toggle->setCursor(Qt::PointingHandCursor);
    lay->addWidget(toggle, 0, Qt::AlignLeft);

    auto *detail = makeLabel(rawText, TextLevel::Mono, Semantic::Muted);
    detail->setWordWrap(true);
    detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detail->hide();
    lay->addWidget(detail);

    QObject::connect(toggle, &DPushButton::clicked, box, [toggle, detail, title] {
        const bool shown = !detail->isVisible();
        detail->setVisible(shown);
        const QString mark = shown ? QStringLiteral("▾ ") : QStringLiteral("▸ ");
        toggle->setText(mark + title);
    });

    return box;
}

} // namespace PanshiUi
