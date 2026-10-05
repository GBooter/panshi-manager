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

    // L2 局部加载
    m_busy = new PanshiUi::PanshiLocalLoading(tr("正在读取部署状态..."), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    m_table = new QTableWidget(0, 6, scrollContent);
    m_table->setHorizontalHeaderLabels({tr("索引"), tr("系统版本"), tr("状态"),
                                        tr("部署时间"), tr("提交ID"), tr("操作")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    PanshiUi::setTableAccessibleName(m_table, tr("系统部署列表"));
    auto *hh = m_table->horizontalHeader();
    hh->setSectionsMovable(false);
    for (int i = 0; i < 6; ++i)
        hh->setSectionResizeMode(i, QHeaderView::Interactive);
    hh->setSectionResizeMode(1, QHeaderView::Stretch);   // 版本列弹性，吸收剩余宽度
    hh->setMinimumSectionSize(72);
    hh->setStretchLastSection(false);

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
    m_empty->setTitle(tr("暂无系统版本"));
    m_empty->setDescription(tr("每次系统升级或回滚都会生成一个新的可启动系统版本（部署）。"));
    m_empty->hide();
    lay->addWidget(m_empty);

    auto *note = Ui::rowDesc(tr("说明: 当前启动的版本带「当前」标记; 历史版本会占用磁盘空间, "
                                "可删除以释放空间, 但不能删除当前正在使用的版本。"));
    lay->addWidget(note);

    setMaskLoadingEnabled(false);
}

void DeploysPage::applyColumnWidths()
{
    if (m_colsSized)
        return;
    m_colsSized = true;
    // 预算（默认窗口态视口 ≈761px）：固定列合计 ≤ 580px，其余由版本列弹性吸收
    // 索引 / 版本(弹性) / 状态 / 部署时间 / 提交ID / 操作
    m_table->setColumnWidth(0, 72);
    m_table->setColumnWidth(2, 100);
    m_table->setColumnWidth(3, 145);
    m_table->setColumnWidth(4, 150);
    m_table->setColumnWidth(5, 120);
    m_table->setColumnWidth(1, 160);   // 版本初始宽度，用户可拖动覆盖
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

const DeployInfo *DeploysPage::findDeploy(const QString &commitId, int index) const
{
    if (!commitId.isEmpty()) {
        for (const auto &d : m_deploys) {
            if (d.commitId == commitId)
                return &d;
        }
    }
    if (index >= 0 && index < m_deploys.size())
        return &m_deploys.at(index);
    return nullptr;
}

void DeploysPage::onLoaded(const QVariant &res)
{
    m_deploys = res.value<QList<DeployInfo>>();
    m_table->setRowCount(0);

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

        // 版本 + 徽章（版本列弹性 → ElidedLabel 中部省略，完整版本走 tooltip）
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
        if (d.isBooted) {
            auto *tag = new PanshiUi::PanshiBadge(tr("当前"), PanshiUi::Semantic::Accent, nameW);
            tag->setToolTip(tr("当前正在运行的系统版本"));
            nl->addWidget(tag, 0, Qt::AlignVCenter);
        }
        if (d.isRollbackBackup) {
            auto *tag = new PanshiUi::PanshiBadge(tr("回滚前"), PanshiUi::Semantic::Warning, nameW);
            tag->setToolTip(tr("回滚前自动创建的安全版本, 确认系统正常后可删除"));
            nl->addWidget(tag, 0, Qt::AlignVCenter);
        }
        m_table->setCellWidget(row, 1, nameW);

        it = new QTableWidgetItem(d.isBooted ? tr("当前启动")
                                             : (d.isRollbackBackup ? tr("回滚备份") : tr("历史")));
        it->setTextAlignment(Qt::AlignCenter);
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
        bDel->setEnabled(!d.isBooted);
        bDel->setToolTip(d.isBooted ? tr("不能删除当前启动的部署") : tr("删除该旧部署以释放空间"));
        // 无障碍：操作列是 cellWidget，带上版本号否则读屏分不清是哪一行
        bDel->setAccessibleName(tr("删除部署：%1").arg(verText));
        connect(bDel, &DPushButton::clicked, this, [this, i, commitId = d.commitId] {
            // 行下标在刷新后会错位，先按 commitId 复核，命中不了就放弃（宁可不删）
            if (i < 0 || i >= m_deploys.size() || m_deploys.at(i).commitId != commitId)
                return;
            onUndeploy(i);
        });
        ol->addWidget(bDel);
        ol->addStretch();
        m_table->setCellWidget(row, 5, opw);
    }

    applyColumnWidths();
    PanshiUi::finalizeTableAccessible(m_table);   // 索引/状态/部署时间/提交ID 四列
}

void DeploysPage::onUndeploy(int index)
{
    const DeployInfo *dp = findDeploy(QString(), index);
    if (!dp)
        return;
    const QString ver = dp->systemVersion.isEmpty() ? QString::number(index) : dp->systemVersion;
    const qint64 deployedAt = dp->time;

    PanshiUi::ConfirmRequest req;
    req.title = tr("删除部署");
    req.message = tr("删除后该版本的系统将无法再启动，占用空间会被释放。");
    req.facts = {tr("系统版本: %1").arg(ver),
                 tr("部署时间: %1").arg(formatTime(deployedAt))};
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
    runAsync([index]() -> QVariant {
        const CommandResult r = undeploy(index);
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
            // 原始输出收进「详情」（方案 §4.4 / O11）
            showFeedbackWithDetails(PanshiUi::Semantic::Danger,
                                    tr("删除失败，请查看详情了解具体原因。"),
                                    tr("完整输出"),
                                    m.value(QStringLiteral("msg")).toString());
        }
        refresh();
    });
}
