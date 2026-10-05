// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui/sidebar.h"
#include "pages/basepage.h"
#include "pages/overviewpage.h"
#include "pages/snapshotspage.h"
#include "pages/versionpage.h"
#include "pages/protectionpage.h"
#include "pages/policypage.h"
#include "pages/deployspage.h"
#include "pages/updatepage.h"
#include "pages/diskusagepage.h"
#include "pages/aboutpage.h"

#include <DTitlebar>
#include <DPushButton>
#include <DLabel>
#include <DPalette>
#include <DGuiApplicationHelper>
#include <DIconTheme>
#include <DFontSizeManager>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QCloseEvent>
#include <QTimer>
DWIDGET_USE_NAMESPACE

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    s_instance = this;
    setWindowTitle(tr("磐石设置"));
    resize(1060, 700);
    setMinimumSize(920, 620);

    auto *tb = titlebar();
    tb->setTitle(tr("磐石设置"));
    tb->setIcon(DIconTheme::findQIcon("panshi"));
    tb->setFixedHeight(46);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    initSidebar();
    initContent();

    // 设置搜索索引（阶段 4）：各页在构造时已建好行，这里收集它们的自报条目。
    // 必须在 initContent() 之后——页面构造才产生 PanshiRow。
    for (BasePage *p : m_pages) {
        const QList<PanshiUi::SettingEntry> entries = p->searchableSettings();
        for (const PanshiUi::SettingEntry &e : entries)
            m_settingIndex.add(e);
    }

    root->addWidget(m_sidebarWidget);
    root->addWidget(m_stack, 1);

    gotoPage(PageId::Overview);
}

void MainWindow::initSidebar()
{
    m_sidebarWidget = new QFrame(this);
    m_sidebarWidget->setObjectName(QStringLiteral("panshiSidebar"));
    m_sidebarWidget->setFixedWidth(200);

    auto *sb = new QVBoxLayout(m_sidebarWidget);
    sb->setContentsMargins(10, 12, 10, 12);
    sb->setSpacing(0);

    // 分组导航（UI 重构方案 §3.1）：
    //  - 标题栏已承载应用名，侧栏不再重复「磐石设置 / 磐石系统管理」标题块；
    //  - 「基于 OSTree」等实现细节归关于页，不再占据侧栏脚注；
    //  - 导航项带主题图标，名称用用户语言（磁盘空间/备份与版本/策略与设置）。
    m_navSidebar = new PanshiUi::PanshiSidebar(m_sidebarWidget);
    //
    // ---- 侧栏图标（2026-10-04 重做）----
    //
    // 原先 7 个名字里有 **5 个在 DCI 图标库里根本不存在**
    // （security-high / object-locked / document-save(-as) /
    //   software-update-available / system-upgrade / go-home / help-about），
    // DIconTheme 返回空 QIcon → navIcon() 一路回落到 QStyle 标准图标，
    // 渲染出来就是用户截图里那两个**灰色细线小箭头**（↓ 备份、↑ 更新）。
    // 只有 drive-harddisk 与 preferences-system 真实存在 ——
    // 这正好解释了截图里「只有磁盘空间是彩色的」。
    //
    // 换库图标后仍不满意（多为单色细线 / 大面积白底，20px 下等于看不见），
    // 故其中 4 项改为**应用自带 SVG**（data/icons/nav/*.svg，键即下表的第一个参数）：
    //   系统保护 shield-check     盾牌+对勾    蓝
    //   备份与版本 layers-snapshot 三层堆叠     琥珀
    //   系统更新 cycle-update     环形箭头     青绿
    //   关于     info-badge       圆角方章+i   靛蓝
    // 自带的好处：任何系统 / 任何图标主题 / 亮暗两套都完全一致，且不依赖 DConfig。
    // 剩余 3 项（总览/磁盘空间/策略与设置）继续用 DCI 彩色图标 ——
    // 它们在库里真实存在且 20px 下清晰（实测彩占比 95%~100%）。
    //
    // ⚠️ 选图标的判据不能只看饱和度：只看 HSV 的 S 分位 90 会被
    // 「大面积白底 + 少量高饱和 accent」的图标骗过去（实测某图标 S90=0.94
    // 但彩占比仅 30%、明度 0.83，20px 下几乎不可见）。必须同时看
    // **彩占比**（着色像素 / 全部不透明像素）。探针 src/tools/navshot.cpp 守着这条。
    m_navSidebar->beginGroup(tr("概览"));
    m_navSidebar->addItem(QStringLiteral("deepin-system-monitor"), QStringLiteral("deepin-home"),
                          tr("总览"), int(PageId::Overview));
    m_navSidebar->beginGroup(tr("系统维护"));
    m_navSidebar->addItem(QStringLiteral("drive-harddisk"), QStringLiteral("deepin-diskmanager"),
                          tr("磁盘空间"), int(PageId::DiskUsage));
    // 这 4 项用**自带 SVG**（第一个参数即 qrc 里的键，见 sidebar.cpp::navIcon
    // 「自带 SVG 优先」）：库里没有对应语义的彩色图标，硬用库图标只会回落成灰箭头。
    m_navSidebar->addItem(QStringLiteral("shield-check"), QStringLiteral("deepin-defender"),
                          tr("系统保护"), int(PageId::Protection));
    m_navSidebar->addItem(QStringLiteral("layers-snapshot"), QStringLiteral("application-x-archive"),
                          tr("备份与版本"), int(PageId::BackupVersion));
    m_navSidebar->addItem(QStringLiteral("cycle-update"), QStringLiteral("upgrade-tool"),
                          tr("系统更新"), int(PageId::Update));
    m_navSidebar->beginGroup(tr("设置"));
    m_navSidebar->addItem(QStringLiteral("preferences-system"), QStringLiteral("deepin-home"),
                          tr("策略与设置"), int(PageId::Policy));
    m_navSidebar->addItem(QStringLiteral("info-badge"), QStringLiteral("dialog-information"),
                          tr("关于"), int(PageId::About));
    m_navSidebar->activated = [this](int data) { gotoPage(static_cast<PageId>(data)); };

    // 设置搜索（阶段 4）：索引在页面构造时已注册，这里只接线落地回调。
    m_navSidebar->setSettingIndex(&m_settingIndex);
    m_navSidebar->settingActivated = [this](int pageId, const QString &key) {
        gotoSetting(pageId, key);
    };

    sb->addWidget(m_navSidebar);
}

void MainWindow::initContent()
{
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("panshiContent"));

    // QMap 按 key 升序迭代，而 PageId 的枚举顺序即期望的导航顺序，
    // 因此 insert 顺序与 QStackedWidget 下标天然一致。
    m_pages.insert(PageId::Overview, new OverviewPage(this));
    m_pages.insert(PageId::DiskUsage, new DiskUsagePage(this));
    m_pages.insert(PageId::BackupVersion, new VersionManagementPage(this));
    m_pages.insert(PageId::Protection, new ProtectionPage(this));
    m_pages.insert(PageId::Policy, new PolicyPage(this));
    m_pages.insert(PageId::Update, new UpdatePage(this));
    m_pages.insert(PageId::About, new AboutPage(this));

    for (BasePage *p : m_pages)
        m_stack->addWidget(p);
}

MainWindow::~MainWindow()
{
    if (s_instance == this)
        s_instance = nullptr;
}

BasePage *MainWindow::page(PageId id) const
{
    return m_pages.value(id, nullptr);
}

int MainWindow::toIndex(PageId id) const
{
    return m_pages.keys().indexOf(id);
}

void MainWindow::gotoPage(PageId id)
{
    if (!page(id))
        return;
    switchTo(id);
}

void MainWindow::gotoPage(PageId id, SubPageId sub)
{
    BasePage *p = page(id);
    if (!p)
        return;
    // 先切换子页，再 switchTo，保证后续 refresh() 刷新的是目标子页
    if (auto *v = qobject_cast<VersionManagementPage *>(p))
        v->showSubPage(static_cast<int>(sub));
    switchTo(id);
}

void MainWindow::gotoPage(PageId id, std::function<void()> afterArrive)
{
    BasePage *p = page(id);
    if (!p)
        return;
    switchTo(id);
    if (afterArrive)
        scheduleArrivalAction(id, std::move(afterArrive));
}

void MainWindow::gotoPage(PageId id, SubPageId sub, std::function<void()> afterArrive)
{
    BasePage *p = page(id);
    if (!p)
        return;
    // 子页切换必须先于 switchTo：switchTo 内部的 refresh() 刷的是"当前子页"，
    // 顺序反了会刷到旧子页，到达动作再落到一个没数据的新子页上。
    if (auto *v = qobject_cast<VersionManagementPage *>(p))
        v->showSubPage(static_cast<int>(sub));
    switchTo(id);
    if (afterArrive)
        scheduleArrivalAction(id, std::move(afterArrive));
}

void MainWindow::scheduleArrivalAction(PageId id, std::function<void()> action)
{
    BasePage *target = page(id);
    if (!target)
        return;

    // 后到覆盖先到：用户最后一次点击最符合其当前意图。
    // 覆盖时先解绑旧宿主的连接，避免旧页在后台空闲时触发已作废的动作。
    if (m_arrivalPending) {
        if (BasePage *prev = page(m_arrivalPageId))
            disconnect(prev, &BasePage::idleReached, this, nullptr);
        m_arrivalPending = false;
    }
    if (m_arrivalTimer)
        m_arrivalTimer->stop();

    m_arrivalAction = std::move(action);
    m_arrivalPageId = id;

    if (!m_arrivalTimer) {
        m_arrivalTimer = new QTimer(this);
        m_arrivalTimer->setSingleShot(true);
        // 超时兜底：后端卡住导致页面迟迟不空闲时，仍让动作有机会执行——
        // 动作自身大多带防重入守卫，最坏情况是提示"上一项操作尚未完成"，
        // 也远好过按钮点下去毫无反馈。
        connect(m_arrivalTimer, &QTimer::timeout, this, [this] {
            flushArrivalAction(true);
        });
    }

    if (!target->isBusy()) {
        // 页面本次 refresh() 是同步完成的（如 UpdatePage::refresh 为空实现，
        // 或已有缓存瞬间返回）：无在途任务，立即执行，不必等一个永不到来的信号。
        flushArrivalAction(true);
        return;
    }

    m_arrivalPending = true;
    connect(target, &BasePage::idleReached, this, [this, id] {
        // 只认当前挂起动作对应的那个页面（用户可能已经点了别的快速操作）
        if (m_arrivalPending && m_arrivalPageId == id)
            flushArrivalAction(true);
    });
    m_arrivalTimer->start(kArrivalTimeoutMs);
}

void MainWindow::flushArrivalAction(bool run)
{
    if (m_arrivalTimer)
        m_arrivalTimer->stop();
    if (m_arrivalPending) {
        if (BasePage *prev = page(m_arrivalPageId))
            disconnect(prev, &BasePage::idleReached, this, nullptr);
        m_arrivalPending = false;
    }
    std::function<void()> act = std::move(m_arrivalAction);
    m_arrivalAction = nullptr;
    // 先清状态再执行：动作内部可能再次 gotoPage（如"可写层冲突 → 去保护页"），
    // 那时 scheduleArrivalAction 看到的是干净状态，不会被本次残留干扰。
    if (run && act)
        act();
}

void MainWindow::switchTo(PageId id)
{
    const int index = toIndex(id);
    if (index < 0)
        return;
    // 任何一次切页都作废上一个挂起的「到达后动作」。
    // 用户改了主意去点侧栏（或别的入口）时，旧动作再迟到弹出来就是误操作。
    // 注意时序：gotoPage(id, action) 里是 switchTo 在前、scheduleArrivalAction
    // 在后 —— 本行清掉的是"上一次"的动作，不会伤到本次刚要挂上的那个。
    flushArrivalAction(false);
    m_stack->setCurrentIndex(index);
    if (m_navSidebar)
        m_navSidebar->setCurrentData(static_cast<int>(id));
    m_pages.value(id)->refresh();
}

void MainWindow::refreshPanshiPages()
{
    // 「磐石功能」开关切换后同步两页状态：
    // - 系统保护页仅读取一次可写层状态，开销极小，总是刷新；
    // - 总览页含较重的存储统计，仅当前可见时刷新（隐藏时由 gotoPage 导航刷新）。
    if (auto *p = page(PageId::Protection))
        p->refresh();
    if (m_stack->currentIndex() == toIndex(PageId::Overview)) {
        if (auto *o = page(PageId::Overview))
            o->refresh();
    }
}

void MainWindow::showDiskDate(const QDate &date)
{
    auto *diskPage = qobject_cast<DiskUsagePage *>(m_pages.value(PageId::DiskUsage));
    if (!diskPage)
        return;
    gotoPage(PageId::DiskUsage);
    diskPage->focusDate(date);
}

void MainWindow::gotoSetting(int pageId, const QString &targetKey)
{
    const PageId id = static_cast<PageId>(pageId);
    BasePage *p = page(id);
    if (!p || targetKey.isEmpty())
        return;

    // 先切页（会触发 refresh），再让页面定位并高亮目标行。
    // 顺序不能反：隐藏页在 QStackedWidget 里尚未布局，此时滚动定位会算错位置。
    switchTo(id);
    p->highlightSetting(targetKey);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // 挂起的「到达后动作」在退出时作废：不执行（用户已不关心），
    // 只解绑连接与定时器，避免关闭过程中回调到正在析构的页面。
    flushArrivalAction(false);
    for (BasePage *p : m_pages)
        p->shutdown();
    DMainWindow::closeEvent(e);
}
