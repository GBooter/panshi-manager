// SPDX-License-Identifier: GPL-3.0-or-later
#include "overviewpage.h"
#include "common.h"
#include "masterswitch.h"
#include "../mainwindow.h"
#include "../backend.h"
#include "protectioncards.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QVariantMap>

DWIDGET_USE_NAMESPACE

using namespace Panshi;
using namespace PanshiSwitch;
using namespace PanshiUi;

OverviewPage::OverviewPage(MainWindow *win)
    : BasePage(win)
    , m_win(win)
{
    // 内容放入滚动容器，窗口尺寸不足时可滚动，避免底部内容被裁剪
    auto *scroll = Ui::makeScrollArea(this);
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(scroll);

    auto *lay = new QVBoxLayout(scrollContent);
    lay->setContentsMargins(Space::XL, 20, Space::XL, 20);
    lay->setSpacing(Space::L);

    lay->addWidget(Ui::pageHeader(tr("总览"), tr("磐石系统运行状态一览"),
                                  QStringLiteral("utilities-system-monitor")));

    // ---- L2 局部加载：本页分两步取数，但永不盖整页遮罩（见 refresh()）----
    m_busy = new PanshiLocalLoading(QString(), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // ---- 页级反馈条：开关切换的结果写在这里（阶段 4，替代模态 info/error 对话框）----
    m_feedback = new PanshiInlineBanner(scrollContent);
    m_feedback->hide();
    lay->addWidget(m_feedback);
    setFeedbackBanner(m_feedback);

    // ---- 磐石功能总开关 ----
    auto *secSwitch = new PanshiSection(tr("磐石功能"),
                                        tr("总开关：控制磐石系统保护的启用状态"), scrollContent);
    m_switchRow = new PanshiSwitchRow(this, true, secSwitch);
    connect(m_switchRow, &PanshiSwitchRow::switchChanged, this,
            [this] { m_win->refreshPanshiPages(); });
    connect(m_switchRow, &PanshiSwitchRow::detailRequested, this,
            [this] { m_win->gotoPage(PageId::Protection); });
    secSwitch->content()->addWidget(m_switchRow);
    lay->addWidget(secSwitch);

    // ---- 状态卡：响应式列数（PanshiResponsiveCardRow）。
    //     旧实现是固定 4 列 QHBoxLayout —— 窄窗下 4 卡硬挤后整行横向溢出，
    //     被外层 makeScrollArea（ScrollBarAlwaysOff）静默裁掉，第 4 卡与
    //     行内开关因此不可达。容器按可用宽度自动折行，永不溢出。
    m_statRow = new PanshiResponsiveCardRow(4, scrollContent);
    m_cardProtect = new PanshiStateCard(tr("磐石保护"), m_statRow);
    m_cardWritable = new PanshiStateCard(tr("系统可写层"), m_statRow);
    m_cardSnap = new PanshiStateCard(tr("备份快照"), m_statRow);
    m_cardFree = new PanshiStateCard(tr("系统盘剩余"), m_statRow);
    m_statRow->addCard(m_cardProtect);
    m_statRow->addCard(m_cardWritable);
    m_statRow->addCard(m_cardSnap);
    m_statRow->addCard(m_cardFree);
    lay->addWidget(m_statRow);
    // 宽度喂入交给 BasePage：它同时监听 viewport Resize 与 showEvent，
    // 覆盖「窗口缩放」与「隐藏页首次 show」两条路径（后者是 3-8 踩到的坑）。
    registerResponsiveCardRow(m_statRow, scroll, Space::XL * 2);

    // L1 骨架屏：占位在同一位置，与状态卡互斥显示
    m_skeleton = new PanshiSkeleton(scrollContent);
    m_skeleton->setRows(3);
    m_skeleton->hide();
    lay->addWidget(m_skeleton);

    // ---- 系统信息 ----
    auto *secSys = new PanshiSection(tr("系统信息"), tr("当前运行的系统与内核"), scrollContent);
    auto *g1 = new QGridLayout;
    g1->setHorizontalSpacing(Space::XL);
    g1->setVerticalSpacing(Space::S);
    g1->addWidget(makeInfoRow(tr("系统名称"), &m_valSys), 0, 0);
    g1->addWidget(makeInfoRow(tr("系统版本"), &m_valVer), 0, 1);
    g1->addWidget(makeInfoRow(tr("内核版本"), &m_valKernel), 1, 0);
    g1->addWidget(makeInfoRow(tr("磐石部署"), &m_valDeploy), 1, 1);
    g1->setColumnStretch(0, 1);
    g1->setColumnStretch(1, 1);
    secSys->content()->addLayout(g1);
    lay->addWidget(secSys);

    // ---- 存储空间 ----
    auto *secStore = new PanshiSection(tr("存储空间"), tr("磐石系统各仓库占用（物理）"),
                                       scrollContent);
    auto *g2 = new QGridLayout;
    g2->setHorizontalSpacing(Space::XL);
    g2->setVerticalSpacing(Space::S);
    g2->addWidget(makeInfoRow(tr("系统仓库 /ostree"), &m_valRepo), 0, 0);
    g2->addWidget(makeInfoRow(tr("数据仓库 /persistent"), &m_valPrepo), 0, 1);
    g2->addWidget(makeInfoRow(tr("快照数据"), &m_valSnap), 1, 0);
    g2->addWidget(makeInfoRow(tr("系统盘可用"), &m_valFree), 1, 1);
    g2->setColumnStretch(0, 1);
    g2->setColumnStretch(1, 1);
    secStore->content()->addLayout(g2);
    lay->addWidget(secStore);

    // ---- 快速操作 ----
    // 三个入口都是「跳页 + 到达后动作」：光跳页会让用户到了目标页还得再找一次
    // 按钮，语义上这是一次动作。动作挂到目标页 idleReached() 上，等 refresh()
    // 结束再执行（详见 MainWindow::gotoPage(PageId, afterArrive) 的说明）。
    auto *secQuick = new PanshiSection(tr("快速操作"), tr("常用功能快捷入口"), scrollContent);
    auto *btns = new QHBoxLayout;
    btns->setSpacing(Space::M);
    auto *bBackup = Ui::button(tr("立即创建备份"), true);
    connect(bBackup, &DPushButton::clicked, this, [this] {
        // 落到「备份快照」子页后直接打开创建对话框（合成一次动作，用户无需再点）。
        // 三个动作都走 QMetaObject::invokeMethod：总览页不持有目标页头文件，
        // 且这些入口是各页的私有实现，转发槽是它们对外唯一的动作契约。
        m_win->gotoPage(PageId::BackupVersion, SubPageId::Snapshots, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::BackupVersion),
                                      "requestCreateSnapshot", Qt::DirectConnection);
        });
    });
    auto *bUpdate = Ui::button(tr("检查系统更新"));
    connect(bUpdate, &DPushButton::clicked, this, [this] {
        m_win->gotoPage(PageId::Update, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::Update), "requestCheckUpdate",
                                      Qt::DirectConnection);
        });
    });
    auto *bProtect = Ui::button(tr("保护设置"));
    connect(bProtect, &DPushButton::clicked, this, [this] {
        // 跳保护页并定位高亮「开启保护（恢复只读）」行：总览的语境是
        // "保护可能没开，去看看"，直接指出该动哪一行比让用户自己找更省事。
        m_win->gotoPage(PageId::Protection, [this] {
            QMetaObject::invokeMethod(m_win->page(PageId::Protection), "highlightEnableProtect",
                                      Qt::DirectConnection);
        });
    });
    btns->addWidget(bBackup);
    btns->addWidget(bUpdate);
    btns->addWidget(bProtect);
    btns->addStretch();
    secQuick->content()->addLayout(btns);
    lay->addWidget(secQuick);

    lay->addStretch();
}

PanshiRow *OverviewPage::makeInfoRow(const QString &label, DLabel **valueOut)
{
    auto *row = new PanshiRow;
    row->setTitle(label);
    // 值标签用 ElidedLabel（中部省略，永不撑破行）+ 显式 minW=0：
    // 纯文本值的退化是优雅的（省略 + 悬停看全文），不需要交互控件那 200 的地板。
    //
    // 为何关键：本页「系统信息 / 存储空间」是 2 列 × 2 行的 PanshiRow 网格。
    // 若值标签带 200 地板且不折行，单行最小宽约 346px、两列合计 806px——
    // 会把 scrollContent 的最小宽顶到 806，窄窗下被外层 makeScrollArea
    // （ScrollBarAlwaysOff）静默裁剪，连累第 3/4 张状态卡与「磐石功能」开关
    // （阶段 3-8 实测：viewport=720 / scrollContent=806 / 裁掉 86px）。
    // 去掉地板 + 可省略后，整页最小宽回落到卡片行的 528px。
    auto *v = new Ui::ElidedLabel(QStringLiteral("-"));
    v->setForegroundRole(DPalette::TextTitle);
    DFontSizeManager::instance()->bind(v, DFontSizeManager::T5);
    row->addControl(v, 0);
    if (valueOut)
        *valueOut = v;
    return row;
}

void OverviewPage::refresh()
{
    // 本页分两步取数（快速状态 + 慢速存储统计）。旧实现对每一步各盖一层全页遮罩，
    // 于是切到总览页会"连闪两次灰屏"（问题清单：加载态 I-01）。
    // 新策略（重构方案 §4.5 三档加载）：
    //   首次进入 → L1 骨架屏占位；之后 → 一律 L2 局部加载，内容始终可见可交互。
    setMaskLoadingEnabled(false);

    if (m_firstLoad) {
        m_statRow->hide();
        m_skeleton->show();
        m_skeleton->startPulse();
    }

    m_busy->setText(tr("正在读取磐石系统状态…"));
    m_busy->start();

    runAsync([]() -> QVariant {
        QVariantMap d;
        d.insert(QStringLiteral("immutable"), isImmutable());
        d.insert(QStringLiteral("os"), getOsVersion());
        d.insert(QStringLiteral("deploy"), getDeployVersion());
        d.insert(QStringLiteral("kernel"), getKernel());
        d.insert(QStringLiteral("writable"), QVariant::fromValue(getWritableStatus()));
        d.insert(QStringLiteral("snaps"), QVariant::fromValue(getSnapshots()));
        return d;
    }, [this](const QVariant &res) { onLoadedFast(res); });
}

void OverviewPage::onLoadedFast(const QVariant &res)
{
    const QVariantMap d = res.toMap();
    const bool immutable = d.value(QStringLiteral("immutable")).toBool();
    const WritableStatus w = d.value(QStringLiteral("writable")).value<WritableStatus>();
    m_snaps = d.value(QStringLiteral("snaps")).value<QList<SnapshotInfo>>();
    const QJsonObject os = d.value(QStringLiteral("os")).toJsonObject();

    // 首次加载完成：骨架屏退场、状态卡入场（此后刷新不再出现骨架屏，避免闪屏）
    if (m_firstLoad) {
        m_firstLoad = false;
        m_skeleton->stopPulse();
        m_skeleton->hide();
        m_statRow->show();
    }

    // 同步磐石功能开关行状态
    if (m_switchRow)
        m_switchRow->applyStatus(presentStatus(w));

    // 状态卡：颜色只表达状态（正常/注意/危险），不再用 4 种装饰色区分卡片。
    //
    // 卡 1/卡 2 的**文案与语义色**走 protectioncards.h，与系统保护页共用同一份映射
    // （此前两页各内联一份，且已经漂移过：概览页写「开启/关闭」、保护页写「已开启/已关闭」）。
    //
    // ⚠️ 但**取数口径不同且刻意不合并**：本页「只读保护」用 isImmutable()
    //    （ctl -s 的 Immutable mode，**fail-closed**：出错返回 true），系统保护页用
    //    !w.enable（ctl status -j 的 conf.enable，**fail-open**：出错返回 false）。
    //    两者是不同命令、不同维度、失败行为相反 —— 合并会在后端出错时产出
    //    「只读保护=已开启 且 可写层=已解锁」这类自相矛盾的卡片。详见 protectioncards.h 头注释。
    //
    // ⚠️ 本页卡片**标题**沿用概览页自己的措辞（磐石保护 / 系统可写层），
    //    不取 protectioncards.h 的（只读保护 / 可写层）—— 标题属各页文案决策，不共享。
    PanshiProtection::applyCard(m_cardProtect, PanshiProtection::protectCard(immutable));
    PanshiProtection::applyCard(m_cardWritable, PanshiProtection::writableCard(w.enable));

    m_cardSnap->setValue(tr("%1 个 · 计算中…").arg(m_snaps.size()), Semantic::Normal);
    m_cardSnap->setStatus(QString());           // 无状态语义时不显示徽章
    m_cardFree->setValue(tr("统计中…"), Semantic::Muted);
    m_cardFree->setStatus(QString());

    m_valSys->setText(os.value(QStringLiteral("system")).toString());
    const QString ver = os.value(QStringLiteral("version")).toString() + QLatin1Char(' ')
                        + os.value(QStringLiteral("edition")).toString();
    m_valVer->setText(ver.trimmed().isEmpty() ? QStringLiteral("-") : ver.trimmed());
    m_valKernel->setText(d.value(QStringLiteral("kernel")).toString());
    const QString deploy = d.value(QStringLiteral("deploy")).toString();
    m_valDeploy->setText(deploy.isEmpty() ? QStringLiteral("-") : deploy);

    // 第二步：后台统计存储空间与快照大小（慢速，有缓存则快）。
    // 沿用 L2 局部加载：只更新顶部那一行 spinner，页面其余部分保持可读。
    m_busy->setText(tr("正在统计存储空间…"));
    runAsync([]() -> QVariant {
        QVariantMap d;
        d.insert(QStringLiteral("sizes"), QVariant::fromValue(getRepoSizes()));
        const auto snapSizes = computeSnapshotSizes();
        QVariantMap ss;
        qint64 total = 0;
        for (auto it = snapSizes.constBegin(); it != snapSizes.constEnd(); ++it)
            total += it.value().first + it.value().second;
        ss.insert(QStringLiteral("total"), total);
        d.insert(QStringLiteral("snapSizes"), ss);
        return d;
    }, [this](const QVariant &res) { onLoadedSizes(res); });
}

void OverviewPage::onLoadedSizes(const QVariant &res)
{
    m_busy->stop();

    const QVariantMap d = res.toMap();
    const RepoSizes sizes = d.value(QStringLiteral("sizes")).value<RepoSizes>();
    const QVariantMap ss = d.value(QStringLiteral("snapSizes")).toMap();
    const qint64 snapTotal = ss.value(QStringLiteral("total")).toLongLong();

    m_cardSnap->setValue(tr("%1 个 · %2").arg(m_snaps.size()).arg(formatSize(snapTotal)),
                         Semantic::Normal);
    m_cardFree->setValue(formatSize(sizes.freeRoot), Semantic::Normal);

    m_valRepo->setText(formatSize(sizes.sysRepo));
    m_valPrepo->setText(formatSize(sizes.persistRepo));
    m_valSnap->setText(formatSize(sizes.snapshotRoot));
    m_valFree->setText(formatSize(sizes.freeRoot));
}
