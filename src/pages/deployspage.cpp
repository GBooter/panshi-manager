// SPDX-License-Identifier: GPL-3.0-or-later
#include "deployspage.h"
#include "common.h"
#include "../mainwindow.h"

#include <DDialog>
#include <DLabel>
#include <DFontSizeManager>

#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariantMap>
#include <QIcon>
#include <QSizePolicy>
DWIDGET_USE_NAMESPACE

using namespace Panshi;

namespace {
/** 操作列「删除部署」按钮的下限宽度（120 实测容纳按钮文案 + 内边距） */
constexpr int kOperationColWidth = 120;
/** 版本号列下限宽度：长版本串有 ElidedLabel + tooltip 兜底，但太窄无意义 */
constexpr int kVersionMinWidth = 140;
} // namespace

// 把 ctl / polkit 的英文（或中文）报错翻译成用户能看懂的原因，用作失败横幅正文
QString DeploysPage::humanizeUndeployError(const QString &raw)
{
    const QString s = raw.toLower();
    if (s.contains("replacement mainline") || s.contains("替换主线") || s.contains("待生效"))
        return tr("该版本是系统更新后待生效的新系统，重启电脑后才会启用，目前受系统保护，无法删除。");
    if (s.contains("current deployment") || s.contains("last visible") || s.contains("当前部署") || s.contains("正在使用"))
        return tr("这是当前正在运行的系统版本，无法删除。");
    if (s.contains("out of range") || s.contains("not exist") || s.contains("不存在") || s.contains("no such"))
        return tr("找不到对应的系统版本（索引越界或已不存在），请刷新列表后重试。");
    if (s.contains("not authorized") || s.contains("需要密码") || s.contains("authentication")
        || s.contains("授权") || s.contains("密码"))
        return tr("没有获得管理员授权，删除未执行。请确认已正确输入密码；若仍失败，请检查系统的授权（polkit）设置。");
    return tr("删除失败，请查看详情了解具体原因。");
}

DeploysPage::DeploysPage(MainWindow *win)
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
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(PanshiUi::Space::M);

    // 页头与「刷新」由外层 VersionManagementPage 统一渲染
    // （UI 重构方案 §3.2：页头全页唯一），此处不再重复。

    // 反馈条：替代 infoDialog / errorDialog（方案 §4.4）
    m_banner = new PanshiUi::PanshiInlineBanner(scrollContent);
    // 注册为基类反馈宿主：否则 showFeedback()/showFeedbackWithDetails()
    // 因 m_feedback == nullptr 而**静默跳过**，本页反馈全部丢失（O11 连带修复）
    setFeedbackBanner(m_banner);
    m_banner->hide();
    lay->addWidget(m_banner);

    // ---- 部署状态三卡（2026-10-06 二轮重设计）----
    // 与快照页的策略卡同构：两个子页视觉结构一致，页面不再是「提示 + 光秃秃表格」。
    // 窄窗由 PanshiResponsiveCardRow 自动折行不溢出（registerResponsiveCardRow 喂宽度）。
    m_statRow = new PanshiUi::PanshiResponsiveCardRow(3, scrollContent);
    m_statTotal = new PanshiUi::PanshiStateCard(tr("部署总数"), m_statRow);
    m_statBooted = new PanshiUi::PanshiStateCard(tr("当前启动"), m_statRow);
    m_statDeletable = new PanshiUi::PanshiStateCard(tr("可删除部署"), m_statRow);
    m_statRow->addCard(m_statTotal);
    m_statRow->addCard(m_statBooted);
    m_statRow->addCard(m_statDeletable);
    lay->addWidget(m_statRow);
    registerResponsiveCardRow(m_statRow, scroll, PanshiUi::Space::XL * 2);

    // L2 局部加载
    m_busy = new PanshiUi::PanshiLocalLoading(tr("正在读取部署状态..."), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    m_table = new QTableWidget(0, 6, scrollContent);
    // ⚠️ 列头原为「系统版本」，与页签名同词异义（本列是版本号字符串，页签指整套部署）→ 改「版本号」
    m_table->setHorizontalHeaderLabels({tr("索引"), tr("版本号"), tr("状态"),
                                        tr("部署时间"), tr("提交ID"), tr("操作")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    m_table->setShowGrid(false);   // 与磁盘页/快照页一致：去网格更清爽
    m_table->horizontalHeader()->setFixedHeight(42);
    PanshiUi::setTableAccessibleName(m_table, tr("系统部署列表"));
    auto *hh = m_table->horizontalHeader();
    hh->setSectionsMovable(false);
    // 列宽：全部列 Interactive，用户可自由拖拽调整任意列宽
    // （2026-10-06 用户反馈「列宽不可以调整」——原「版本号 Stretch」列手柄是死的。
    //   与快照页同案：取消 Stretch，全列可拖；操作列保留下限钳制，按钮不被截字；
    //   宽窗留白由用户拖宽「版本号」吸收，窄窗由表格内部横向滚动条兜底。）
    for (int i = 0; i < 6; ++i)
        hh->setSectionResizeMode(i, QHeaderView::Interactive);
    hh->setMinimumSectionSize(72);
    hh->setStretchLastSection(false);
    m_table->setColumnWidth(5, kOperationColWidth);
    connect(hh, &QHeaderView::sectionResized, this, [hh](int logical, int, int newSize) {
        if (logical == 5 && newSize < kOperationColWidth)
            hh->resizeSection(5, kOperationColWidth);   // 弹回后再次触发 == 不成立，无递归
        else if (logical == 1 && newSize < kVersionMinWidth)
            hh->resizeSection(1, kVersionMinWidth);     // 版本号下限，避免被压到不可读
    });

    // 与 SnapshotsPage 同因同治（详见 §十二）：窄窗下表格被外层
    // makeScrollArea（ScrollBarAlwaysOff）静默裁掉右侧，「删除部署」不可达。
    // 让布局忽略表格自身的大小提示、给出小的显式下限，再把横向溢出交给
    // 表格内部的滚动条——页面布局不受影响，操作列始终可达。
    {
        QSizePolicy sp = m_table->sizePolicy();
        sp.setHorizontalPolicy(QSizePolicy::Ignored);
        m_table->setSizePolicy(sp);
    }
    m_table->setMinimumWidth(320);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setMinimumHeight(260);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    DFontSizeManager::instance()->bind(m_table, DFontSizeManager::T6);
    lay->addWidget(m_table, 1);

    // 空状态
    m_empty = new PanshiUi::PanshiEmptyState(scrollContent);
    m_empty->setIcon(QStringLiteral("system-upgrade"));
    m_empty->setTitle(tr("暂无系统部署实例"));
    m_empty->setDescription(tr("每次系统升级或回滚都会生成一个新的可启动系统（部署实例）。"));
    m_empty->hide();
    lay->addWidget(m_empty);

    auto *note = Ui::rowDesc(tr("提示：带「当前启动」「待生效」标记的部署受系统保护，不能删除；"
                                "要回到某个时间点，请到「系统快照」页还原。"));
    lay->addWidget(note);

    setMaskLoadingEnabled(false);
}

void DeploysPage::applyColumnWidths()
{
    if (m_colsSized)
        return;
    m_colsSized = true;
    // 预算（默认窗口态视口 ≈761px）——无 Stretch 列，全列可拖：
    //   索引 50 + 版本号 210 + 状态 90 + 部署时间 140 + 提交ID 100 + 操作 120 = 710px
    //   < 视口，默认窗口态不出横向滚动条；版本号 210 足够完整显示
    //   28 字符的版本串（如 deepin-V25-25.2.2-103.100，实测 ≈185px）。
    m_table->setColumnWidth(0, 50);
    m_table->setColumnWidth(1, 210);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 140);
    m_table->setColumnWidth(4, 100);
    m_table->setColumnWidth(5, kOperationColWidth);
}

void DeploysPage::refresh()
{
    if (m_running)
        return;
    m_busy->setText(tr("正在读取部署状态..."));
    m_busy->start();
    // 静默执行：本页已关闭整页遮罩，避免标签切换时再闪一层灰
    runAsyncQuiet([]() -> QVariant { return QVariant::fromValue(getDeploys()); },
                  [this](const QVariant &res) {
                      m_busy->stop();
                      onLoaded(res);
                  });
}

const DeployInfo *DeploysPage::findDeploy(const QString &commitId) const
{
    if (commitId.isEmpty())
        return nullptr;
    for (const auto &d : m_deploys) {
        if (d.commitId == commitId)
            return &d;
    }
    return nullptr;
}

void DeploysPage::onLoaded(const QVariant &res)
{
    m_deploys = res.value<QList<DeployInfo>>();
    m_table->setRowCount(0);

    // 刷新三张状态卡：总数 / 当前启动 / 可删除部署（可删判据与操作列按钮同一处）
    int booted = 0, deletable = 0;
    for (const DeployInfo &d : m_deploys) {
        if (d.isBooted)
            ++booted;
        if (Panshi::isDeployDeletable(d))
            ++deletable;
    }
    m_statTotal->setValue(QString::number(m_deploys.size()));
    m_statBooted->setValue(QString::number(booted),
                           booted > 0 ? PanshiUi::Semantic::Accent : PanshiUi::Semantic::Muted);
    m_statDeletable->setValue(QString::number(deletable),
                              deletable > 0 ? PanshiUi::Semantic::Normal : PanshiUi::Semantic::Muted);

    const bool empty = m_deploys.isEmpty();
    m_table->setVisible(!empty);
    m_empty->setVisible(empty);

    for (int i = 0; i < m_deploys.size(); ++i) {
        const DeployInfo &d = m_deploys.at(i);
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *it = new QTableWidgetItem(QString::number(i));
        it->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, it);

        // 版本号：纯文本 + 当前启动加粗，完整版本经 tooltip 承载。
        // 2026-10-06 二轮重设计：原「当前启动/待生效/回滚前」徽章与「状态」列取值
        // 重复（同一行两个「当前启动」），且挤占版本列宽度，28 字符版本串被中省略
        // 成「deep….100」——关键信息反而看不清。徽章职责并入状态列（语义着色），
        // 版本列加宽到 210，完整显示版本号。
        auto *nameW = new QWidget;
        auto *nl = new QHBoxLayout(nameW);
        nl->setContentsMargins(6, 2, 6, 2);
        nl->setSpacing(PanshiUi::Space::S);
        const QString verText = d.systemVersion.isEmpty() ? QStringLiteral("-") : d.systemVersion;
        auto *nm = new Ui::ElidedLabel(verText, nameW);
        if (d.isBooted) {
            QFont f = nm->font();
            f.setBold(true);
            nm->setFont(f);
        }
        nm->setToolTip(verText);
        // 无障碍：版本列是 cellWidget（不是 item），读屏读不到 item 角色 → 单独挂名
        nm->setAccessibleName(verText);
        nl->addWidget(nm, 1);
        m_table->setCellWidget(row, 1, nameW);

        // 状态列：把「能不能删」直接写进这一行的 tooltip，
        // 用户不必 hover 到操作列去找那条置灰按钮的原因。
        // 2026-10-06 二轮重设计：按语义着色 —— 当前启动=Accent（正在运行）、
        // 待生效/回滚备份=Warning（有保护或需要留意）、历史=Muted（已不再使用），
        // 扫视即分「正在运行 / 待启用 / 可清理」三档。
        QString statusText;
        QString statusTip;
        PanshiUi::Semantic se;
        if (d.isBooted) {
            statusText = tr("当前启动");
            statusTip = tr("此刻正在运行的系统，受系统保护，不能删除。");
            se = PanshiUi::Semantic::Accent;
        } else if (d.hidden) {
            statusText = tr("待生效");
            statusTip = tr("系统更新后待生效的新系统，重启电脑后才会启用，届时它会变成「当前启动」。受系统保护，不能删除。");
            se = PanshiUi::Semantic::Warning;
        } else if (d.isRollbackBackup) {
            statusText = tr("回滚备份");
            statusTip = tr("还原系统时自动创建的安全部署，确认系统正常后可以删除。");
            se = PanshiUi::Semantic::Warning;
        } else {
            statusText = tr("历史");
            statusTip = tr("已不再使用的旧系统，可删除以释放磁盘空间。");
            se = PanshiUi::Semantic::Muted;
        }
        it = new QTableWidgetItem(statusText);
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(PanshiUi::semanticColor(se));
        it->setToolTip(statusTip);
        m_table->setItem(row, 2, it);

        it = new QTableWidgetItem(formatTime(d.time));
        m_table->setItem(row, 3, it);

        it = new QTableWidgetItem(d.commitId.left(16) + QStringLiteral("..."));
        it->setToolTip(d.commitId);
        it->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, it);

        auto *opw = new QWidget;
        auto *ol = new QHBoxLayout(opw);
        ol->setContentsMargins(4, 2, 4, 2);
        ol->setSpacing(6);
        auto *bDel = Ui::button(tr("删除部署"), false, true);
        // 置灰判据只有一处（isDeployDeletable），tooltip 也按同一判据给理由，
        // 避免出现「按钮灰了但提示说可以删」这种自相矛盾（可删性是同一件事）。
        const bool deletable = Panshi::isDeployDeletable(d);
        bDel->setEnabled(deletable);
        if (d.isBooted)
            bDel->setToolTip(tr("这是当前正在运行的系统，不能删除。"));
        else if (d.hidden)
            bDel->setToolTip(tr("这是系统更新后待生效的新系统，重启电脑后才会启用，目前受系统保护，不能删除。"));
        else if (d.commitId.isEmpty())
            bDel->setToolTip(tr("该部署缺少标识信息，无法删除，请刷新列表后重试。"));
        else
            bDel->setToolTip(tr("删除该部署实例以释放磁盘空间，删除后该系统将无法再启动。"));
        // 无障碍：操作列是 cellWidget，带上版本号否则读屏分不清是哪一行
        bDel->setAccessibleName(tr("删除部署：%1").arg(verText));
        connect(bDel, &DPushButton::clicked, this, [this, commitId = d.commitId] {
            // 始终按 commitId 定位（下标在刷新后会错位，且隐藏主线两条都是 Index 0）
            if (commitId.isEmpty())
                return;
            onUndeploy(commitId);
        });
        ol->addWidget(bDel);
        ol->addStretch();
        m_table->setCellWidget(row, 5, opw);
    }

    applyColumnWidths();
    PanshiUi::finalizeTableAccessible(m_table);   // 索引/状态/部署时间/提交ID 四列
}

void DeploysPage::onUndeploy(const QString &commitId)
{
    const DeployInfo *dp = findDeploy(commitId);
    if (!dp)
        return;
    const DeployInfo d = *dp;
    const QString ver = d.systemVersion.isEmpty() ? d.commitId.left(12) : d.systemVersion;
    const qint64 deployedAt = d.time;

    PanshiUi::ConfirmRequest req;
    req.title = tr("删除部署");
    req.message = tr("删除后该版本的系统将无法再启动，占用空间会被释放。此操作不可撤销。");
    req.facts = {tr("系统版本: %1").arg(ver),
                 tr("部署时间: %1").arg(formatTime(deployedAt)),
                 tr("提交ID: %1").arg(d.commitId.left(12) + QStringLiteral("..."))};
    req.confirmText = tr("删除");
    req.cancelText = tr("取消");
    req.level = PanshiUi::DialogLevel::Danger;
    if (!PanshiUi::askConfirm(this, req))
        return;

    if (m_running)
        return;
    m_running = true;
    m_busy->setText(tr("正在删除部署..."));
    m_busy->start();
    runAsync([commitId]() -> QVariant {
        const CommandResult r = undeploy(commitId);
        return QVariantMap{{QStringLiteral("ok"), r.ok}, {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            m_banner->setSemantic(PanshiUi::Semantic::Success);
            m_banner->setClosable(false);
            m_banner->setText(tr("部署已删除，占用空间已释放。"));
            m_banner->autoDismiss(3000);
        } else {
            // 把 ctl / polkit 的原始报错翻译成用户能看懂的原因，原文收进「详情」
            showFeedbackWithDetails(PanshiUi::Semantic::Danger,
                                    humanizeUndeployError(m.value(QStringLiteral("msg")).toString()),
                                    tr("完整输出"),
                                    m.value(QStringLiteral("msg")).toString());
        }
        refresh();
    });
}
