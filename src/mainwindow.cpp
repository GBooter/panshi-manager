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
#include <QIcon>
#include <DFontSizeManager>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWindow>
#include <QFrame>
#include <QScrollArea>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QProcess>
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
    // 新 Logo（2026-10-09）：内嵌资源优先。系统图标目录可能残留旧版
    // panshi.svg（只读 /usr 无法覆盖），DIconTheme 会先解析到它；
    // 内嵌 :/icons/panshi.svg 才能保证任何安装形态下都是新 Logo（同 C9 先例）。
    {
        QIcon appIcon(QStringLiteral(":/icons/panshi.svg"));
        if (appIcon.isNull())
            appIcon = DIconTheme::findQIcon("panshi");
        tb->setIcon(appIcon);
    }
    tb->setFixedHeight(46);

    // ---- 标题栏拖动（2026-10-06 修复：CSD 方案废弃）----
    //
    // 背景：旧方案 DPlatformHandle::setEnabledNoTitlebarForWindow(win, true)
    // 会唤起 dxcb 的 DNoTitlebarWindowHelper，用 DVtableHook 钩住 QWindow::event
    // 把**整个客户区**的鼠标点击语义吞掉 —— 侧栏/按钮/标题栏菜单全部点不动
    // （2026-10-06 A/B 实测坐实）。不启用 CSD 则窗口拖不动：本工程编译时
    // DTK_TITLE_DRAG_WINDOW 宏未定义，DTitlebar 自带的拖动状态机未编进来。
    // 上一轮因此误判为「点击与拖动在当前 DTK 下互斥、只能二选一」。
    //
    // 本方案两者兼得：DTitlebar 的 mousePressed / mouseMoving 信号在 DTK6 库里
    // **无条件导出**（不受 DTK_TITLE_DRAG_WINDOW 宏影响，已用 nm 验证），
    // 标题栏被按住并拖动时发出。我们用「按下时记录基准、移动时按位移跟随」
    // 的方式手动移动窗口（X11 下 setPosition 立即生效、不依赖 WM 交互式移动
    // 的时序，快速甩动也能稳定跟上）；Wayland 下客户端不能自设顶层位置，
    // 改走 QWindow::startSystemMove() 交给合成器。
    // 事件路径保持正常，不吞任何点击 —— 点击可用与拖拽可用不再互斥。
    // ---- 标题栏拖动（2026-10-06 二修：交给窗口管理器交互式移动）----
    //
    // 背景：旧方案在 X11 下用 QWindow::setPosition() 手动跟随光标。
    // 实测发现 KWin 对客户端 configure 请求执行 keepInArea（整窗钳制在屏内）——
    // 窗口一旦贴近屏幕边缘就无法再拖出去（用户反馈「窗口没法拖到桌面屏幕之外」）。
    // 而 WM 的交互式移动（_NET_WM_MOVERESIZE）只约束「标题栏保持可见」，
    // 其余部分允许拖出屏幕边缘。
    // ⇒ 改为标题栏按下（左键）即调用 startSystemMove()：
    //   · X11：Qt 发送 _NET_WM_MOVERESIZE_MOVE，KWin 接管拖动（交互式移动），
    //     窗口可部分拖出屏幕（仅标题栏保持可见）；
    //   · Wayland：合成器接管拖动（xdg_toplevel move），同样允许部分出屏。
    // 事件路径保持正常，不吞任何点击——点击可用与拖拽可用不互斥。
    connect(tb, &DTitlebar::mousePressed, this, [this](Qt::MouseButtons buttons) {
        if ((buttons & Qt::LeftButton) && windowHandle())
            windowHandle()->startSystemMove();
    });

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    initSidebar();
    initContent();
    setupLanguageMenu();

    // ⚠️ 2026-10-06 按用户要求移除侧栏设置搜索 → 这里不再收集设置索引。
    // 原逻辑（收集各页 searchableSettings() 填 m_settingIndex 供侧栏搜索查询）
    // 已随搜索入口一并撤除：m_settingIndex 与页面侧 searchableSettings() 均保留，
    // 但侧栏不再消费，恢复搜索时接回 setSettingIndex() 即可。

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
    //
    // ---- 2026-10-24：侧栏改「线性图标 + 平铺」风格（对齐参考设计）----
    //
    // ① **去掉分组标题**（原「概览/系统维护/设置」三组）：参考设计
    //    （中州韵输入法安装器）侧栏为无分组平铺列表，7 项一屏放得下，
    //    分组标题在只有 7 项时只增加视觉层级、不增加信息量。
    //    PanshiSidebar::beginGroup() 本身保留（API 未删，恢复分组时接回）。
    //
    // ② **图标改单色线性**：原 4 个自带 SVG 是彩色实心、3 个走 DCI 彩色，
    //    选中态只染文字不染图标。现 7 项全部改用自带**单色 outline** SVG
    //    （data/icons/nav/*.svg，第一个参数即 qrc 键），颜色由 delegate
    //    在绘制时现染（选中=Accent、常规=正文色），图标文字一起高亮。
    //    自带的好处不变：任何系统 / 任何图标主题 / 亮暗两套都完全一致，
    //    且不依赖 DConfig —— 顺带把原走 DCI 的 3 项（总览/磁盘空间/
    //    策略与设置）也从「图标主题依赖」里解放出来。
    //
    // ⚠️ 历史教训保留（2026-10-04 彩色图标时期）：判图标不能只看饱和度，
    //    「大面积白底 + 少量高饱和 accent」会被 S90 骗过（实测 S90=0.94
    //    但 20px 下几乎不可见）。线性图标时代的对应判据（墨迹覆盖率 +
    //    图标间差异度）已改由探针 src/tools/navshot.cpp 守着。
    m_navSidebar = new PanshiUi::PanshiSidebar(m_sidebarWidget);
    m_navSidebar->addItem(QStringLiteral("deepin-system-monitor"), QStringLiteral("deepin-home"),
                          tr("总览"), int(PageId::Overview));
    m_navSidebar->addItem(QStringLiteral("drive-harddisk"), QStringLiteral("deepin-diskmanager"),
                          tr("磁盘空间"), int(PageId::DiskUsage));
    m_navSidebar->addItem(QStringLiteral("shield-check"), QStringLiteral("deepin-defender"),
                          tr("系统保护"), int(PageId::Protection));
    m_navSidebar->addItem(QStringLiteral("layers-snapshot"), QStringLiteral("application-x-archive"),
                          tr("备份与版本"), int(PageId::BackupVersion));
    m_navSidebar->addItem(QStringLiteral("cycle-update"), QStringLiteral("upgrade-tool"),
                          tr("系统更新"), int(PageId::Update));
    m_navSidebar->addItem(QStringLiteral("preferences-system"), QStringLiteral("deepin-home"),
                          tr("策略与设置"), int(PageId::Policy));
    m_navSidebar->addItem(QStringLiteral("info-badge"), QStringLiteral("dialog-information"),
                          tr("关于"), int(PageId::About));
    m_navSidebar->activated = [this](int data) { gotoPage(static_cast<PageId>(data)); };

    // ⚠️ 2026-10-06 移除侧栏设置搜索：原此处 setSettingIndex(&m_settingIndex) +
    // settingActivated → gotoSetting() 的接线已删除（搜索入口不存在了）。
    // gotoSetting() 与 m_settingIndex 保留在类里，恢复搜索时接回即可。

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

void MainWindow::setupLanguageMenu()
{
    // 标题栏菜单（☰）：加入「语言」子菜单，提供中/英/跟随系统三档。
    // 切换语言需要重载翻译并重建所有代码内建 UI，最稳妥、最完整的做法是
    // 把偏好写入 QSettings 后重启自身（见 switchLanguage）。
    DTitlebar *tb = titlebar();
    QMenu *root = tb->menu();
    if (!root) {
        root = new QMenu(this);
        tb->setMenu(root);
    }

    auto *langMenu = new QMenu(tr("语言"), root);

    QAction *actSys = langMenu->addAction(tr("跟随系统"));
    QAction *actZh  = langMenu->addAction(tr("简体中文"));
    QAction *actEn  = langMenu->addAction(tr("English"));

    auto *grp = new QActionGroup(langMenu);
    grp->setExclusive(true);
    actSys->setActionGroup(grp); actSys->setCheckable(true);
    actZh->setActionGroup(grp);  actZh->setCheckable(true);
    actEn->setActionGroup(grp);  actEn->setCheckable(true);

    QSettings s(QStringLiteral("deepin"), QStringLiteral("panshi-manager"));
    const QString cur =
        s.value(QStringLiteral("language"), QStringLiteral("system")).toString();
    if (cur == QStringLiteral("zh_CN"))
        actZh->setChecked(true);
    else if (cur == QStringLiteral("en_US"))
        actEn->setChecked(true);
    else
        actSys->setChecked(true);

    connect(actSys, &QAction::triggered, this,
            [this] { switchLanguage(QStringLiteral("system")); });
    connect(actZh, &QAction::triggered, this,
            [this] { switchLanguage(QStringLiteral("zh_CN")); });
    connect(actEn, &QAction::triggered, this,
            [this] { switchLanguage(QStringLiteral("en_US")); });

    root->addMenu(langMenu);
    tb->setMenuVisible(true);
}

void MainWindow::switchLanguage(const QString &code)
{
    // 选中当前项时不重启（避免"点一下当前语言却整个应用重启"的突兀行为）。
    QSettings s(QStringLiteral("deepin"), QStringLiteral("panshi-manager"));
    const QString cur =
        s.value(QStringLiteral("language"), QStringLiteral("system")).toString();
    if (cur == code)
        return;

    s.setValue(QStringLiteral("language"), code);
    s.sync();

    // 重启自身以完整加载目标语言：本应用 UI 全部代码内建，含大量 tr() 文本与
    // DTK 控件，不重启无法可靠地整体 retranslate。
    //
    // ⚠️ 必须**清掉继承来的 LANGUAGE/LANG/LC_ALL**：本进程启动时 main.cpp 已按
    //   旧偏好 setenv 过这几项；若不覆盖，子进程会先读到旧语言，菜单就"切不动"。
    //
    // ⚠️ 两条实测得到的硬约束（照抄，别简化）：
    //   ① LANG 必须带 codeset（`en_US.UTF-8`），写裸 `en_US` 会挑不到 .qm；
    //   ② LC_ALL 优先级最高，**必须清掉**，否则会盖掉 LANGUAGE 的选择。
    //
    // ⚠️ 用 qputenv + startDetached（不带 environment 参数）而不是
    //   startDetached(path, args, env)：本 Qt 的 QProcess::startDetached **没有**
    //   接受 QProcessEnvironment 的重载（实测编译报错），而 startDetached 会
    //   原样继承当前进程环境，故 qputenv 即可达到同效。
    qunsetenv("LC_ALL");
    if (code == QStringLiteral("system")) {
        qunsetenv("LANGUAGE");
        qunsetenv("LANG");
    } else {
        const QString locale = code == QStringLiteral("en_US")
                                   ? QStringLiteral("en_US.UTF-8")
                                   : QStringLiteral("zh_CN.UTF-8");
        qputenv("LANGUAGE", code.toUtf8());
        qputenv("LANG", locale.toUtf8());
    }
    const QString path = QApplication::applicationFilePath();
    QProcess::startDetached(path);
    qApp->quit();
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
