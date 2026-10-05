// SPDX-License-Identifier: GPL-3.0-or-later
#include "sidebar.h"

#include <DFontSizeManager>
#include <DIconTheme>
#include <DGuiApplicationHelper>

#include <QApplication>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace PanshiUi {

namespace {

// 自定义数据角色
//
// 注意：导航模型与结果模型共用同一个 SidebarDelegate，delegate 一律按
// kGroupRole / kDataRole 取值。因此两套角色必须落在**互不重叠**的区间，
// 否则结果行会把 pageId 喂给 kGroupRole，被误判成"分组标题行"而错位。
//   · 导航模型：UserRole + 1 / + 2
//   · 结果模型：UserRole + 11 / + 12 / + 13
constexpr int kDataRole = Qt::UserRole + 1;   // int：PageId
constexpr int kGroupRole = Qt::UserRole + 2;  // bool：分组标题行
// 搜索结果行
constexpr int kTargetKeyRole = Qt::UserRole + 11;  // QString：落地键（PanshiRow 标题）
constexpr int kResultPageRole = Qt::UserRole + 12; // int：PageId
constexpr int kHintRole = Qt::UserRole + 13;       // bool：提示行（无匹配）
// 导航项存原始图标名（主题切换时据此重查，2026-10-03）。
// 落在 +21/+22，仍与上面两段互不重叠 —— 角色号撞车会让 delegate 取到错值。
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
        const QIcon ic = idx.data(Qt::DecorationRole).value<QIcon>();
        const QRect iconRect(r.left() + Space::M,
                             r.center().y() - Size::Icon / 2,
                             Size::Icon, Size::Icon);
        // ⚠️ 彩色图标 + Selected 模式会互相干扰：QIcon::Selected 会对图标做
        // 状态替换（部分主题下直接把彩色图标压成单色），反而丢掉了颜色锚点。
        // 故**恒用 QIcon::Normal** —— 靠「柔和底 + 文字高亮」表达选中，
        // 不动图标本身（这正是选中态仍清晰的关键）。
        ic.paint(p, iconRect, Qt::AlignCenter, QIcon::Normal,
                 opt.state & QStyle::State_Enabled ? QIcon::On : QIcon::Off);

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
    // 无障碍：列表本身要有名称，读屏才知道当前在导航区还是搜索结果区
    m_view->setAccessibleName(QObject::tr("主导航"));

    // ---- 设置搜索（阶段 4）：输入框 + 结果列表 ----
    // 结果列表覆盖在导航之上（同一位置互斥显示），避免侧栏被撑成两段。
    m_search = new DLineEdit(this);
    m_search->setPlaceholderText(QObject::tr("搜索设置"));
    m_search->setClearButtonEnabled(true);
    // 无障碍：placeholder 只是视觉提示，屏幕阅读器需显式名称；
    // 搜索结果列表也要有名称，否则读屏只会念出一串条目而无上下文
    m_search->setAccessibleName(QObject::tr("搜索设置"));
    m_search->lineEdit()->setObjectName(QStringLiteral("panshiSearchEdit"));
    m_search->lineEdit()->setAccessibleDescription(
        QObject::tr("输入设置名称可跳转到对应设置项"));
    m_search->hide();   // setSettingIndex() 后才显示

    m_resultModel = new QStandardItemModel(this);
    m_results = new QListView(this);
    m_results->setModel(m_resultModel);
    m_results->setFrameShape(QFrame::NoFrame);
    m_results->setItemDelegate(new SidebarDelegate(m_results));
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setMouseTracking(true);
    m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_results->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_results->viewport()->setAutoFillBackground(false);
    m_results->setObjectName(QStringLiteral("panshiSearchResultList"));
    m_results->setAccessibleName(QObject::tr("设置搜索结果"));
    m_results->hide();

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(Space::S);
    lay->addWidget(m_search);
    lay->addWidget(m_view, 1);
    lay->addWidget(m_results, 1);

    // 点击与回车（activated）都触发导航
    auto emitActivation = [this](const QModelIndex &idx) {
        if (idx.isValid() && !idx.data(kGroupRole).toBool() && activated)
            activated(idx.data(kDataRole).toInt());
    };
    connect(m_view, &QListView::clicked, this, emitActivation);
    connect(m_view, &QListView::activated, this, emitActivation);

    // 搜索：文本变化即刷新结果；回车/双击落地；Esc 清空；↑↓ 移动高亮
    connect(m_search->lineEdit(), &QLineEdit::textChanged,
            this, &PanshiSidebar::onSearchTextChanged);
    connect(m_search->lineEdit(), &QLineEdit::returnPressed, this, [this] {
        const QModelIndex idx = m_results->currentIndex();
        if (idx.isValid() && !idx.data(kHintRole).toBool())
            activateResult(idx.row());
    });
    connect(m_results, &QListView::clicked, this,
            [this](const QModelIndex &idx) { activateResult(idx.row()); });
    connect(m_results, &QListView::activated, this,
            [this](const QModelIndex &idx) { activateResult(idx.row()); });

    // ↑↓ / Esc 的事件过滤器装在**输入框**上，不是结果列表。
    //
    // ⚠️ 缺陷 A2-1 / A2-2 的根因（2026-10-02 实测）：原实现把 filter 装在
    // m_results 上、且只处理 Esc。但搜索态下**焦点始终在 lineEdit**——
    //   · ↑↓ 被 QLineEdit 消费为「输入历史上下翻」，根本不会到达结果列表；
    //   · Esc 同理，用户看到的是 QLineEdit 自带行为（未清空我们的状态）；
    //   · 而结果列表**永远拿不到焦点**：点它会走 clicked → activateResult
    //     → 立即落地并切页（这本身是设计），焦点没机会停留。
    // 结论：把键盘导航放在**持有焦点的那个控件**上。
    // 保留 m_results 上的过滤器：焦点若因程序化 setFocus 落到列表上（如
    // 未来加「结果列表 Tab 可达」），Esc 仍能正确清空。
    m_search->lineEdit()->installEventFilter(this);
    m_results->installEventFilter(this);

    // ---- 主题切换：重查图标 + 强制重绘（2026-10-03 用户反馈修）----
    //
    // 症状（用户描述）：「切换暗色时有时不会变」「图标也没法看见」。
    // 两个根因，本处一并解决：
    //
    // ① **图标不跟随主题**：`DIconTheme::findQIcon()` 返回的 QIcon 在**构造时**
    //    就把当前主题的资源解析好了，存在模型里之后**不会因主题切换而重新解析**
    //    （DTK 官方文档「主题切换时的图标更新」一节要求手动重查）。
    //    → 必须重查并重写回模型。
    //
    // ② **delegate 不重绘**：侧栏是自绘（`SidebarDelegate::paint` 走
    //    `appPalette()` 实时取色，本身是对的），但 QListView 在调色板替换后
    //    未必会主动重绘全部行 → 表现为「有时不变」（部分行变了、部分没变）。
    //    → 显式 `viewport()->update()` 强制全量重绘。
    //
    // 挂 `applicationPaletteChanged` 而非 `themeTypeChanged`：前者在新调色板
    // 就位后才发（main.cpp 的注释已详述该时序差），两个切换方向都稳。
    auto rebuildIcons = [this] {
        for (QListView *view : {m_view, m_results}) {
            QStandardItemModel *mdl = qobject_cast<QStandardItemModel *>(view->model());
            if (!mdl)
                continue;
            for (int r = 0; r < mdl->rowCount(); ++r) {
                QStandardItem *it = mdl->item(r);
                if (!it)
                    continue;
                // 结果模型的图标是搜索统一用 preferences-system，重查即可；
                // 导航模型则从 kNavIconRole 存的原名重查（见 addItem）。
                const QString primary = it->data(kNavIconRole).toString();
                if (primary.isEmpty())
                    continue;
                const QString fallback = it->data(kNavFallbackRole).toString();
                it->setIcon(navIcon(primary, fallback));
            }
            view->viewport()->update();
        }
    };
    connect(DGuiApplicationHelper::instance(),
            &DGuiApplicationHelper::applicationPaletteChanged,
            this, [this, rebuildIcons] {
        rebuildIcons();
        // 分组标题与选中态也依赖取色，一并重绘
        if (m_view)  m_view->viewport()->update();
        if (m_results) m_results->viewport()->update();
    });
}

/**
 * 搜索态键盘导航：↑↓ 移动结果高亮、Esc 清空。
 *
 * 装在 m_search->lineEdit() 与 m_results 两处（见构造函数的说明）。
 */
bool PanshiSidebar::eventFilter(QObject *watched, QEvent *e)
{
    const bool isInput   = (watched == m_search->lineEdit());
    const bool isResults = (watched == m_results);
    if (!isInput && !isResults)
        return QWidget::eventFilter(watched, e);
    // 只有搜索态（结果列表可见）才接管方向键，避免影响导航列表的正常 ↑↓
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, e);

    auto *ke = static_cast<QKeyEvent *>(e);

    // Esc：清空关键词（textChanged 会把视图切回导航）并把焦点交回输入框
    if (ke->key() == Qt::Key_Escape) {
        if (!m_search->lineEdit()->text().isEmpty()) {
            m_search->lineEdit()->clear();
            m_search->lineEdit()->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        // 已空：让 Esc 继续冒泡（不吞掉，避免影响外层对话框/主窗口的 Esc 语义）
        return QWidget::eventFilter(watched, e);
    }

    // ↑↓：移动结果高亮（缺陷 A2-1）。必须在 lineEdit 上拦截 ——
    // 否则 QLineEdit 会把它当输入历史导航，列表高亮不动。
    if (isInput && (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)) {
        if (!m_results->isVisible() || m_resultModel->rowCount() == 0)
            return QWidget::eventFilter(watched, e);
        // 「无匹配」提示行只有 1 行且不可选，无需移动
        if (m_resultModel->rowCount() == 1
            && m_resultModel->index(0, 0).data(kHintRole).toBool())
            return true;

        const int cur = m_results->currentIndex().row();
        const int last = m_resultModel->rowCount() - 1;
        int next = cur;
        if (ke->key() == Qt::Key_Down)
            next = (cur < 0) ? 0 : ((cur >= last) ? 0 : cur + 1);   // 到末尾回卷到首项
        else
            next = (cur < 0) ? 0 : ((cur <= 0) ? last : cur - 1);  // 到首项回卷到末项
        m_results->setCurrentIndex(m_resultModel->index(next, 0));
        m_results->scrollTo(m_resultModel->index(next, 0),
                            QAbstractItemView::PositionAtCenter);
        return true;   // 吞掉：阻止 QLineEdit 翻输入历史
    }

    return QWidget::eventFilter(watched, e);
}

void PanshiSidebar::setSettingIndex(const SettingIndex *index)
{
    m_index = index;
    // 无索引时不提供搜索入口（侧栏退化为纯导航）
    m_search->setVisible(index != nullptr);
    if (!index) {
        m_results->hide();
        m_view->show();
    }
}

void PanshiSidebar::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_resultModel->clear();
        m_results->hide();
        m_view->show();
        return;
    }
    refreshResults();
}

void PanshiSidebar::refreshResults()
{
    m_resultModel->clear();
    const QString kw = m_search->lineEdit()->text().trimmed();

    if (!m_index) {
        showNoResultHint();
        return;
    }
    const QList<SettingEntry> hits = m_index->query(kw);
    if (hits.isEmpty()) {
        showNoResultHint();
        return;
    }

    // 搜索结果统一用「设置」图标；具名常量便于主题切换时重查
    static const QString kResultIcon = QStringLiteral("preferences-system");
    for (const SettingEntry &e : hits) {
        auto *it = new QStandardItem(navIcon(kResultIcon, QString()), e.title);
        it->setData(e.targetKey, kTargetKeyRole);
        it->setData(e.pageId, kResultPageRole);
        it->setToolTip(e.description.isEmpty() ? e.title : e.description);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        // 同样存原名，供主题切换时重建图标（与导航项一致）
        it->setData(kResultIcon, kNavIconRole);
        m_resultModel->appendRow(it);
    }

    m_view->hide();
    m_results->show();
    m_results->setCurrentIndex(m_resultModel->index(0, 0));
}

void PanshiSidebar::showNoResultHint()
{
    m_resultModel->clear();
    auto *it = new QStandardItem(QObject::tr("未找到匹配的设置项"));
    it->setData(true, kHintRole);
    it->setFlags(Qt::NoItemFlags);
    m_resultModel->appendRow(it);
    m_view->hide();
    m_results->show();
}

void PanshiSidebar::activateResult(int row)
{
    const QModelIndex idx = m_resultModel->index(row, 0);
    if (!idx.isValid() || idx.data(kHintRole).toBool())
        return;
    const int pageId = idx.data(kResultPageRole).toInt();
    const QString key = idx.data(kTargetKeyRole).toString();

    // 落地后清空搜索并回到导航视图，避免结果列表长期占位
    m_search->lineEdit()->clear();   // 触发 textChanged → 恢复导航视图

    if (settingActivated)
        settingActivated(pageId, key);
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
