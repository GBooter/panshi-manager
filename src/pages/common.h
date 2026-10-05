// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <DFrame>
#include <DLabel>
#include <DFontSizeManager>
#include <DIconButton>
#include <DPushButton>
#include <DSuggestButton>
#include <DWarningButton>
#include <DIconTheme>
#include <DPalette>
#include <DGuiApplicationHelper>

#include "ui/components.h"
#include "ui/confirmdialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QScrollArea>
#include <QIcon>
#include <QStyle>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

// ======================================================================
// 图标（UI 重构方案 §2.2）：一律走 DIconTheme 主题图标，
// 不再内嵌 SVG（原 15 个手写 SVG 与 IconBadge 已删除）。
// ======================================================================
namespace Ui {

/** 主题图标，找不到时回落到 Qt 标准图标，保证任何主题下可见 */
inline QIcon themedIcon(const QString &name, QStyle::StandardPixmap fallback = QStyle::SP_DriveHDIcon)
{
    const QIcon ic = DIconTheme::findQIcon(name);
    return ic.isNull() ? QApplication::style()->standardIcon(fallback) : ic;
}

/**
 * 主题图标徽章：仅渲染图标本身，不再自绘底色
 * （底色由所在卡片/行的 DPalette 决定，颜色跟随主题）
 *
 * ⚠️ 主题切换必须重查图标（2026-10-03 用户反馈「图标也没法看见」）：
 *   原来构造时直接 `setPixmap(icon.pixmap(isz))` —— **pixmap 是快照**，
 *   QIcon 后续怎么换主题都不会影响已画进 label 的这张图，于是切暗色后
 *   页面里的大图标仍是亮色主题那一版（浅色图标在深底上几乎看不见）。
 *   DIconTheme 官方文档「主题切换时的图标更新」也要求手动重查。
 *   → 记下图标名 + 尺寸，挂 `applicationPaletteChanged` 重设 pixmap。
 */
inline DLabel *iconBadge(const QString &iconKey, int size = 44)
{
    auto *l = new DLabel;
    l->setFixedSize(size, size);
    l->setAlignment(Qt::AlignCenter);
    const int isz = qMax(12, int(size * 0.62));
    l->setPixmap(themedIcon(iconKey).pixmap(isz, isz));
    // 主题切换后重查：新主题的图标资源可能不同（尤其单色图标会反色）
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::applicationPaletteChanged,
                     l, [l, iconKey, isz] {
        l->setPixmap(themedIcon(iconKey).pixmap(isz, isz));
    });
    return l;
}

// ======================================================================
// 统一确认对话框（UI 重构方案 §4.3）：全应用唯一的确认实现。
//
// 阶段 4 起，**操作结果反馈不再走模态对话框** —— 原先的
// Ui::infoDialog / Ui::errorDialog 已删除，改由页面内的
// PanshiUi::PanshiInlineBanner 承接（见 BasePage::setFeedbackBanner /
// showFeedback）。理由：模态框要求用户再点一次「确定」才能继续，
// 且会盖住刚被改动的界面；页内 Banner 让反馈与操作对象同屏可见。
// ======================================================================

/**
 * 确认对话框（UI 重构方案 §4.3「全应用只有 1 个确认组件」）。
 * 实现委托给 PanshiUi::askConfirm / askDanger（src/ui/confirmdialog.cpp），
 * 本函数只是既有调用点（update/snapshots/protection/deploys）的薄封装，
 * 不再持有自己的 DDialog 构造逻辑。
 * danger=true 时确认按钮为警告色（危险操作），默认焦点在「取消」。
 *
 * 需要"说明 + 后果清单 + 单选选项"的复杂决策，请直接构造
 * PanshiUi::ConfirmRequest 并调用 PanshiUi::askConfirm（见 masterswitch.cpp）。
 */
inline bool confirmDialog(QWidget *parent, const QString &title, const QString &text,
                          const QString &okText = QString(), bool danger = false)
{
    const QString confirmText = okText.isEmpty() ? QObject::tr("确定") : okText;
    if (danger)
        return PanshiUi::askDanger(parent, title, text, confirmText);
    return PanshiUi::askConfirm(parent, title, text, confirmText);
}

// ======================================================================
// 布局辅助组件
// ======================================================================

/** 创建自适应滚动容器: 窗口尺寸不足时内容可滚动, 避免底部裁剪 */
inline QScrollArea *makeScrollArea(QWidget *parent = nullptr)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setObjectName(QStringLiteral("panshiScroll"));
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

/** 页面标题 + 副标题（可选图标） */
inline QWidget *pageHeader(const QString &title, const QString &subtitle,
                           const QString &iconName = QString())
{
    auto *w = new QWidget;
    auto *row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    if (!iconName.isEmpty())
        row->addWidget(iconBadge(iconName, 48));
    auto *tl = new QVBoxLayout;
    tl->setSpacing(2);
    auto *t = new DLabel(title);
    t->setObjectName(QStringLiteral("panshiPageTitle"));
    t->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(t, DFontSizeManager::T3);
    tl->addWidget(t);
    if (!subtitle.isEmpty()) {
        auto *s = new DLabel(subtitle);
        s->setObjectName(QStringLiteral("panshiPageSubtitle"));
        s->setForegroundRole(DPalette::TextTips);
        DFontSizeManager::instance()->bind(s, DFontSizeManager::T6);
        tl->addWidget(s);
    }
    row->addLayout(tl);
    row->addStretch();
    return w;
}

/** 白色圆角卡片 */
inline DFrame *card(QWidget *parent = nullptr)
{
    auto *c = new DFrame(parent);
    c->setObjectName(QStringLiteral("panshiCard"));
    c->setLineWidth(0);
    return c;
}

inline QVBoxLayout *cardLayout(DFrame *c)
{
    auto *lay = new QVBoxLayout(c);
    lay->setContentsMargins(22, 20, 22, 20);
    lay->setSpacing(12);
    return lay;
}

/** 卡片标题（可选带图标） */
inline void cardTitle(DFrame *c, const QString &title, const QString &desc = QString(),
                      const QString &iconName = QString())
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    if (!iconName.isEmpty())
        row->addWidget(iconBadge(iconName, 40));
    auto *tl = new QVBoxLayout;
    tl->setSpacing(3);
    auto *t = new DLabel(title);
    t->setObjectName(QStringLiteral("panshiCardTitle"));
    t->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(t, DFontSizeManager::T5);
    tl->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = new DLabel(desc);
        d->setObjectName(QStringLiteral("panshiCardDesc"));
        d->setForegroundRole(DPalette::TextTips);
        d->setWordWrap(true);
        DFontSizeManager::instance()->bind(d, DFontSizeManager::T6);
        tl->addWidget(d);
    }
    row->addLayout(tl);
    row->addStretch();
    qobject_cast<QVBoxLayout *>(c->layout())->addLayout(row);
}

/** 通用按钮 */
inline DPushButton *button(const QString &text, bool suggest = false, bool warning = false)
{
    DPushButton *b = nullptr;
    if (suggest)
        b = new DSuggestButton(text);
    else if (warning) {
        b = new DWarningButton;
        b->setText(text);
    } else
        b = new DPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

/** 行标签（设置项标题） */
inline DLabel *rowLabel(const QString &text)
{
    auto *l = new DLabel(text);
    l->setObjectName(QStringLiteral("panshiRowLabel"));
    l->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(l, DFontSizeManager::T5);
    return l;
}

/** 行描述 */
inline DLabel *rowDesc(const QString &text)
{
    auto *l = new DLabel(text);
    l->setObjectName(QStringLiteral("panshiRowDesc"));
    l->setForegroundRole(DPalette::TextTips);
    l->setWordWrap(true);
    DFontSizeManager::instance()->bind(l, DFontSizeManager::T6);
    return l;
}

/** 图例色点（图表图例；存语义档位，绘制时实时取色，自动跟随主题切换） */
class LegendDot : public QWidget
{
public:
    LegendDot(PanshiUi::Semantic s, QWidget *parent = nullptr)
        : QWidget(parent), m_sem(s)
    {
        setFixedSize(10, 10);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(PanshiUi::semanticColor(m_sem));
        p.drawEllipse(rect());
    }
private:
    PanshiUi::Semantic m_sem;
};

inline QWidget *legendDot(PanshiUi::Semantic s, const QString &text)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    h->addWidget(new LegendDot(s, w));
    auto *l = new DLabel(text);
    l->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(l, DFontSizeManager::T7);
    h->addWidget(l);
    return w;
}

/** 图例虚线（趋势线说明） */
inline QWidget *legendLine(const QString &text)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto *dash = new DLabel(QStringLiteral("— —"));
    dash->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(dash, DFontSizeManager::T7);
    h->addWidget(dash);
    auto *l = new DLabel(text);
    l->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(l, DFontSizeManager::T7);
    h->addWidget(l);
    return w;
}

/**
 * 中部省略标签：宽度不足时把文本从中间截断（保留首尾），不会撑破所在布局。
 *
 * 用途：需要「可被压缩的文本」处——表格弹性列、信息行右侧的值。
 * 弹性列的实际宽度由其它列决定，内容随时可能超出；`setWordWrap(false)`
 * 只能禁止折行，无法阻止横向撑开。完整文本请由调用方 `setToolTip()` 提供。
 *
 * 与 aboutpage.cpp 的 `ElidedCommandLabel` 同源（该处为保证等宽命令不折行而写），
 * 阶段 3-6 提升到这里并补齐：强制不折行 + 默认垂直居中，供跨页复用。
 *
 * **阶段 3-8 补 `minimumSizeHint()`**：原先只覆盖 paintEvent，自身仍按完整文本
 * 索要宽度——`QHeaderView` 列的宽度由我们显式设定，故表格场景看不出问题；
 * 但在 `QHBoxLayout` 里（信息行的值）它会撑破行，把整页最小宽顶上去，
 * 最终被外层 ScrollBarAlwaysOff 静默裁剪。必须让最小宽回到「可忽略」。
 *
 * **阶段 3-8 二轮修正**：横向策略不能用 Ignored —— Ignored 在布局分配中
 * sizeHint 记 0，与带 stretch 的兄弟项（PanshiRow 左文本列 stretch=1）同处
 * 一行时，富余宽度全被文本列吸走，本控件被分到 0 宽、什么都不画
 * （总览页「系统信息」四行值整体消失）。改回 Preferred：minimumSizeHint(24)
 * 已保证最小宽可忽略、窄时可压窄省略；宽裕时按自然宽参与分配、完整显示。
 */
class ElidedLabel : public DLabel
{
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr)
        : DLabel(text, parent)
    {
        setWordWrap(false);
        setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        // 横向 Preferred 而非 Ignored：Ignored 的 sizeHint 在布局分配中记 0，
        // 会被带 stretch 的兄弟项挤到 0 宽（见类注释「二轮修正」）；
        // 最小宽已由 minimumSizeHint() 压到 24，Preferred 不会撑破行
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    QSize sizeHint() const override
    {
        // 首选宽仍取文本自然宽（宽裕时完整显示），但不参与"必须满足"的协商
        return QSize(fontMetrics().horizontalAdvance(text()) + 2,
                     DLabel::sizeHint().height());
    }

    QSize minimumSizeHint() const override
    {
        // 最小宽给一个很小的常数：省略号本身也需要一点空间
        return QSize(24, DLabel::minimumSizeHint().height());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        p.setFont(font());
        const QString shown = fontMetrics().elidedText(text(), Qt::ElideMiddle, width());
        p.drawText(rect(), static_cast<int>(alignment()), shown);
    }
};

/**
 * 行内复制按钮：优先 edit-copy 主题图标按钮，图标缺失时退化为文字按钮。
 * 复制成功给 2 秒微反馈（对勾图标 / 「已复制」文字），不再弹模态窗。
 *
 * 用 QTimer::singleShot(…, btn, …) 带上下文参数：定时器绑定在 btn 上，
 * 2 秒内对话框被关掉时自动取消，不会打在已析构的对象上。
 */
inline QAbstractButton *makeCopyButton(const QString &payload, QWidget *parent)
{
    const QIcon ic = DIconTheme::findQIcon(QStringLiteral("edit-copy"));
    QAbstractButton *btn = nullptr;
    if (!ic.isNull()) {
        auto *ib = new DIconButton(parent);
        ib->setIcon(ic);
        ib->setIconSize(QSize(PanshiUi::Size::IconSmall, PanshiUi::Size::IconSmall));
        ib->setToolTip(QObject::tr("复制"));
        ib->setAccessibleName(QObject::tr("复制命令"));
        btn = ib;
    } else {
        auto *tb = new DPushButton(QObject::tr("复制"), parent);
        tb->setToolTip(QObject::tr("复制"));
        btn = tb;
    }
    btn->setCursor(Qt::PointingHandCursor);

    QObject::connect(btn, &QAbstractButton::clicked, btn, [btn, payload] {
        QApplication::clipboard()->setText(payload);
        if (auto *ib = qobject_cast<DIconButton *>(btn)) {
            const QIcon ok = DIconTheme::findQIcon(QStringLiteral("object-select"));
            if (!ok.isNull())
                ib->setIcon(ok);
        } else {
            btn->setText(QObject::tr("已复制"));
        }
        QTimer::singleShot(2000, btn, [btn] {
            if (auto *ib = qobject_cast<DIconButton *>(btn)) {
                const QIcon back = DIconTheme::findQIcon(QStringLiteral("edit-copy"));
                if (!back.isNull())
                    ib->setIcon(back);
            } else {
                btn->setText(QObject::tr("复制"));
            }
        });
    });
    return btn;
}

/**
 * 命令行：等宽命令 + 小字说明 + 行尾复制按钮。
 *
 * 命令标签用 ElidedLabel（中部省略、不折行、垂直居中）；完整命令始终
 * 可用悬停提示查看，行尾复制按钮复制的也是全文。命令是自由长度文本，
 * 绝不能让它决定行宽（否则长命令会把整行撑爆），故它占弹性位、
 * 说明与按钮固定不参与压缩。
 *
 * 放在 common.h 而非某个页面里：关于对话框与关于页都要用它，
 * 两处各写一份必然分叉（文案与复制行为会走偏）。
 */
inline QWidget *makeCommandRow(const QString &cmd, const QString &comment,
                               QWidget *parent = nullptr)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(PanshiUi::Space::M);

    auto *c = new ElidedLabel(cmd, w);
    c->setToolTip(cmd);
    DFontSizeManager::instance()->bind(c, DFontSizeManager::T8);
    PanshiUi::applySemantic(c, PanshiUi::Semantic::Normal);
    h->addWidget(c, 1);   // 弹性占位：窄窗口时命令中部省略，说明与按钮始终可见
    h->addWidget(PanshiUi::makeLabel(comment, PanshiUi::TextLevel::Aux,
                                     PanshiUi::Semantic::Muted),
                 0, Qt::AlignVCenter);
    h->addWidget(makeCopyButton(cmd, w));
    return w;
}

} // namespace Ui
