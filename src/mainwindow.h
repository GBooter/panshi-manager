// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <DMainWindow>

#include "ui/settingindex.h"

#include <QList>
#include <QMap>
#include <QDate>
#include <functional>
#include <dtkwidget_global.h>
DWIDGET_USE_NAMESPACE

class QStackedWidget;
class QFrame;
class QTimer;
class BasePage;
namespace PanshiUi { class PanshiSidebar; }

/**
 * 页面标识（UI 重构方案 §4.5）
 *
 * 背景：原实现使用裸下标 gotoPage(int) 与 m_pages.at(n)，在功能演进过程中
 * 页面顺序发生变化后出现了"跳转落地错误页"的缺陷。改为强类型枚举后，
 * 下标与页面不再耦合，编译期即可排除这类错误。
 */
enum class PageId
{
    Overview = 0,     // 总览
    DiskUsage,        // 磁盘空间
    BackupVersion,    // 备份与版本
    Protection,       // 系统保护
    Policy,           // 策略与设置
    Update,           // 系统更新
    About,            // 关于
};

/** 「备份与版本」页的子页 */
enum class SubPageId
{
    Snapshots = 0,    // 系统快照（可还原的备份点）
    Deploys,          // 系统部署实例（可启动的一整套系统）
};

class MainWindow : public DMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static MainWindow *instance() { return s_instance; }

    void gotoPage(PageId id);
    /** 跳转到「备份与版本」页并切换到指定子页 */
    void gotoPage(PageId id, SubPageId sub);

    /**
     * 跨页跳转 + 「到达后动作」（阶段 5，方案 §5.2 第 10 条）。
     *
     * 背景：总览页的快速操作只做"跳页"，用户到了目标页还得自己再找一次按钮
     * （总览「立即创建备份」→ 备份页 → 再点「创建备份」）。语义上这是一次
     * 动作，不该拆成两次点击。
     *
     * 为什么不能在 connect 里直接连着调用：
     *   switchTo() 里的 refresh() 是**异步**的（runAsync / runAsyncQuiet）。
     *   若跳页后立即弹创建对话框，会踩两处：
     *     1. 对话框展示的是尚未刷新的旧数据；
     *     2. 目标页动作入口普遍带 `if (m_running) return;` 防重入守卫，
     *        refresh 在途时动作被**静默丢弃**——用户看到"点了没反应"。
     *   因此本方法把动作挂到目标页的 idleReached() 上，等 refresh 真正结束
     *   再执行；并用 kArrivalTimeoutMs 兜底，目标页长时间不空闲（如后端卡死）
     *   也会放行动作，不让用户永远停在"没有反应"的状态。
     *
     * 同一时刻只保留一个待执行动作：后到的动作会替换先到的（用户最后一次
     * 点击最符合其当前意图）。
     */
    void gotoPage(PageId id, std::function<void()> afterArrive);

    /**
     * 跳转到指定子页 + 到达后动作。
     * 子页切换必须在 switchTo() 之前完成，否则 refresh() 刷到的是旧子页，
     * 随后到达动作会作用在一个尚未加载的子页上。
     */
    void gotoPage(PageId id, SubPageId sub, std::function<void()> afterArrive);

    /** 「磐石功能」开关切换成功后，同步刷新总览页与系统保护页 */
    void refreshPanshiPages();
    /** 通知"查看详情"定位：跳转磁盘占用页并选中指定日期 */
    void showDiskDate(const QDate &date);

    /**
     * 设置搜索索引（供页面在构造时注册条目；阶段 4）
     *
     * ⚠️ 2026-10-06 移除侧栏搜索后已无调用者，**故意保留**：
     * 恢复搜索功能时需要它（页面侧注册入口）。删除前先确认不再恢复搜索。
     */
    PanshiUi::SettingIndex *settingIndex() { return &m_settingIndex; }

    /**
     * 按 PageId 取页面（阶段 5 跨页跳转需要：动作常要转交给目标页执行）。
     * 未注册返回 nullptr。
     */
    BasePage *page(PageId id) const;

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void initSidebar();
    void initContent();

    /** 标题栏菜单加入「语言」子菜单（中/英/跟随系统），切换即重启生效 */
    void setupLanguageMenu();
    /** 写入语言偏好并重启自身以加载目标语言 */
    void switchLanguage(const QString &code);

    /**
     * 设置搜索落地：切到目标页 → 让页面按 targetKey 定位并高亮该行。
     *
     * 为什么让页面自己定位：索引只记「标题」，不持有控件指针（避免页面重建后
     * 悬垂）。页面用 PanshiRow::title() 自行比对，是本项目「注册点」范式的延续。
     *
     * ⚠️ 2026-10-06 移除侧栏搜索后已无调用者，**故意保留**（恢复搜索时接回）。
     */
    void gotoSetting(int pageId, const QString &targetKey);

    /** PageId → QStackedWidget 下标 */
    int toIndex(PageId id) const;
    /** 切换页面工作：更新栈/导航/刷新（不含子页切换） */
    void switchTo(PageId id);

    /**
     * 挂起一个「到达后动作」并在切换完成后执行。
     * switchTo() 之后调用；等目标页空闲或超时即执行。
     */
    void scheduleArrivalAction(PageId id, std::function<void()> action);
    /** 执行/丢弃当前挂起的到达动作（run = true 表示执行） */
    void flushArrivalAction(bool run);

    /** 到达动作等待上限：目标页迟迟不空闲（后端阻塞）也不让用户无从下手 */
    static constexpr int kArrivalTimeoutMs = 4000;

    QFrame *m_sidebarWidget = nullptr;
    QStackedWidget *m_stack = nullptr;
    PanshiUi::PanshiSidebar *m_navSidebar = nullptr;
    QMap<PageId, BasePage *> m_pages;
    PanshiUi::SettingIndex m_settingIndex;

    /** 待执行的「到达后动作」；空表示无挂起动作 */
    std::function<void()> m_arrivalAction;
    /** 兜底定时器：超时后强制执行挂起动作 */
    QTimer *m_arrivalTimer = nullptr;
    /**
     * 正在等待空闲的页面。
     * 用 PageId 而不是指针：页面是 m_stack 的子控件、生命周期不早于本窗口，
     * 但用枚举做键可以让"是否为当前挂起目标"的判定与指针有效性彻底解耦
     * （避免 QPointer 给仅有前置声明的 BasePage 带来的不完整类型问题）。
     */
    PageId m_arrivalPageId = PageId::Overview;
    /** 是否有挂起目标（PageId 无"空值"，另用一个标志位表达） */
    bool m_arrivalPending = false;

    inline static MainWindow *s_instance = nullptr;
};
