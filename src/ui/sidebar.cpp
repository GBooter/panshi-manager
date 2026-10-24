// SPDX-License-Identifier: GPL-3.0-or-later
#include "sidebar.h"

#include <DFontSizeManager>
#include <DIconTheme>
#include <DGuiApplicationHelper>

#include <QApplication>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QFontMetrics>
#include <QPixmap>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace PanshiUi {

namespace {

// 自定义数据角色
//
// 注意：delegate 按 kGroupRole / kDataRole 取值。
//   · 导航模型：UserRole + 1 / + 2
//   · 导航项存原始图标名（主题切换时据此重查，2026-10-03）：UserRole + 21 / + 22
//
// 2026-10-06：原先搜索结果模型用的 +11/+12/+13 三个角色已随搜索功能一并移除。
constexpr int kDataRole = Qt::UserRole + 1;   // int：PageId
constexpr int kGroupRole = Qt::UserRole + 2;  // bool：分组标题行
// 导航项存原始图标名（主题切换时据此重查，2026-10-03）。
constexpr int kNavIconRole = Qt::UserRole + 21;     // QString：首选图标名
constexpr int kNavFallbackRole = Qt::UserRole + 22; // QString：备选图标名

/**
 * 导航图标查找：**自带 SVG 优先**，其次 DCI 彩色版，最后单色兜底。
 *
 * ① 自带 SVG（2026-10-04 新增，优先级最高）
 *   为什么排在最前：本次故障的根因就是「图标库里没有对应语义的彩色图标」——
 *   DIconTheme 返回空，navIcon() 一路回落到 QStyle 标准图标，渲染成灰色小箭头。
 *   四个语义（保护/备份/更新/关于）在库里要么缺失、要么单色细线、要么大面积白底
 *   （20px 下等于看不见）。自带 SVG（data/icons/nav/*.svg，见 icons.qrc）才能保证
 *   **任何系统、任何图标主题、亮/暗两套下都完全一致**，且不依赖 DConfig。
 *   加载键就是 addItem() 传入的第一个参数，故此处不需要额外映射表。
 *
 * ② DCI 彩色版
 *   ⚠️ 彩色优先的原因（2026-10-03 用户反馈「图标也没法看见」）：
 *   库里的单色 symbolic 浅底上是深灰、深底上是浅灰，**两边都缺色彩锚点**，
 *   扫视侧栏时辨识度很低。彩色版有稳定色相，识别度显著提升。
 *
 *   ⚠️ 2026-10-24 侧栏改线性图标风格后：自带 SVG 改为「单色 outline 源图」
 *   （纯黑，颜色由 delegate 现染，见 tintedPixmap()），7 个语义全部有自带文件，
 *   本条 DCI 链实际已不再命中，保留只为「自带文件被删」时的兜底。
 *
 * 完整回落链（任一级缺失都还有下一级，保证任何环境下都有图）：
 *   1. 自带 SVG → 2. 彩色首选名 → 3. 彩色备选名 → 4. 单色同名 → 5. QStyle 标准图标
 */
QIcon navIcon(const QString &name, const QString &fallbackName)
{
    // ① 自带 SVG（qrc）。QFileInfo::exists 对 qrc 路径同样有效。
    const QString bundled = QStringLiteral(":/icons/nav/%1.svg").arg(name);
    if (QFileInfo::exists(bundled))
        return QIcon(bundled);

    // ②③④ DCI：彩色版候选，deepin 图标库的彩色命名习惯是加后缀，逐一试；
    // 缺失时 DIconTheme 返回空 QIcon，继续下一级。
    static const QStringList kColorSuffixes = {
        QStringLiteral("-colored"),   // 部分主题用 -colored
        QStringLiteral("_colored"),
        QString()                     // 原名（若主题里它本身就是彩色）
    };

    for (const QString &suffix : kColorSuffixes) {
        const QIcon ic = DIconTheme::findQIcon(name + suffix);
        if (!ic.isNull())
            return ic;
    }
    if (!fallbackName.isEmpty()) {
        const QIcon ic = DIconTheme::findQIcon(fallbackName);
        if (!ic.isNull())
            return ic;
    }
    return QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder);
}

// 图标染色统一用 PanshiUi::tintedPixmap()（2026-10-24 提到 components 层，
// 侧栏 delegate 与按钮图标共用一份实现；原命名空间内的私有副本已删除，
// 设计说明见 components.h）。

class SidebarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &idx) const override
    {
        if (idx.data(kGroupRole).toBool())
            // 首个分组贴顶不需要额外空隙，后续分组与上一项留出呼吸空间
            return QSize(opt.rect.width(), idx.row() == 0 ? 32 : 46);
        // 44 而非 40：图标从 16px 提到 20px（用户反馈 16px 太小不易识别），
        // 行高须容得下 20px 图标 + 上下内边距，否则图标贴边显得局促。
        return QSize(opt.rect.width(), 44);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const DPalette pa = appPalette();

        if (idx.data(kGroupRole).toBool()) {
            // 分组标题：小号 TextTips 文字，首行贴顶
            p->setFont(DFontSizeManager::instance()->get(DFontSizeManager::T7));
            p->setPen(pa.color(DPalette::TextTips));
            const int topPad = idx.row() == 0 ? 8 : 16;
            p->drawText(opt.rect.adjusted(Space::L, topPad, -Space::L, 0),
                        Qt::AlignLeft | Qt::AlignVCenter, idx.data(Qt::DisplayRole).toString());
            p->restore();
            return;
        }

        // 导航项：圆角底（选中=Highlight 柔和底，hover=ItemBackground），无描边
        const bool selected = (opt.state & QStyle::State_Selected);
        const bool hovered = !selected && (opt.state & QStyle::State_MouseOver);
        QRect r = opt.rect.adjusted(Space::S, 2, -Space::S, -2);
        if (selected) {
            p->setPen(Qt::NoPen);
            p->setBrush(softFill(Semantic::Accent));
            p->drawRoundedRect(r, Radius::Frame, Radius::Frame);
        } else if (hovered) {
            p->setPen(Qt::NoPen);
            p->setBrush(itemBackground());
            p->drawRoundedRect(r, Radius::Frame, Radius::Frame);
        }

        // 图标 20px（Size::Icon）：从 16px 提上来的原因见 sizeHint() 注释 ——
        // 用户反馈 16px 在侧栏里不易识别，20px 是「明显变大但不挤压文字」的上限
        // （再大到 24px 需要行高 48，一屏能见的导航项会少两个）。
        //
        // 2026-10-24 线性图标风格：SVG 是纯黑单色源图，颜色在这里现染 ——
        //   选中 = Accent（与文字同色，图标+文字一起高亮，对齐参考设计）；
        //   常规 = Normal（正文色：亮色深灰/暗色浅灰，跟随 DPalette）。
        // 恒用「 Normal 语义 + 自染」而不给 QIcon 传 Selected 状态的原因
        // 见 tintedPixmap() 注释（主题会整图替换 Selected，颜色失控）。
        const QIcon ic = idx.data(Qt::DecorationRole).value<QIcon>();
        const QRect iconRect(r.left() + Space::M,
                             r.center().y() - Size::Icon / 2,
                             Size::Icon, Size::Icon);
        const QColor iconColor = selected ? semanticColor(Semantic::Accent)
                                          : semanticColor(Semantic::Normal);
        const QPixmap iconPx = tintedPixmap(ic, iconRect.size(), iconColor);
        if (!iconPx.isNull())
            p->drawPixmap(iconRect, iconPx);

        // 文字：选中 = Highlight（语义 Accent），常规 = TextTitle（语义 Normal）
        p->setFont(DFontSizeManager::instance()->get(DFontSizeManager::T6));
        p->setPen(selected ? semanticColor(Semantic::Accent)
                           : semanticColor(Semantic::Normal));
        // 左缩进随图标尺寸联动（16→20 多出的 4px 要让出来，文字才不会偏左）
        QRect textRect = r.adjusted(Size::Icon + Space::M * 2, 0, -Space::S, 0);
        p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(p->font()).elidedText(
                        idx.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                        textRect.width()));

        // 键盘焦点框：用 QStyle 原生绘制（方案 §4.7：焦点框可见、非自绘 outline:none）
        if (opt.state & QStyle::State_HasFocus) {
            QStyleOptionFocusRect fo;
            fo.rect = r.adjusted(2, 2, -2, -2);
            fo.state = opt.state;
            fo.backgroundColor = pa.color(DPalette::Base);
            opt.widget->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &fo, p);
        }
        p->restore();
    }
};

} // namespace

PanshiSidebar::PanshiSidebar(QWidget *parent)
    : QWidget(parent)
{
    m_model = new QStandardItemModel(this);

    m_view = new QListView(this);
    m_view->setModel(m_model);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setItemDelegate(new SidebarDelegate(m_view));
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setMouseTracking(true);              // 保证 hover 高亮
    m_view->setFocusPolicy(Qt::StrongFocus);     // Tab 可进入侧栏（默认策略实测未入焦链）
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->viewport()->setAutoFillBackground(false);
    m_view->setObjectName(QStringLiteral("panshiNavList"));
    // 无障碍：列表本身要有名称，读屏才知道当前在导航区
    m_view->setAccessibleName(QObject::tr("主导航"));

    // ⚠️ 2026-10-06 按用户要求移除设置搜索：原此处创建 DLineEdit(m_search) 与
    // 结果列表(m_results)，并把两者塞进同一个 QVBoxLayout 与 m_view 互斥显示。
    // 现在侧栏只有导航列表一项，布局里不再需要 addWidget(m_search)/addWidget(m_results)。
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::S);
    lay->addWidget(m_view, 1);

    // 点击与回车（activated）都触发导航
    auto emitActivation = [this](const QModelIndex &idx) {
        if (idx.isValid() && !idx.data(kGroupRole).toBool() && activated)
            activated(idx.data(kDataRole).toInt());
    };
    connect(m_view, &QListView::clicked, this, emitActivation);
    connect(m_view, &QListView::activated, this, emitActivation);

    // ⚠️ 原先装在 m_search->lineEdit() 与 m_results 上的 eventFilter
    // （搜索态 ↑↓/Esc 键盘导航）已随搜索功能一并移除。

    // ---- 主题切换：强制全量重绘（2026-10-03 用户反馈修；2026-10-24 简化）----
    //
    // 症状（用户描述）：「切换暗色时有时不会变」「图标也没法看见」。
    // 两个根因，历史解法与现状：
    //
    // ① **图标不跟随主题**：`DIconTheme::findQIcon()` 返回的 QIcon 在**构造时**
    //    就把当前主题的资源解析好了，存在模型里之后**不会因主题切换而重新解析**
    //    （DTK 官方文档「主题切换时的图标更新」一节要求手动重查）。
    //    → 原先靠 rebuildIcons 重查并重写回模型。
    //    ⚠️ 2026-10-24 起**颜色已不再依赖重查**：自带 SVG 改为单色源图，
    //    颜色由 delegate 在 paint 时按 appPalette() 现染（见 tintedPixmap()），
    //    主题切换天然跟随。rebuildIcons 现在只为「自带文件被删、回落到 DCI」
    //    这条兜底路径保留（qrc 内的文件不会因主题变化，但兜底图标会）。
    //
    // ② **delegate 不重绘**：侧栏是自绘（`SidebarDelegate::paint` 走
    //    `appPalette()` 实时取色，本身是对的），但 QListView 在调色板替换后
    //    未必会主动重绘全部行 → 表现为「有时不变」（部分行变了、部分没变）。
    //    → 显式 `viewport()->update()` 强制全量重绘（**这条仍是主路径**）。
    //
    // 挂 `applicationPaletteChanged` 而非 `themeTypeChanged`：前者在新调色板
    // 就位后才发（main.cpp 的注释已详述该时序差），两个切换方向都稳。
    auto rebuildIcons = [this] {
        QStandardItemModel *mdl = qobject_cast<QStandardItemModel *>(m_view->model());
        if (!mdl)
            return;
        for (int r = 0; r < mdl->rowCount(); ++r) {
            QStandardItem *it = mdl->item(r);
            if (!it)
                continue;
            const QString primary = it->data(kNavIconRole).toString();
            if (primary.isEmpty())
                continue;
            const QString fallback = it->data(kNavFallbackRole).toString();
            it->setIcon(navIcon(primary, fallback));
        }
        m_view->viewport()->update();
    };
    connect(DGuiApplicationHelper::instance(),
            &DGuiApplicationHelper::applicationPaletteChanged,
            this, [this, rebuildIcons] {
        rebuildIcons();
        // 分组标题与选中态也依赖取色，一并重绘
        m_view->viewport()->update();
    });
}

void PanshiSidebar::beginGroup(const QString &title)
{
    auto *it = new QStandardItem(title);
    it->setData(true, kGroupRole);
    it->setFlags(Qt::NoItemFlags);   // 不可选中/不可聚焦，键盘导航自动跳过
    m_model->appendRow(it);
}

void PanshiSidebar::addItem(const QString &iconName, const QString &fallbackIcon,
                            const QString &text, int pageData)
{
    auto *it = new QStandardItem(navIcon(iconName, fallbackIcon), text);
    it->setData(pageData, kDataRole);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    it->setToolTip(text);
    // 存原始图标名：主题切换时 DIconTheme 需重新解析才能拿到新主题的图标
    // （QIcon 一旦构造完成就固化了，详见构造函数里 rebuildIcons 的注释）
    it->setData(iconName, kNavIconRole);
    it->setData(fallbackIcon, kNavFallbackRole);
    m_model->appendRow(it);
}

bool PanshiSidebar::setCurrentData(int pageData)
{
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QModelIndex idx = m_model->index(i, 0);
        if (!idx.data(kGroupRole).toBool()
            && idx.data(kDataRole).toInt() == pageData) {
            m_view->blockSignals(true);   // 编程式选中不回调
            m_view->setCurrentIndex(idx);
            m_view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
            m_view->blockSignals(false);
            return true;
        }
    }
    return false;
}

} // namespace PanshiUi
