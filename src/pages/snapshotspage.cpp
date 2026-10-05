// SPDX-License-Identifier: GPL-3.0-or-later
#include "snapshotspage.h"
#include "common.h"
#include "../mainwindow.h"

#include <DDialog>
#include <DLineEdit>
#include <DTextEdit>
#include <DLabel>
#include <DFontSizeManager>
#include <DPalette>

#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QJsonObject>
#include <QVariantMap>
#include <QIcon>
#include <QProcess>
#include <QSizePolicy>
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using namespace Panshi;

namespace {

/** 还原策略取值（与后端 saveRollbackState 的 strategy 字段一致） */
const char *kStrategyManual = "manual";
const char *kStrategyReboot3 = "reboot3";
const char *kStrategyNone = "none";

/**
 * 操作列固定宽度。
 * 需容纳「还原」+「删除」两个按钮（含间距与内边距）——
 * 72px（QHeaderView 默认下限）会把文案截成单字，170px 是实测舒适值。
 */
constexpr int kOperationColWidth = 170;

} // namespace

SnapshotsPage::SnapshotsPage(MainWindow *win)
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

    // 页头与「刷新/创建备份」由外层 VersionManagementPage 统一渲染
    // （UI 重构方案 §3.2：页头全页唯一），此处不再重复。

    // 反馈条：替代原先的 infoDialog / errorDialog（方案 §4.4）
    m_banner = new PanshiUi::PanshiInlineBanner(scrollContent);
    // 注册为基类反馈宿主：否则 showFeedback()/showFeedbackWithDetails()
    // 因 m_feedback == nullptr 而**静默跳过**，本页反馈全部丢失（O11 连带修复）
    setFeedbackBanner(m_banner);
    m_banner->hide();
    lay->addWidget(m_banner);

    // 策略提示条：原先自绘 QFrame + QSS 双 objectName，
    // 现改为语义 Banner（图标 + 柔和底 + 中性正文），与全应用反馈样式统一
    m_policyBar = new PanshiUi::PanshiInlineBanner(scrollContent);
    m_policyBar->setSemantic(PanshiUi::Semantic::Accent);
    lay->addWidget(m_policyBar);

    // L2 局部加载：刷新时不盖整页，表格保持可读
    m_busy = new PanshiUi::PanshiLocalLoading(tr("正在读取快照列表..."), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // 表格：列宽预算按「默认窗口态」倒推，而不是按最大化。
    // 实测默认启动窗口（main.cpp 的 w.show()）表格视口约 761px，
    // 因此固定宽度列合计必须 ≤ 约 580px，其余交给弹性列吸收。
    // 描述列已移除（自由长度文本无法预算宽度，完整内容改由行内 tooltip 承载，
    // 详见 docs/UI-重构进度-阶段3-6-备份与版本页.md §九）。
    m_table = new QTableWidget(0, 6, scrollContent);
    m_table->setHorizontalHeaderLabels({tr("名称"), tr("类型"), tr("创建时间"), tr("占用空间"),
                                        tr("ID"), tr("操作")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    PanshiUi::setTableAccessibleName(m_table, tr("系统备份列表"));
    auto *hh = m_table->horizontalHeader();
    hh->setSectionsMovable(false);
    for (int i = 0; i < 6; ++i)
        hh->setSectionResizeMode(i, QHeaderView::Interactive);
    // 弹性归属：只有「名称」列 Stretch，操作列改为 Fixed 固定宽度。
    //
    // 为什么不让操作列也 Stretch：Stretch 模式下列宽由 QHeaderView 在每次布局时
    // 重算，setColumnWidth 对它无效——空间不足时它会一路缩到 minimumSectionSize
    // （72px），两个按钮文案被截成单字，且外部无法校正（试过 geometriesChanged
    // 回调，无法稳定生效且有递归风险）。改用 Fixed 后宽度完全由我们掌握。
    //
    // 代价：横向空间不足时只有名称列会被压缩（它有 ElidedLabel 兜底，是唯一
    // 适合被压缩的列），压缩到下限后整表宽度仍超出则出现横向滚动条。
    // 由于固定列合计 443 + 名称下限 72 + 操作 170 = 685px < 默认视口 761px，
    // 默认窗口态不会触发滚动条。
    hh->setSectionResizeMode(0, QHeaderView::Stretch);
    hh->setSectionResizeMode(5, QHeaderView::Fixed);
    hh->setMinimumSectionSize(72);
    hh->setStretchLastSection(false);
    m_table->setColumnWidth(5, kOperationColWidth);

    // 关键：让表格能真正缩进视口，横向溢出交给**表格自己的**滚动条兜底。
    //
    // 背景（第三轮验证实测）：最窄窗口（~434 逻辑宽）下「删除」按钮被裁掉右半。
    // 定位结论——不是按钮被截字，而是**外层容器静默裁剪**：
    //   · 表格各列最小宽合计 = 443（固定列）+ 72（名称下限）+ 170（操作 Fixed）= 685px；
    //   · 外层 Ui::makeScrollArea 是 setWidgetResizable(true) + ScrollBarAlwaysOff；
    //   · 视口窄于 685px 时，内容被钳住，多出部分被直接裁掉而不给滚动条，
    //     表格自身"看起来不溢出"，于是它自己的滚动条也不出现 → 「删除」不可达。
    //   实测阈值：视口 685px 对应窗口约 984 逻辑宽，比默认窗口窄约 8% 即触发。
    //
    // 修法（不改全局 makeScrollArea 策略，避免把其它页的意外超宽一并暴露）：
    //   1) QSizePolicy 横向设为 Ignored —— 让布局彻底忽略表格的大小提示
    //      （minimumSizeHint 里含 QHeaderView 按节累加出的宽度，是钳住表格的根因），
    //      表格于是可以自由缩到视口宽度；
    //   2) 显式给出一个小的 minimumWidth 作为真正的下限，避免被压到不可用；
    //   3) 横向策略设为 AsNeeded —— 缩到小于列宽合计时，滚动条出现在**表格内部**，
    //      页面整体布局不受影响，「删除」始终可达（滚一下即可）。
    //
    // 这与「接受截字」的区别：截字是信息丢失，滚动条是信息可达，后者才是闭环。
    {
        QSizePolicy sp = m_table->sizePolicy();
        sp.setHorizontalPolicy(QSizePolicy::Ignored);
        m_table->setSizePolicy(sp);
    }
    m_table->setMinimumWidth(320);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setMinimumHeight(320);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    DFontSizeManager::instance()->bind(m_table, DFontSizeManager::T6);
    lay->addWidget(m_table, 1);

    // 空状态：独立控件，随数据显隐（取代原来的 setSpan 占位行）
    m_empty = new PanshiUi::PanshiEmptyState(scrollContent);
    m_empty->setIcon(QStringLiteral("document-save"));
    m_empty->setTitle(tr("暂无备份"));
    m_empty->setDescription(tr("创建备份后可在系统出问题时还原到当前状态。"
                               "系统初始备份由磐石自动创建，受保护、不可删除。"));
    auto *bCreate = m_empty->addAction(tr("创建备份"), true);
    connect(bCreate, &DPushButton::clicked, this, &SnapshotsPage::onCreate);
    m_empty->hide();
    lay->addWidget(m_empty);

    auto *note = Ui::rowDesc(tr("说明: 系统初始备份受磐石系统保护, 不可删除或重命名。"
                                "回滚前备份带「回滚前」标识, 可在回滚成功后手动删除。"
                                "备份描述已并入名称列, 悬停行首即可查看全文。"));
    lay->addWidget(note);

    // 本页自带 L2 加载控件，关闭整页遮罩（加载态三档分级，方案 §4.6）
    setMaskLoadingEnabled(false);
}

void SnapshotsPage::applyColumnWidths()
{
    if (m_colsSized)
        return;
    m_colsSized = true;
    // 预算（默认窗口态视口 ≈761px）：
    //   固定列 类型110 + 时间145 + 空间100 + ID 88 = 443px
    //   操作列 Fixed 170px；名称列 Stretch 吸收其余宽度
    //   443 + 170 + 名称下限 72 = 685px < 761px → 默认窗口态无横向滚动条
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 145);
    m_table->setColumnWidth(3, 100);
    m_table->setColumnWidth(4, 88);
    m_table->setColumnWidth(0, 140);   // 名称初始宽度，用户可拖动覆盖
    m_table->setColumnWidth(5, kOperationColWidth);
}

void SnapshotsPage::refresh()
{
    if (m_running)
        return;
    m_busy->setText(tr("正在读取快照列表..."));
    m_busy->start();
    // 本页已关闭整页遮罩，这里再走一次静默执行即可，
    // 避免标签切换时"点一下 → 闪一层灰 → 再出内容"
    runAsyncQuiet([]() -> QVariant { return SnapshotsPage::loadPayload(); },
                  [this](const QVariant &res) {
                      m_busy->stop();
                      onLoaded(res);
                  });
}

QVariant SnapshotsPage::loadPayload()
{
    QVariantMap d;
    d.insert(QStringLiteral("snaps"), QVariant::fromValue(getSnapshots()));
    const auto sizes = computeSnapshotSizes();
    QVariantMap s;
    for (auto it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
        QVariantMap m;
        m.insert(QStringLiteral("extra"), it.value().first);
        m.insert(QStringLiteral("unique"), it.value().second);
        m.insert(QStringLiteral("total"), it.value().first + it.value().second);
        s.insert(it.key(), m);
    }
    d.insert(QStringLiteral("sizes"), s);
    d.insert(QStringLiteral("state"), QVariant::fromValue(getRollbackState()));
    return d;
}

void SnapshotsPage::onLoaded(const QVariant &res)
{
    const QVariantMap d = res.toMap();
    const auto snaps = d.value(QStringLiteral("snaps")).value<QList<SnapshotInfo>>();
    const QVariantMap sizesMap = d.value(QStringLiteral("sizes")).toMap();
    QMap<QString, QPair<qint64, qint64>> sizes;
    for (auto it = sizesMap.constBegin(); it != sizesMap.constEnd(); ++it) {
        const QVariantMap m = it.value().toMap();
        sizes.insert(it.key(), qMakePair(m.value(QStringLiteral("extra")).toLongLong(),
                                         m.value(QStringLiteral("unique")).toLongLong()));
    }
    const RollbackState state = d.value(QStringLiteral("state")).value<RollbackState>();
    render(snaps, sizes, state);
}

void SnapshotsPage::updatePolicyText(int count, const RollbackState &state)
{
    QString statusTxt = tr("未启用");
    if (state.valid) {
        const QString st = state.status;
        if (st == QLatin1String("manual")) statusTxt = tr("手动");
        else if (st == QLatin1String("reboot3")) statusTxt = tr("重启 3 次后");
        else if (st == QLatin1String("done")) statusTxt = tr("已完成");
        else if (st == QLatin1String("failed")) statusTxt = tr("失败");
        else if (st == QLatin1String("cancelled")) statusTxt = tr("已取消");
        else statusTxt = tr("进行中");
    }
    m_policyBar->setSemantic(state.valid ? PanshiUi::Semantic::Warning
                                        : PanshiUi::Semantic::Accent);
    m_policyBar->setText(tr("备份数量上限: %1　·　当前 %2 个　·　回滚前安全备份: %3")
                             .arg(tr("不限制")).arg(count).arg(statusTxt));
}

const SnapshotInfo *SnapshotsPage::findSnapshot(const QString &id) const
{
    for (const auto &s : m_snaps) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

void SnapshotsPage::render(const QList<SnapshotInfo> &snaps,
                           const QMap<QString, QPair<qint64, qint64>> &sizes,
                           const RollbackState &state)
{
    m_snaps = snaps;
    m_sizes = sizes;
    m_table->setRowCount(0);

    updatePolicyText(snaps.size(), state);

    // 空列表：表格整体让位给空状态控件（含「创建备份」主操作）
    const bool empty = snaps.isEmpty();
    m_table->setVisible(!empty);
    m_empty->setVisible(empty);

    const QString rbId = state.backupSnapshotId;
    for (int i = 0; i < snaps.size(); ++i) {
        const SnapshotInfo &s = snaps.at(i);
        const bool isRb = (s.id == rbId);
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        // 名称 + 「回滚前」/「初始」徽章
        // 名称列弹性且可能被压窄 → 用 ElidedLabel 做中部省略，完整名称与描述走 tooltip
        auto *nameWidget = new QWidget;
        auto *nl = new QHBoxLayout(nameWidget);
        nl->setContentsMargins(6, 2, 6, 2);
        nl->setSpacing(PanshiUi::Space::S);
        auto *nm = new Ui::ElidedLabel(s.name, nameWidget);
        QFont f = nm->font();
        f.setBold(true);
        nm->setFont(f);
        // 描述已从独立列移除，合并进名称 tooltip（描述为空时只显示名称）
        nm->setToolTip(s.desc.isEmpty()
                           ? s.name
                           : tr("%1\n描述: %2").arg(s.name, s.desc));
        // 无障碍：名称列是 cellWidget（不是 item），读屏读不到 item 角色，
        // 且描述信息只存在于 tooltip —— 一并写进可访问名，否则等于丢失。
        nm->setAccessibleName(s.desc.isEmpty()
                                  ? s.name
                                  : tr("%1，%2").arg(s.name, s.desc));
        nl->addWidget(nm, 1);
        if (s.isFirst) {
            auto *tag = new PanshiUi::PanshiBadge(tr("初始"), PanshiUi::Semantic::Accent,
                                                  nameWidget);
            tag->setToolTip(tr("系统初始备份, 受磐石保护, 不可删除"));
            nl->addWidget(tag, 0, Qt::AlignVCenter);
        }
        if (isRb) {
            auto *tag = new PanshiUi::PanshiBadge(tr("回滚前"), PanshiUi::Semantic::Warning,
                                                  nameWidget);
            tag->setToolTip(tr("还原前自动创建的安全备份, 回滚成功后可在本页手动删除"));
            nl->addWidget(tag, 0, Qt::AlignVCenter);
        }
        m_table->setCellWidget(row, 0, nameWidget);

        // 类型
        QString typ = tr("普通备份");
        if (s.isFirst) typ = tr("系统初始备份");
        else if (isRb) typ = tr("回滚前备份");
        auto *it = new QTableWidgetItem(typ);
        it->setTextAlignment(Qt::AlignCenter);
        it->setToolTip(typ);
        m_table->setItem(row, 1, it);

        it = new QTableWidgetItem(formatTime(s.time));
        it->setToolTip(formatTime(s.time));
        m_table->setItem(row, 2, it);

        const auto sz = sizes.value(s.id);
        const qint64 total = sz.first + sz.second;
        it = new QTableWidgetItem(total > 0 ? formatSize(total) : tr("计算中..."));
        it->setTextAlignment(Qt::AlignCenter);
        it->setToolTip(tr("增量数据: %1\n独占对象: %2")
                           .arg(formatSize(sz.first)).arg(formatSize(sz.second)));
        m_table->setItem(row, 3, it);

        it = new QTableWidgetItem(s.id.left(12) + QStringLiteral("..."));
        it->setToolTip(s.id);
        it->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, it);

        // 操作
        auto *opw = new QWidget;
        auto *ol = new QHBoxLayout(opw);
        ol->setContentsMargins(4, 2, 4, 2);
        ol->setSpacing(6);
        auto *bRestore = Ui::button(tr("还原"));
        bRestore->setToolTip(tr("将系统还原到此备份的状态（重启后生效）"));
        // 无障碍：操作列是 cellWidget，按钮自带文字尚可，但读屏逐格移动时
        // 听不出是哪个备份 → 带上快照名
        bRestore->setAccessibleName(tr("还原到备份：%1").arg(s.name));
        connect(bRestore, &DPushButton::clicked, this, [this, id = s.id] { onRollback(id); });
        ol->addWidget(bRestore);
        auto *bDel = Ui::button(tr("删除"), false, true);
        bDel->setEnabled(!s.isFirst);
        bDel->setToolTip(s.isFirst ? tr("系统初始备份受保护, 不可删除") : tr("删除此备份"));
        bDel->setAccessibleName(tr("删除备份：%1").arg(s.name));
        connect(bDel, &DPushButton::clicked, this, [this, id = s.id] { onDelete(id); });
        ol->addWidget(bDel);
        ol->addStretch();
        m_table->setCellWidget(row, 5, opw);
    }

    applyColumnWidths();
    PanshiUi::finalizeTableAccessible(m_table);   // 类型/时间/占用/ID 四列
}

// ---------- 创建 ----------
void SnapshotsPage::onCreate()
{
    // 防重入检查放在对话框之前：否则刷新进行中打开创建框、填完却被静默丢弃
    if (m_running) {
        m_banner->setSemantic(PanshiUi::Semantic::Warning);
        m_banner->setText(tr("上一项操作尚未完成，请稍候再试。"));
        m_banner->setClosable(true);
        return;
    }

    DDialog dlg(this);
    dlg.setTitle(tr("创建备份"));
    dlg.setFixedWidth(480);
    auto *content = new QWidget(&dlg);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(20, 12, 20, 12);
    lay->setSpacing(PanshiUi::Space::M);

    lay->addWidget(Ui::rowLabel(tr("备份名称")));
    auto *nameEdit = new DLineEdit(content);
    nameEdit->setPlaceholderText(tr("例如: 更新前备份"));
    nameEdit->setText(tr("手动备份-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm")));
    lay->addWidget(nameEdit);

    lay->addWidget(Ui::rowLabel(tr("描述(可选)")));
    auto *descEdit = new DTextEdit(content);
    descEdit->setPlaceholderText(tr("记录该备份的用途..."));
    descEdit->setFixedHeight(76);
    lay->addWidget(descEdit);

    dlg.addContent(content);
    dlg.addButton(tr("取消"), false, DDialog::ButtonNormal);
    dlg.addButton(tr("创建"), false, DDialog::ButtonRecommend);
    if (dlg.exec() != 1)
        return;

    const QString name = nameEdit->text().trimmed();
    const QString desc = descEdit->toPlainText().trimmed();
    if (name.isEmpty()) {
        // 反馈内联化：不再弹错误窗（方案 §4.4）
        m_banner->setSemantic(PanshiUi::Semantic::Danger);
        m_banner->setText(tr("备份名称不能为空，请填写后重试。"));
        m_banner->setClosable(true);
        return;
    }

    m_running = true;
    m_busy->setText(tr("正在创建备份, 请稍候..."));
    m_busy->start();
    runAsync([name, desc]() -> QVariant {
        const CommandResult r = createSnapshot(name, desc);
        return QVariantMap{{QStringLiteral("ok"), r.ok},
                           {QStringLiteral("msg"), r.ok ? name : r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            m_banner->setSemantic(PanshiUi::Semantic::Success);
            m_banner->setClosable(false);
            m_banner->setText(tr("备份“%1”创建成功。")
                                  .arg(m.value(QStringLiteral("msg")).toString()));
            m_banner->autoDismiss(3000);
        } else {
            // 原始输出收进「详情」（方案 §4.4 / O11）
            showFeedbackWithDetails(PanshiUi::Semantic::Danger,
                                    tr("创建失败，请查看详情了解具体原因。"),
                                    tr("完整输出"),
                                    m.value(QStringLiteral("msg")).toString());
        }
        refresh();
    });
}

// ---------- 删除 ----------
void SnapshotsPage::onDelete(const QString &id)
{
    // 防重入检查在对话框之前（同 onCreate）
    if (m_running)
        return;
    const SnapshotInfo *snap = findSnapshot(id);
    if (!snap)
        return;

    PanshiUi::ConfirmRequest req;
    req.title = tr("删除备份");
    req.message = tr("删除后该备份的增量数据将被释放，且无法恢复。");
    req.facts = {tr("目标备份: %1").arg(snap->name),
                 tr("创建时间: %1").arg(formatTime(snap->time))};
    req.confirmText = tr("删除");
    req.cancelText = tr("取消");
    req.level = PanshiUi::DialogLevel::Danger;
    if (!PanshiUi::askConfirm(this, req))
        return;

    m_running = true;
    m_busy->setText(tr("正在删除备份..."));
    m_busy->start();
    runAsync([id]() -> QVariant {
        const CommandResult r = deleteSnapshot(id);
        return QVariantMap{{QStringLiteral("ok"), r.ok}, {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            m_banner->setSemantic(PanshiUi::Semantic::Success);
            m_banner->setClosable(false);
            m_banner->setText(tr("备份已删除，占用空间已释放。"));
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

// ---------- 还原 ----------
void SnapshotsPage::onRollback(const QString &id)
{
    if (m_running)
        return;
    const SnapshotInfo *snap = findSnapshot(id);
    if (!snap)
        return;

    // 可写层冲突检查
    const WritableStatus w = getWritableStatus();
    if (w.enable) {
        PanshiUi::ConfirmRequest req;
        req.title = tr("可写层冲突");
        req.message = tr("当前可写层（只读保护已关闭）处于开启状态，磐石会拒绝还原操作。\n"
                         "请先关闭可写层、恢复只读保护。");
        req.facts = {tr("要还原的备份: %1").arg(snap->name)};
        req.confirmText = tr("前往设置");
        req.level = PanshiUi::DialogLevel::Normal;
        // 跳保护页并定位高亮「开启保护（恢复只读）」行：用户在这个语境下
        // 唯一要做的事就是点它，直接指出来（阶段 5 跨页跳转）。
        if (PanshiUi::askConfirm(this, req))
            m_win->gotoPage(PageId::Protection,
                            [this] {
                                QMetaObject::invokeMethod(m_win->page(PageId::Protection),
                                                          "highlightEnableProtect",
                                                          Qt::DirectConnection);
                            });
        return;
    }

    // 单次决策：把原先「还原对话框 + 二次确认」两次打断合并为一次
    // （摘要 + 后果清单 + 策略单选都由 PanshiUi::ConfirmRequest 承载）
    const QStringList strategies = {
        tr("自动创建回滚前备份，保留待手动删除（推荐）"),
        tr("自动创建，且系统重启 3 次正常后自动删除"),
        tr("不创建回滚前备份（不推荐）"),
    };
    int strategyIdx = 0;

    PanshiUi::ConfirmRequest req;
    req.title = tr("还原到备份");
    req.message = tr("系统将还原到所选备份的状态，重启后生效。");
    req.facts = {tr("目标备份: %1").arg(snap->name),
                 tr("创建时间: %1").arg(formatTime(snap->time)),
                 tr("描述: %1").arg(snap->desc.isEmpty() ? tr("无") : snap->desc)};
    req.consequences = {
        tr("该备份之后对【系统层】的修改（安装的软件、系统配置）将丢失"),
        tr("用户数据（/home 数据分区）不受影响"),
        tr("还原期间请勿断电，系统会在失败时自动回退"),
    };
    req.options = strategies;
    req.optionDefault = 0;
    req.optionResult = &strategyIdx;
    req.confirmText = tr("开始还原");
    req.cancelText = tr("取消");
    req.level = PanshiUi::DialogLevel::Warning;
    if (!PanshiUi::askConfirm(this, req))
        return;

    QString strategy = QLatin1String(kStrategyManual);
    if (strategyIdx == 1) strategy = QLatin1String(kStrategyReboot3);
    else if (strategyIdx == 2) strategy = QLatin1String(kStrategyNone);

    doRollback(snap->id, snap->name, strategy);
}

void SnapshotsPage::doRollback(const QString &id, const QString &name, const QString &strategy)
{
    if (m_running)
        return;
    m_running = true;
    m_busy->setText(tr("正在准备还原..."));
    m_busy->start();
    runAsync([id, name, strategy]() -> QVariant {
        QVariantMap out;
        const QString orig = getBootedDeployId();
        QString backupId;
        QString bname;

        if (strategy != QLatin1String(kStrategyNone)) {
            // 前缀也走 tr()（与「手动备份-」同一类修法）：这是**用户可见的
            // 备份名**，会出现在列表、tooltip 与还原事实里，不译就是混排。
            bname = tr("回滚前备份-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
            CommandResult r = createSnapshot(bname, tr("回滚到[%1]前的系统状态").arg(name));
            if (!r.ok) {
                out.insert(QStringLiteral("ok"), false);
                out.insert(QStringLiteral("msg"), r.errorMessage);
                return out;
            }
            const auto snaps = getSnapshots();
            for (const auto &s : snaps) {
                if (s.name == bname) { backupId = s.id; break; }
            }
        }
        const CommandResult rb = rollbackSnapshot(id, false);
        if (!rb.ok) {
            out.insert(QStringLiteral("ok"), false);
            out.insert(QStringLiteral("msg"), rb.errorMessage);
            return out;
        }
        if (!backupId.isEmpty()) {
            QJsonObject state;
            state.insert(QStringLiteral("rollback_target"), id);
            state.insert(QStringLiteral("rollback_target_name"), name);
            state.insert(QStringLiteral("backup_snapshot_id"), backupId);
            state.insert(QStringLiteral("backup_snapshot_name"), bname);
            state.insert(QStringLiteral("original_booted"), orig);
            state.insert(QStringLiteral("created_at"), QDateTime::currentSecsSinceEpoch());
            state.insert(QStringLiteral("reboot_count"), 0);
            state.insert(QStringLiteral("reboot_limit"), 3);
            state.insert(QStringLiteral("strategy"), strategy);
            state.insert(QStringLiteral("status"), QStringLiteral("pending"));
            saveRollbackState(state);
            if (strategy == QLatin1String(kStrategyReboot3))
                installRollbackCleanup();
        }
        out.insert(QStringLiteral("ok"), true);
        out.insert(QStringLiteral("backupId"), backupId);
        return out;
    }, [this, name](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (!m.value(QStringLiteral("ok")).toBool()) {
            // 原始输出收进「详情」，正文只留可行动说明（方案 §4.4 / O11）
            showFeedbackWithDetails(PanshiUi::Semantic::Danger,
                                    tr("还原失败，请查看详情了解具体原因。"),
                                    tr("完整输出"),
                                    m.value(QStringLiteral("msg")).toString());
            refresh();
            return;
        }

        // 还原已就绪：重启是"下一步动作"，不是错误反馈，保留一次决策
        PanshiUi::ConfirmRequest req;
        req.title = tr("还原已就绪");
        req.message = tr("系统将在重启后自动还原到所选备份。请先保存您的工作。");
        req.facts = {tr("目标备份: %1").arg(name)};
        req.confirmText = tr("立即重启");
        req.cancelText = tr("稍后手动重启");
        req.level = PanshiUi::DialogLevel::Normal;
        if (PanshiUi::askConfirm(this, req)) {
            QProcess::startDetached(QStringLiteral("systemctl"), QStringList() << QStringLiteral("reboot"));
        } else {
            m_banner->setSemantic(PanshiUi::Semantic::Warning);
            m_banner->setClosable(true);
            m_banner->setText(tr("还原已就绪，请稍后手动重启系统以生效。"));
        }
        refresh();
    });
}
