// SPDX-License-Identifier: GPL-3.0-or-later
//
// 自绘「关于」对话框
//
// 为什么不用 DAboutDialog（2026-10-04）：
//   DApplication 会自动挂一个 DAboutDialog，深in 桌面环境菜单里的「关于」调的就是它。
//   那个对话框的布局是**写死在 DTK 内部**的：左侧一个居中的大图标区、右侧一列
//   「版本/主页/描述/致谢/协议」的 label:value 竖排 —— 全部左对齐、无卡片分组、
//   字号权重全部相同、留白极大。用户原话「现在这个看着好丑」，确实如此。
//   而 DAboutDialog 的这些子部件**没有任何对外接口**可改样式，接管不了。
//   → 只能自绘，并用 DApplication::setAboutDialog() 把入口换掉。
//
// 设计要点（对齐项目既有的 UI 重构约定，见 docs/UI-重构进度-阶段5）：
//   · 层次靠**色差 + 圆角**，不描边不阴影（同 components.h 的原则）
//   · 颜色一律走 DPalette / PanshiUi::Semantic，不写死 hex
//   · 标题区用产品图标 + 产品名 + 版本（版本是用户最常问的信息，给足权重）
//   · 元信息分卡：作者 / 主页 / 协议，各自成组，避免 label:value 流水账
//   · 主页可点击（DTK 里是纯文本，点了没反应）
//   · 底部「技术参考 / 常用命令」复用 AboutPage 的两组折叠内容，不重复实现
//
#include "ui/aboutdialog.h"
#include "pages/common.h"
#include "ui/components.h"

#include <DApplication>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DIconTheme>
#include <DLabel>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace PanshiUi {

namespace {

/**
 * 应用图标。
 *
 * 优先走 DIconTheme（安装后 `panshi.svg` 落在系统图标目录，
 * 由图标主题接管，明暗两套都能自动适配）。
 * ⚠️ 图标 pixmap 是**构造那一刻的快照**，主题切换必须重查 —— 见 showEvent
 *    里挂的 applicationPaletteChanged。
 * DIconTheme 找不到时回落到项目自带的 data/icons/panshi.svg：
 * 开发期与未安装场景（图标还没进系统目录）下不至于空着一块。
 */
QIcon productIcon()
{
    QIcon ic = DIconTheme::findQIcon(QStringLiteral("panshi"));
    if (!ic.isNull())
        return ic;
    static const QString kFallback = QStringLiteral(PANSHI_ICON_SVG);
    if (QFileInfo::exists(kFallback))
        return QIcon(kFallback);
    return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
}

QString appVersion()
{
    const QString v = QApplication::applicationVersion();
    return v.isEmpty() ? QStringLiteral("1.0.0") : v;
}

/**
 * 简介行：一句定位 + 平台/技术栈。
 * 与旧版区别：拆成两行、去掉冗长的「采用 DDE 桌面风格(DTK)」这类
 * 对用户无意义的话（那是给开发者看的），只留用户会关心的「支持什么系统」。
 */
QString taglineText()
{
    return QObject::tr("Deepin 25 磐石（immutable）系统管理工具");
}

/**
 * 平台说明。
 *
 * 保持**一行放得下**：原先的「支持 Deepin 25（X11 / Wayland）· 基于 OSTree 快照与只读保护」
 * 在默认宽度下会折行，而行高被卡片压缩 → 第二行被裁（用户截图）。
 * 现在拆掉冗余的「支持」「基于」二字，语义不减但能一行排完。
 */
QString platformText()
{
    return QObject::tr("Deepin 25（X11 / Wayland）· OSTree 快照与只读保护");
}

/**
/**
 * 换行标签的高度补正器。
 *
 * ⚠️ 为什么需要它（Qt 的一个已知坑）：`QLabel` 开了 `wordWrap` 后，
 * **`sizeHint()` 不会随实际宽度重算**，Qt 只在 `heightForWidth()` 里给出换行后
 * 的真实高度。于是「平台」那行按单行高度排布，第二行被**裁掉**
 * （用户截图里就是缺了一截）。这里监听 Resize，用 `heightForWidth()` 把下限补上。
 *
 * 为什么不在构建时一次性算好：当时宽度还没定，且用户可拖拽改变宽度，
 * 换行位置会变 —— 只能在宽度变化时重算。
 */
class WrapHeightFixer : public QObject
{
public:
    explicit WrapHeightFixer(QWidget *target) : QObject(target), m_target(target) {}

    /**
     * 按当前宽度补足换行标签的高度下限。
     *
     * 单独做成静态函数，是因为**只在 Resize 事件里补不够**：
     * 首次 Resize 触发时布局尚未稳定（宽度还是中间值），算出的需求偏小，
     * 之后再无 Resize 事件 → 下限永远停在错误的低值（实测 minH=0 而 hfw=42）。
     * 对话框在最终 resize 之后会**显式再调一次**本函数兜底。
     */
    static void apply(QWidget *w)
    {
        if (!w)
            return;
        auto *lab = qobject_cast<QLabel *>(w);
        if (lab && lab->wordWrap()) {
            const int need = lab->heightForWidth(lab->width());
            if (need > lab->minimumHeight())
                lab->setMinimumHeight(need);
        }
        for (QWidget *c : w->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
            apply(c);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *e) override
    {
        if (watched == m_target
            && (e->type() == QEvent::Resize || e->type() == QEvent::Show
                || e->type() == QEvent::LayoutRequest)) {
            apply(m_target);
        }
        return QObject::eventFilter(watched, e);
    }

private:
    QWidget *m_target;
};

} // namespace

// ---- LinkRow ----

LinkRow::LinkRow(const QString &label, const QString &url, QWidget *parent)
    : QWidget(parent)
    , m_url(url)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(Size::RowMinHeight);

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(Space::M, Space::XS, Space::M, Space::XS);
    l->setSpacing(Space::M);

    m_title = makeLabel(label, TextLevel::Body, Semantic::Muted);
    m_title->setMinimumWidth(56);
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    l->addWidget(m_title, 0);

    m_value = new DLabel(this);
    m_value->setText(url);
    m_value->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_value->setOpenExternalLinks(true);
    m_value->setCursor(Qt::PointingHandCursor);
    m_value->setWordWrap(true);
    m_value->setAccessibleName(QObject::tr("%1：%2").arg(label, url));
    l->addWidget(m_value, 1);

    setAccessibleName(QObject::tr("%1：%2").arg(label, url));
}

/**
 * 普通信息行：标签 + 值。值缺省时才用占位「—」。
 * 与旧版差别：标签定宽（56px）→ 所有行的值左边缘对齐，形成整齐的竖线；
 * 旧版是「标签在上、值在下」，五组堆下来视觉上散成一片。
 */
QWidget *makeInfoRow(const QString &label, const QString &value, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(Space::M, Space::XS, Space::M, Space::XS);
    l->setSpacing(Space::M);

    auto *t = makeLabel(label, TextLevel::Body, Semantic::Muted);
    t->setMinimumWidth(56);
    t->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    l->addWidget(t, 0);

    auto *v = makeLabel(value.isEmpty() ? QObject::tr("—") : value,
                        TextLevel::Body, Semantic::Normal);
    // 保留 wordWrap：用户把窗口拖窄时不至于把文字直接截断。
    // 默认宽度下（kAboutMinWidth=600）所有信息行都能一行排完，不触发换行；
    // 万一真换行了，showEvent 末尾会显式调 WrapHeightFixer::apply 补足高度。
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextBrowserInteraction);
    v->setOpenExternalLinks(true);
    l->addWidget(v, 1);

    // ⚠️ 这里**试过**给行设 `QSizePolicy::Fixed` + `setMinimumHeight(sizeHint())`
    // 来强制不被压缩（当时行被卡片从 92px 压到 35px，第二行文字被裁）。
    // 实测**行高确实补上了，但行与行之间会重叠** —— 卡片自身的高度是按各行
    // 原始 sizeHint 算的，强行拉高某些行却不拉高卡片，布局就错位了。
    // 属于治标不治本，已回退。
    // 真正的解法是**让文字本来就放得下**：对话框给足默认宽度（kAboutMinWidth）
    // + 精简平台说明文案（见 platformText 的注释），从而不触发换行。
    // WrapHeightFixer 只作为「用户手动拖窄窗口」时的兜底。

    w->setAccessibleName(QObject::tr("%1：%2").arg(label, value));
    return w;
}

void LinkRow::retheme()
{
    const QColor c = semanticColor(Semantic::Accent);
    m_value->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(c.name()));
    update();
}

void LinkRow::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);
    m_hover = true;
    update();
}

void LinkRow::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    m_hover = false;
    update();
}

void LinkRow::mouseReleaseEvent(QMouseEvent *e)
{
    // 只响应左键：右键/中键不该触发跳转
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
        QDesktopServices::openUrl(QUrl(m_url));
}

void LinkRow::paintEvent(QPaintEvent *)
{
    if (!m_hover)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(softFill(Semantic::Accent, 24));   // 极淡的强调底，仅作「可点」提示
    p.drawRoundedRect(rect(), Radius::Element, Radius::Element);
}

// ---- PanshiAboutDialog ----

PanshiAboutDialog::PanshiAboutDialog(QWidget *parent)
    : DAboutDialog(parent)
{
    setWindowTitle(tr("关于磐石设置"));
    // 内容在 showEvent 里建：必须等基类 showEvent 把自己的内容摆好之后，
    // 才知道该隐藏哪些（见 showEvent 的注释）。
}

/**
 * 隐藏 DAboutDialog 自带的内容，改为显示自绘布局。
 *
 * ⚠️ 关键教训：最初这里是**删除**基类内容（setParent(nullptr)+deleteLater），
 *    结果探针一 show 就 segfault。原因是 DAboutDialog 的内容控件指针被
 *    记在 DTK 私有的 DAboutDialogPrivate 里，界面显示期间随时可能被访问 ——
 *    控件没了，指针就是悬垂的。
 *    正确做法是 **setVisible(false)**：指针仍然有效，DTK 自己照常访问，
 *    只是不显示。标题栏由 DDialog 走窗口装饰绘制，不在这些子控件里，
 *    隐藏后仍能看到标题与关闭按钮。
 *
 * 幂等：showEvent 每次显示都会触发（隐藏的控件可能被基类再次 setVisible(true)，
 * 所以这里每次都重跑一遍），而自绘内容只在第一次构建。
 */
void PanshiAboutDialog::showEvent(QShowEvent *event)
{
    DAboutDialog::showEvent(event);   // 先让基类摆好它自己的内容

    // 只隐藏 DAboutDialog 自带的**内容区**，**保留 DTitlebar**。
    //
    // ⚠️ 这里踩过一次坑：原先是「隐藏全部直接子控件」，结果 DAboutDialog 有
    // **两个**直接子控件 —— 内容区 + `Dtk::Widget::DTitlebar`（内含
    // `DTitlebarDWindowCloseButton` 关闭按钮、标题文字、菜单）。一并藏掉的后果：
    //   ① 右上角关闭按钮消失（用户反馈「标准菜单按钮都没了」）；
    //   ② DDialog 自身背景是**半透明**的（圆角+阴影设计），内容区一藏，
    //      壁纸就透过对话框（用户反馈「透明度太高」）。
    // → 按类名跳过 DTitlebar，其余（内容区）才隐藏。
    const QList<QWidget *> inherited =
        findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : inherited) {
        if (w->metaObject()->className() == QLatin1String("Dtk::Widget::DTitlebar"))
            continue;   // 保留标题栏：它提供关闭按钮与窗口标题
        w->setVisible(false);
    }

    if (!m_body) {
        m_body = buildBody();
        layout()->addWidget(m_body);   // DAboutDialog 的顶层布局（标题栏之下）
        layout()->setContentsMargins(0, 0, 0, 0);
        connect(DGuiApplicationHelper::instance(),
                &DGuiApplicationHelper::applicationPaletteChanged,
                this, [this] {
            if (m_icon)
                m_icon->setPixmap(productIcon().pixmap(64, 64));
            retheme();
        });
        setMinimumWidth(kAboutMinWidth);
    }
    m_body->setVisible(true);

    // ---- 定宽 → 再量高（顺序不能反）----
    //
    // ⚠️ 两条独立的教训：
    //
    // ① 必须用 setMinimumHeight 而不只是 resize()。实测：body 的 sizeHint 是 480，
    //    但对话框只给到 270，信息卡被压到 65px、行文字全叠在一起 ——
    //    可用高度不足时布局会**优先压缩可压缩的子项**。resize() 只是请求值，
    //    随后被布局压回；setMinimumHeight 才是硬地板。
    //
    // ② **必须先定宽再量高**。「平台」那行文字较长会换行，宽度一变换行位置就变、
    //    所需高度也跟着变。先量高会把「按旧宽度算出的高度」定死，
    //    结果第二行被裁掉（用户截图里「平台」行就是这样缺了一截）。
    const int w = qMax(width(), kAboutMinWidth);
    resize(w, height());
    if (auto *lay = m_body->layout())
        lay->activate();          // 按新宽度重排
    m_body->updateGeometry();     // 作废缓存的 sizeHint
    const int contentH = m_body->sizeHint().height();
    setMinimumHeight(contentH);

    // 换行标签补高：必须在宽度定死、内容区也排好之后做（见 WrapHeightFixer::apply
    // 的注释 —— 只靠 Resize 事件会停在错误的低值）。放最后是因为它可能让高度再涨。
    WrapHeightFixer::apply(m_body);
    m_body->updateGeometry();
    setMinimumHeight(m_body->sizeHint().height());
    resize(w, m_body->sizeHint().height());
    retheme();
}

QWidget *PanshiAboutDialog::buildBody()
{
    auto *body = new QWidget(this);
    body->setObjectName(QStringLiteral("panshiAboutBody"));
    // 不透明底色：DDialog 自身背景是半透明的（圆角 + 阴影靠它），
    // 隐藏原内容区后若不补底，桌面壁纸会整片透进来。
    // autoFillBackground 用 `palette().color(QPalette::Window)`，
    // 而 body 的调色板继承自对话框 → 切暗色时 Qt 会发 PaletteChange 自动重绘，
    // 不必手动刷（手动 setPalette 反而会在切主题后留下一块旧色）。
    body->setAutoFillBackground(true);
    auto *root = new QVBoxLayout(body);
    root->setContentsMargins(Space::XL, Space::L, Space::XL, Space::XL);
    root->setSpacing(Space::L);

    // ---- ① 标题区：图标 + 产品名 + 版本 ----
    // 版式意图：把「这是什么、什么版本」这两条最常被问到的信息放在视觉首位，
    // 用大字号 + 粗体建立层级；旧版把它们埋在右侧一列普通文字里。
    auto *head = new QWidget(body);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(Space::L);

    m_icon = new DLabel(head);
    m_icon->setFixedSize(64, 64);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setPixmap(productIcon().pixmap(64, 64));
    m_icon->setAccessibleName(tr("应用图标"));
    hl->addWidget(m_icon, 0, Qt::AlignTop);

    auto *headText = new QWidget(head);
    auto *htl = new QVBoxLayout(headText);
    htl->setContentsMargins(0, 0, 0, 0);
    htl->setSpacing(Space::XS);

    auto *title = new DLabel(tr("磐石设置"), headText);
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    DFontSizeManager::instance()->bind(title, DFontSizeManager::T2);
    title->setAccessibleName(tr("应用名称"));
    htl->addWidget(title);

    // 版本标签是局部变量而非成员：它是纯文本，主题切换时 DLabel 自身
    // 会跟随调色板重绘，无需像图标那样手动重查。
    auto *version = makeLabel(tr("版本 %1").arg(appVersion()),
                              TextLevel::Desc, Semantic::Accent);
    htl->addWidget(version);

    auto *tag = makeLabel(taglineText(), TextLevel::Desc, Semantic::Muted);
    tag->setWordWrap(true);
    htl->addWidget(tag);

    hl->addWidget(headText, 1);
    root->addWidget(head);

    // ---- ② 信息卡：作者 / 平台 / 致谢 / 主页 / 协议 ----
    auto *card = new PanshiPanel(body, PanshiPanel::Level::Surface);
    auto *cl = card->body();
    cl->setSpacing(0);

    cl->addWidget(makeInfoRow(tr("作者"), QStringLiteral("Feelup"), card));
    cl->addWidget(new PanshiSeparator(card));
    cl->addWidget(makeInfoRow(tr("平台"), platformText(), card));
    cl->addWidget(new PanshiSeparator(card));
    cl->addWidget(makeInfoRow(tr("致谢"), tr("感谢所使用的开源软件"), card));
    cl->addWidget(new PanshiSeparator(card));

    m_linkRow = new LinkRow(tr("主页"), QStringLiteral("https://www.deepin.org"), card);
    cl->addWidget(m_linkRow);
    cl->addWidget(new PanshiSeparator(card));
    cl->addWidget(makeInfoRow(tr("协议"),
                               tr("磐石设置遵循 GPL-3.0-or-later 协议发布"), card));
    root->addWidget(card);

    // ---- ③ 技术参考 / 常用命令（默认折叠）----
    // 内容与「关于」页共用 Ui::makeCommandRow 与同一段说明文案，避免两处分叉。
    m_refBox = new PanshiCollapsible(tr("磐石系统技术参考"), body);
    auto *ref = makeLabel(
        tr("· 核心目录只读保护: /usr 只读 overlay, /etc /opt 可写 overlay\n"
           "· 原子更新: 升级失败自动回滚 (auto-rollback-on-failure)\n"
           "· 快照备份: 基于 OSTree 秒级增量快照, 支持导出/导入\n"
           "· 初始备份受系统保护, 不可删除/修改\n"
           "· 快照还原需重启生效, 可写层开启时禁止还原\n"
           "· 系统与数据分区分层: /home 等数据分区不受还原影响"),
        TextLevel::Desc, Semantic::Normal);
    ref->setWordWrap(true);
    m_refBox->setContent(ref);
    root->addWidget(m_refBox);

    m_cmdBox = new PanshiCollapsible(tr("常用命令参考"), body);
    auto *cmdHost = new QWidget(m_cmdBox);
    auto *cmdLay = new QVBoxLayout(cmdHost);
    cmdLay->setContentsMargins(0, 0, 0, 0);
    cmdLay->setSpacing(Space::S);
    struct Cmd { const char *cmd; const char *cmt; };
    static const Cmd kCmds[] = {
        {"sudo deepin-immutable-ctl snapshot list", "列出快照"},
        {"sudo deepin-immutable-ctl snapshot create <名称> <描述>", "创建快照"},
        {"sudo deepin-immutable-ctl snapshot rollback <id>", "还原快照"},
        {"sudo deepin-immutable-writable status", "保护状态"},
        {"sudo deepin-immutable-writable enable|disable", "开/关可写层"},
        {"sudo deepin-immutable-ctl admin status", "部署状态"},
    };
    for (const Cmd &c : kCmds)
        cmdLay->addWidget(Ui::makeCommandRow(QString::fromLatin1(c.cmd),
                                              tr(c.cmt), cmdHost));
    m_cmdBox->setContent(cmdHost);
    root->addWidget(m_cmdBox);

    return body;
}

void PanshiAboutDialog::retheme()
{
    if (m_linkRow)
        m_linkRow->retheme();
}

} // namespace PanshiUi
