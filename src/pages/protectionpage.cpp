// SPDX-License-Identifier: GPL-3.0-or-later
#include "protectionpage.h"
#include "common.h"
#include "masterswitch.h"
#include "protectioncards.h"
#include "../backend.h"
#include "../mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariantMap>

DWIDGET_USE_NAMESPACE

using namespace Panshi;
using namespace PanshiSwitch;
using namespace PanshiUi;

ProtectionPage::ProtectionPage(MainWindow *win)
    : BasePage(win)
    , m_win(win)
{
    // 内容放入滚动容器，窗口尺寸不足时可滚动，避免底部内容被裁剪
    auto *scroll = Ui::makeScrollArea(this);
    m_scroll = scroll;   // 跨页跳转定位高亮时需要它把目标行滚入视野
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(scroll);

    auto *lay = new QVBoxLayout(scrollContent);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(Space::L);

    lay->addWidget(Ui::pageHeader(tr("系统保护"), tr("磐石系统只读保护(可写层)管理"),
                                 QStringLiteral("security")));

    // ---- 反馈条 / 局部加载 ----
    m_banner = new PanshiInlineBanner(scrollContent);
    m_banner->hide();
    lay->addWidget(m_banner);
    // 注册给基类：磐石开关切换（PanshiSwitch::requestSwitch）的结果也写到这里，
    // 与 setProtect() 共用同一条反馈位（阶段 4）
    setFeedbackBanner(m_banner);

    m_busy = new PanshiLocalLoading(QString(), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // ---- 状态区（响应式折行；3 列预算 528px，窄窗下同样不会溢出，
    //      但统一走组件层容器，避免将来增卡或改宽度时重蹈总览页的覆辙）----
    m_statRow = new PanshiResponsiveCardRow(3, scrollContent);
    // 标题也取自 protectioncards.h：若标题只写在页面里，探针就无法按文案定位
    // 真实卡片的标签（它靠标题文本匹配），标题会成为唯一分叉点。
    m_cardProtect = new PanshiStateCard(PanshiProtection::protectCardTitle(), m_statRow);
    m_cardWritable = new PanshiStateCard(PanshiProtection::writableCardTitle(), m_statRow);
    m_cardReboot = new PanshiStateCard(PanshiProtection::rebootCardTitle(), m_statRow);
    m_statRow->addCard(m_cardProtect);
    m_statRow->addCard(m_cardWritable);
    m_statRow->addCard(m_cardReboot);
    lay->addWidget(m_statRow);
    // 宽度喂入交给 BasePage（viewport Resize + showEvent 双路径）。
    // 本页此前用 resizeEvent 单路径 —— 隐藏页首次 show 采到未布局的尺寸，
    // 3 卡会粘在 1 列（3-8 实测），交基类后免疫。
    registerResponsiveCardRow(m_statRow, scroll, 24 * 2);

    // ---- 说明 ----
    auto *secInfo = new PanshiSection(tr("说明"), QString(), scrollContent);
    auto *desc = makeLabel(
        tr("磐石系统将核心目录(/usr 等)以只读方式挂载, 防止误操作或恶意软件破坏系统。\n"
           "开启“可写层”将解除只读保护(适合开发调试), 关闭后恢复保护。\n"
           "注意: 开启可写层后系统将拒绝执行快照还原。"),
        TextLevel::Desc, Semantic::Normal);
    desc->setWordWrap(true);
    secInfo->content()->addWidget(desc);
    lay->addWidget(secInfo);

    // ---- 保护开关 ----
    auto *secSwitch = new PanshiSection(tr("保护开关"), tr("永久开启或关闭磐石系统保护"),
                                        scrollContent);

    // 「磐石功能」同步开关行：与总览页状态互相同步
    m_switchRow = new PanshiSwitchRow(this, false, secSwitch);
    connect(m_switchRow, &PanshiSwitchRow::switchChanged, this, [this] { m_win->refreshPanshiPages(); });
    secSwitch->content()->addWidget(m_switchRow);

    // 常规操作平铺
    m_rowEnable = new PanshiRow(secSwitch);
    m_rowEnable->setTitle(tr("开启保护 (恢复只读)"));
    m_rowEnable->setDescription(tr("恢复核心目录只读挂载, 需要重启系统后生效"));
    auto *bEnable = Ui::button(tr("开启保护"), true);
    connect(bEnable, &DPushButton::clicked, this, [this] { setProtect(true, false); });
    m_rowEnable->addControl(bEnable);
    secSwitch->content()->addWidget(m_rowEnable);

    // 危险操作收进折叠区（默认收起，方案 §3.3）
    auto *dangerBox = new PanshiCollapsible(tr("关闭保护（高级操作）"), secSwitch);
    auto *dangerHost = new QWidget(dangerBox);
    auto *dangerLay = new QVBoxLayout(dangerHost);
    dangerLay->setContentsMargins(0, 0, 0, 0);
    dangerLay->setSpacing(Space::S);

    auto *warn = makeLabel(tr("关闭保护后核心目录将可写, 且系统拒绝执行快照还原。请仅在开发调试时短期使用。"),
                           TextLevel::Desc, Semantic::Warning);
    warn->setWordWrap(true);
    dangerLay->addWidget(warn);

    auto *rPerm = new PanshiRow(dangerHost);
    rPerm->setTitle(tr("永久关闭保护 (开启可写层)"));
    rPerm->setDescription(tr("重启后仍保持可写, 需手动再次开启保护"));
    auto *bPerm = Ui::button(tr("永久关闭保护"), false, true);
    connect(bPerm, &DPushButton::clicked, this, [this] { setProtect(false, false); });
    rPerm->addControl(bPerm);
    dangerLay->addWidget(rPerm);

    auto *rTmp = new PanshiRow(dangerHost);
    rTmp->setTitle(tr("临时关闭保护 (重启后自动恢复)"));
    rTmp->setDescription(tr("仅本次开机生效, 重启后自动恢复只读保护"));
    auto *bTmp = Ui::button(tr("临时关闭保护"), false, true);
    connect(bTmp, &DPushButton::clicked, this, [this] { setProtect(false, true); });
    rTmp->addControl(bTmp);
    dangerLay->addWidget(rTmp);

    dangerBox->setContent(dangerHost);
    secSwitch->content()->addWidget(dangerBox);

    lay->addWidget(secSwitch);
    lay->addStretch();

    // ---- 设置搜索（阶段 4）：登记「磐石功能」开关行 ----
    // 它在总览页也出现，但搜索一律落到本页（此处才是完整的管理入口）。
    registerSearchTarget(tr("磐石功能"), m_switchRow, scroll);
}

QList<PanshiUi::SettingEntry> ProtectionPage::searchableSettings() const
{
    QList<PanshiUi::SettingEntry> out;
    if (!m_switchRow)
        return out;

    PanshiUi::SettingEntry e;
    e.pageId = static_cast<int>(PageId::Protection);
    e.group = tr("保护开关");
    e.title = tr("磐石功能");
    e.description = tr("总开关：控制磐石系统保护的启用状态");
    e.targetKey = e.title;
    out.append(e);
    return out;
}

void ProtectionPage::highlightEnableProtect()
{
    if (!m_rowEnable)
        return;
    // 折叠区内的危险操作不在此列——「开启保护」是常规操作，始终平铺可见，
    // 因此不需要先展开折叠区；只需把行滚入视野并闪一下。
    if (m_scroll && m_scroll->viewport())
        m_scroll->ensureWidgetVisible(m_rowEnable, Space::M, Space::M);
    m_rowEnable->flashHighlight();
}

void ProtectionPage::refresh()
{
    m_busy->setText(tr("正在读取保护状态..."));
    m_busy->start();
    setMaskLoadingEnabled(false);   // 快任务走 L2 局部加载，不盖整页
    runAsync([]() -> QVariant {
        return QVariant::fromValue(getWritableStatus());
    }, [this](const QVariant &res) { onLoaded(res); });
}

void ProtectionPage::onLoaded(const QVariant &res)
{
    m_busy->stop();

    const WritableStatus w = res.value<WritableStatus>();

    // 同步磐石功能开关行状态
    if (m_switchRow)
        m_switchRow->applyStatus(presentStatus(w));

    // 三张卡的文案与语义色全部走 protectioncards.h —— 与 panshi-protectionprobe
    // **共用同一份定义**，这样探针断言的正是页面真正在用的映射（设计说明见该文件头注释）。
    //
    // ⚠️ 本页「只读保护是否生效」由 !w.enable 推导；概览页用 isImmutable()。
    //    两者是不同信号（ctl status -j 的 conf.enable vs ctl -s 的 Immutable mode），
    //    且失败行为相反（fail-open vs fail-closed），**刻意不合并** ——
    //    合并会在后端出错时产出「只读保护=已开启 且 可写层=已解锁」这类自相矛盾的卡片。
    PanshiProtection::applyCard(m_cardProtect, PanshiProtection::protectCard(!w.enable));
    PanshiProtection::applyCard(m_cardWritable, PanshiProtection::writableCard(w.enable));
    PanshiProtection::applyCard(m_cardReboot, PanshiProtection::rebootCard(w));
}

void ProtectionPage::setProtect(bool protect, bool clearAfterReboot)
{
    if (m_running)
        return;
    const QString action = protect ? tr("开启只读保护") : tr("关闭只读保护(开启可写层)");
    QString extra;
    if (!protect && clearAfterReboot)
        extra = tr("\n\n本次为临时关闭, 重启后系统将自动恢复只读保护。");
    if (!Ui::confirmDialog(this, action,
                       tr("确定要%1吗？\n此操作需要重启后生效。%2").arg(action, extra),
                       action, !protect))
        return;

    m_running = true;
    m_busy->setText(tr("正在设置保护状态..."));
    m_busy->start();
    setMaskLoadingEnabled(false);
    runAsync([protect, clearAfterReboot]() -> QVariant {
        const CommandResult r = protect ? disableWritable()
                                        : enableWritable(clearAfterReboot, true);
        return QVariantMap{{QStringLiteral("ok"), r.ok}, {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            // 走基类统一反馈（Success + 可关闭 + 5s 自动消失）
            showFeedback(Semantic::Success, tr("设置成功，请重启系统使设置生效。"), true);
        } else {
            // 原始输出（ostree/deepin-immutable-ctl 的 stderr）收进「详情」，
            // 正文只留一句可行动说明（方案 §4.4 / O11）
            showFeedbackWithDetails(Semantic::Danger,
                                    tr("设置失败，请查看详情了解具体原因。"),
                                    tr("完整输出"),
                                    m.value(QStringLiteral("msg")).toString());
        }
        refresh();
    });
}
