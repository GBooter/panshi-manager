// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QFutureWatcher>
#include <QVariant>
#include <QtConcurrent>
#include <functional>
#include <dtkwidget_global.h>
DWIDGET_USE_NAMESPACE

#include "ui/components.h"
#include "ui/settingindex.h"

#include <DSpinner>
#include <QHash>
#include <QScrollArea>
class QLabel;

/**
 * 页面基类：提供后台任务执行 + 加载指示
 */
class BasePage : public QWidget
{
    Q_OBJECT
public:
    explicit BasePage(QWidget *parent = nullptr);

    virtual void refresh() {}
    virtual void shutdown() {}

    /**
     * 页面后台任务全部结束（在途计数归零）时发出（阶段 5「到达后动作」）。
     *
     * 为什么需要它：跨页跳转携带动作（如「立即创建备份」→ 跳备份页并直接开
     * 创建对话框）必须在目标页 refresh() **完成之后**执行——否则：
     *   · 对话框会在数据尚未就绪时打开，用户看到的是上一轮的旧数据；
     *   · 更关键的是，多数页面的动作入口带 `if (m_running) return;` 防重入
     *     守卫，refresh 在途时点开会**被静默丢弃**（只留一条"上一项操作尚未
     *     完成"的提示），用户认知里就是"点了没反应"。
     *
     * 由基类在 runAsync / runAsyncQuiet 层统一计数，页面无需各自上报——
     * 与本项目 registerResponsiveCardRow / setFeedbackBanner 同一"注册点"思路。
     */
signals:
    void idleReached();

public:
    /**
     * 当前是否有在途后台任务（runAsync / runAsyncQuiet 共用计数）。
     *
     * virtual：复合页（如 VersionManagementPage 把 refresh 转发给子页）的在途
     * 任务落在子页身上，本页计数恒为 0，必须由复合页覆写并把子页状态聚合上来，
     * 否则「到达后动作」会在子页数据未就绪时提前执行。
     */
    virtual bool isBusy() const { return m_inFlight > 0; }

    /**
     * 在后台线程执行 fn，完成后在主线程调用 onDone(result)
     */
    template <typename Fn>
    void runAsync(Fn fn, std::function<void(const QVariant &)> onDone)
    {
        auto *watcher = new QFutureWatcher<QVariant>(this);
        beginTask();
        connect(watcher, &QFutureWatcher<QVariant>::finished, this,
                [this, watcher, onDone] {
                    const QVariant res = watcher->result();
                    watcher->deleteLater();
                    hideLoading();
                    // 顺序有意为之：先 onDone 再 endTask。
                    // 部分页面在 onDone 里接着发起第二段任务（总览页 = 快速状态 → 慢速统计），
                    // 那时 beginTask 会先把计数抬到 2，再 endTask 落回 1 —— 计数不归零，
                    // 就不会误发 idleReached() 让「到达后动作」在半程就执行。
                    onDone(res);
                    endTask();
                });
        if (m_maskEnabled)
            showLoading(m_loadingText);
        watcher->setFuture(QtConcurrent::run([fn]() -> QVariant {
            return QVariant::fromValue(fn());
        }));
    }

    void showLoading(const QString &text = QString());
    void hideLoading();

    /**
     * 页面自带 L2 局部加载控件时关闭整页遮罩（加载态三档分级，方案 §4.6）。
     * 关闭后 runAsync 不再调用 showLoading，遮罩逻辑保留给未迁移页面。
     */
    void setMaskLoadingEnabled(bool on) { m_maskEnabled = on; }

    /**
     * 静默执行：与 runAsync 相同，但完全不碰整页遮罩。
     *
     * 用于「切换标签页时的预读」——这类刷新紧跟在用户点击之后，此时再弹一次
     * 灰屏遮罩会在标签切换动画之上叠加一层闪烁，观感上像"闪两次灰屏"。
     * 页面若已有 L2 局部加载控件，应由调用方自行 start/stop，本方法不介入。
     */
    template <typename Fn>
    void runAsyncQuiet(Fn fn, std::function<void(const QVariant &)> onDone)
    {
        auto *watcher = new QFutureWatcher<QVariant>(this);
        beginTask();
        connect(watcher, &QFutureWatcher<QVariant>::finished, this,
                [this, watcher, onDone] {
                    const QVariant res = watcher->result();
                    watcher->deleteLater();
                    // 同 runAsync：先 onDone 再 endTask，容纳"完成回调里再起一段"的页面
                    onDone(res);
                    endTask();
                });
        watcher->setFuture(QtConcurrent::run([fn]() -> QVariant {
            return QVariant::fromValue(fn());
        }));
    }

    // ------------------------------------------------ 页面级反馈（阶段 4）
    /**
     * 注册页面的反馈 Banner 宿主。
     *
     * 为什么放在基类：操作结果的反馈（成功/失败）此前由 Ui::infoDialog /
     * Ui::errorDialog 弹**模态对话框**给出——用户必须再点一次「确定」才能
     * 继续操作，且对话框会盖住刚被改动的界面。统一为页内 Banner 后，
     * 反馈与操作对象同屏可见。
     *
     * 共享逻辑（如 PanshiSwitch::requestSwitch）只拿到 `BasePage *`，
     * 不认识各页内部结构，因此由页面在构造时把自带的 Banner 注册进来。
     * 未注册（banner == nullptr 或从未调用）时反馈被静默跳过。
     */
    void setFeedbackBanner(PanshiUi::PanshiInlineBanner *banner) { m_feedback = banner; }

    /**
     * 显示一条操作结果：成功走 Success + 可关闭 + 自动消失，
     * 失败走 Danger + 可关闭 + 常驻（需用户确认已读）。
     * 无已注册 Banner 时静默跳过（不阻断调用方流程）。
     *
     * public：共享逻辑 PanshiSwitch::requestSwitch 只拿到 `BasePage *`，
     * 需要从外部回写执行结果（阶段 4 编译修正，原为 protected）。
     */
    void showFeedback(PanshiUi::Semantic s, const QString &text, bool autoHide = false);

    /**
     * 同上，但把后端原始输出收进 Banner 内的「详情」折叠块（方案 §4.4）。
     *
     * 为什么不直接拼进 text：后端失败时 errorMessage 常是多行原始输出
     * （ostree / deepin-immutable-ctl 的 stderr），拼进正文会让 Banner 变成一大坨日志，
     * 用户读不到那句可行动的说明。
     *
     * @param text       一行可行动说明（如「设置失败，请检查是否已在 sudoers 组中」）
     * @param detailsTitle 折叠块标题（走 tr，如「完整输出」）
     * @param rawText    原始输出；空串则退化为普通 showFeedback
     */
    void showFeedbackWithDetails(PanshiUi::Semantic s, const QString &text,
                                 const QString &detailsTitle, const QString &rawText,
                                 bool autoHide = false);

    // ------------------------------------------------ 设置搜索（阶段 4）
    /**
     * 注册一个可被设置搜索定位到的行。
     *
     * 页面在构造时把每个 PanshiRow / PanshiSwitchRow 交给基类；基类据此
     * 实现 highlightSetting(key)：按 key 找到控件 → 滚动到可见 → 调其
     * flashHighlight()。
     *
     * 为什么由基类做定位：搜索落地需要「滚动到该行」——滚动容器由页面自己
     * 创建（Ui::makeScrollArea），只有页面知道哪个是滚动区。注册一次即可
     * 让基类同时拿到「控件」与「所在滚动区」，页面不必各自实现定位逻辑。
     */
    void registerSearchTarget(const QString &key, QWidget *row, QScrollArea *scrollArea = nullptr);

    /** 设置搜索落地调用：定位到 key 对应的行并高亮；无匹配返回 false */
    bool highlightSetting(const QString &key);

    /**
     * 页面自报可搜索项（供 MainWindow 构造 SettingIndex 时使用）。
     * 默认返回空；子类按需覆盖。
     */
    virtual QList<PanshiUi::SettingEntry> searchableSettings() const { return {}; }

protected:
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *e) override;
    void setLoadingText(const QString &text) { m_loadingText = text; }

    /**
     * 注册「响应式卡片行」并让基类负责喂入可用宽度。
     *
     * 为什么放在基类（阶段 3-8 实测教训）：
     *  1. **触发时机的坑**：本应用页面都在 QStackedWidget 里，隐藏页**不跟随
     *     窗口 resize**——首次 show 时页面的 resizeEvent 采到的往往是尚未布局的
     *     尺寸（实测 viewport ≈100px），于是卡片行算出 1 列并**粘住**（之后切页
     *     不再触发 resizeEvent）。保护页实测：3 卡首次进入竖排单列，
     *     手动拖一次窗口才弹回 3 列。
     *  2. **口径统一**：可用宽度应取滚动区 viewport 宽度（自动扣除竖向滚动条
     *     与侧边栏），而不是页面宽度减去若干常量的估算。
     *  3. 三页（总览 / 保护 / 磁盘）若各写一份，就是第三次复制——
     *     按本项目惯例（ElidedLabel 的提升）应在复制前收敛。
     *
     * 基类同时监听 viewport 的 Resize 与自身的 showEvent，
     * 保证「首次显示」与「窗口缩放」两条路径都会重算。
     *
     * @param row         卡片行控件
     * @param scrollArea  该页内容所在的滚动区（用于取 viewport 宽度）
     * @param hPadding    内容区左右内边距合计（卡片行两侧的留白）
     */
    void registerResponsiveCardRow(PanshiUi::PanshiResponsiveCardRow *row,
                                   QScrollArea *scrollArea, int hPadding);

private:
    void feedResponsiveCardRow();   // 按当前视口宽度喂给已注册的卡片行
    /** 在途任务计数 +1（runAsync / runAsyncQuiet 进入时调用） */
    void beginTask();
    /** 在途任务计数 -1，归零时发 idleReached() */
    void endTask();

    QWidget *m_mask = nullptr;
    DSpinner *m_spinner = nullptr;
    QLabel *m_maskLabel = nullptr;
    QString m_loadingText;
    bool m_maskEnabled = true;
    /** 在途后台任务数（含 runAsync 与 runAsyncQuiet）；归零即页面对外"空闲" */
    int m_inFlight = 0;

    PanshiUi::PanshiResponsiveCardRow *m_cardRow = nullptr;
    QScrollArea *m_cardRowScroll = nullptr;
    int m_cardRowPadding = 0;

    /** 页面级反馈宿主（由子类注册；未注册为 nullptr） */
    PanshiUi::PanshiInlineBanner *m_feedback = nullptr;

    /** 设置搜索定位表：key → (行控件, 所在滚动区) */
    struct SearchTarget
    {
        QWidget *row = nullptr;
        QScrollArea *scroll = nullptr;
    };
    QHash<QString, SearchTarget> m_searchTargets;
};
