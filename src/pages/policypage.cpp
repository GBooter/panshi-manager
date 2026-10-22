// SPDX-License-Identifier: GPL-3.0-or-later
#include "policypage.h"
#include "common.h"
#include "../mainwindow.h"
#include "../backend.h"

#include <DSwitchButton>
#include <DSpinBox>
#include <DComboBox>
#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QVariantMap>

DWIDGET_USE_NAMESPACE

using namespace Panshi;
using namespace PanshiUi;

namespace {
QString configPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir d(base + QStringLiteral("/panshi-manager"));
    if (!d.exists()) d.mkpath(".");
    return d.filePath(QStringLiteral("config.json"));
}
} // namespace

PolicyPage::PolicyPage(MainWindow *win)
    : BasePage(win)
    , m_win(win)
{
    m_appCfg = loadAppConfig();

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
    lay->setSpacing(Space::L);

    lay->addWidget(Ui::pageHeader(tr("策略与设置"), tr("备份保留策略与磐石系统参数"),
                                 QStringLiteral("preferences-system")));

    // ---- 反馈条 / 局部加载 ----
    m_banner = new PanshiInlineBanner(scrollContent);
    setFeedbackBanner(m_banner);
    m_banner->hide();
    lay->addWidget(m_banner);

    m_busy = new PanshiLocalLoading(QString(), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // ================= 分组1: 备份保留 =================
    // 信息架构重排（2026-10-06）：备份（数量+清理）聚合一组，
    // 回滚相关移入「更新与回滚」，系统级参数（阈值）单独成组。
    auto *secPolicy = new PanshiSection(
        tr("备份保留"),
        tr("系统升级会自动创建备份（系统内置，不可关闭）。以下策略控制备份保留与清理，避免无限累积。"),
        scrollContent);
    // 组头右侧「即时生效」徽章：本组设置改动即写入应用配置，无需手动保存
    secPolicy->addHeaderWidget(new PanshiBadge(tr("即时生效"), Semantic::Muted, secPolicy));

    // 备份数量上限（主导设置）
    auto *rMax = new PanshiRow(secPolicy);
    m_rowMax = rMax;
    rMax->setTitle(tr("备份数量上限"));
    rMax->setDescription(tr("0 表示不限；超出后按清理策略处理"));
    m_spinMax = new DSpinBox(rMax);
    m_spinMax->setRange(0, 99);
    m_spinMax->setSuffix(tr(" 个"));
    m_spinMax->setMinimumWidth(120);
    m_spinMax->setValue(m_appCfg.value(QStringLiteral("max_snapshots")).toInt(0));
    rMax->addControl(m_spinMax);
    secPolicy->content()->addWidget(rMax);

    // 超限自动清理（从属于「备份数量上限」：上限=0 时无清理可言，开关禁用）
    auto *rAutoPrune = new PanshiRow(secPolicy);
    m_rowAutoPrune = rAutoPrune;
    rAutoPrune->setTitle(tr("超限自动清理"));
    m_swAutoPrune = new DSwitchButton(rAutoPrune);
    m_swAutoPrune->setChecked(m_appCfg.value(QStringLiteral("auto_prune")).toBool(true));
    rAutoPrune->addControl(m_swAutoPrune);
    secPolicy->content()->addWidget(rAutoPrune);

    // 上限/清理联动：上限=0（不限）→ 清理开关禁用并换提示文案；>0 → 恢复
    const auto applyPruneLinkage = [this]() {
        const bool limited = m_spinMax->value() > 0;
        m_swAutoPrune->setEnabled(limited);
        m_rowAutoPrune->setDescription(limited
            ? tr("仅删除最旧普通备份；初始备份永不删除")
            : tr("上限为不限时无需自动清理"));
    };
    applyPruneLinkage();
    connect(m_spinMax, QOverload<int>::of(&DSpinBox::valueChanged), this, [this, applyPruneLinkage](int v) {
        m_appCfg.insert(QStringLiteral("max_snapshots"), v);
        saveAppConfig(m_appCfg);
        applyPruneLinkage();
    });
    connect(m_swAutoPrune, &DSwitchButton::toggled, this, [this](bool v) {
        m_appCfg.insert(QStringLiteral("auto_prune"), v);
        saveAppConfig(m_appCfg);
    });

    lay->addWidget(secPolicy);

    // ================= 分组2: 更新与回滚 =================
    // 回滚全链路聚合：失败回滚 → 回滚前备份清理 → 保留旧部署数，一处评估
    auto *secRollback = new PanshiSection(
        tr("更新与回滚"),
        tr("以下策略控制系统升级失败时的回滚与清理流程。"),
        scrollContent);
    secRollback->addHeaderWidget(new PanshiBadge(tr("即时生效"), Semantic::Muted, secRollback));

    // 升级失败自动回滚
    auto *rRollback = new PanshiRow(secRollback);
    m_rowRollback = rRollback;
    rRollback->setTitle(tr("升级失败自动回滚"));
    rRollback->setDescription(tr("失败时回滚到升级前状态，避免系统无法启动"));
    m_swRollback = new DSwitchButton(rRollback);
    m_swRollback->setChecked(true);
    rRollback->addControl(m_swRollback);
    secRollback->content()->addWidget(rRollback);
    connect(m_swRollback, &DSwitchButton::toggled, this, [this](bool) { setSysDirty(); });

    // 回滚前备份清理
    auto *rRb = new PanshiRow(secRollback);
    m_rowRb = rRb;
    rRb->setTitle(tr("回滚前备份清理"));
    rRb->setDescription(tr("还原前自动创建回滚前备份；回滚成功后按所选方式清理"));
    m_cbRb = new DComboBox(rRb);
    m_cbRb->addItems({tr("手动清理（默认）"), tr("重启 3 次正常后自动清理")});
    m_cbRb->setMinimumWidth(180);
    m_cbRb->setCurrentIndex(m_appCfg.value(QStringLiteral("rb_auto_clean")).toString()
                                    == QLatin1String("reboot3") ? 1 : 0);
    rRb->addControl(m_cbRb);
    secRollback->content()->addWidget(rRb);
    connect(m_cbRb, QOverload<int>::of(&DComboBox::currentIndexChanged), this, [this](int idx) {
        m_appCfg.insert(QStringLiteral("rb_auto_clean"), idx == 1 ? QStringLiteral("reboot3")
                                              : QStringLiteral("manual"));
        saveAppConfig(m_appCfg);
    });

    // 保留旧部署数
    auto *rDeploy = new PanshiRow(secRollback);
    m_rowDeploy = rDeploy;
    rDeploy->setTitle(tr("保留旧部署数"));
    rDeploy->setDescription(tr("升级/回滚时保留的旧系统部署数量（默认 1，新备份自动覆盖旧备份）"));
    m_spinDeploy = new DSpinBox(rDeploy);
    m_spinDeploy->setRange(0, 9);
    m_spinDeploy->setSuffix(tr(" 个"));
    m_spinDeploy->setMinimumWidth(100);
    m_spinDeploy->setValue(1);
    rDeploy->addControl(m_spinDeploy);
    secRollback->content()->addWidget(rDeploy);
    connect(m_spinDeploy, QOverload<int>::of(&DSpinBox::valueChanged), this, [this](int) { setSysDirty(); });

    lay->addWidget(secRollback);

    // ================= 分组3: 磐石系统参数 =================
    // 需管理员权限 + 点保存才生效 —— 与组1/组2「即时生效」形成显性区分；
    // 任一控件改动 → 组头出现「有未保存修改」徽章，保存成功后清除
    auto *secSys = new PanshiSection(
        tr("磐石系统参数"),
        tr("以下参数写入系统配置文件，需管理员权限；修改后点击「保存系统参数」生效。"),
        scrollContent);
    m_badgeDirty = new PanshiBadge(tr("有未保存修改"), Semantic::Warning, secSys);
    m_badgeDirty->hide();
    secSys->addHeaderWidget(m_badgeDirty);
    auto *bSave = Ui::button(tr("保存系统参数"), true);
    connect(bSave, &DPushButton::clicked, this, &PolicyPage::saveSysConfig);
    secSys->addHeaderWidget(bSave);

    // 自动刷新阈值
    auto *rRefresh = new PanshiRow(secSys);
    m_rowRefresh = rRefresh;
    rRefresh->setTitle(tr("自动刷新阈值"));
    rRefresh->setDescription(tr("修改层超过该阈值时自动刷新合并到系统层；建议 200–1000 MiB"));
    m_spinRefresh = new DSpinBox(rRefresh);
    m_spinRefresh->setRange(50, 2000);
    m_spinRefresh->setSuffix(tr(" MiB"));
    m_spinRefresh->setMinimumWidth(110);
    m_spinRefresh->setValue(200);
    rRefresh->addControl(m_spinRefresh);
    secSys->content()->addWidget(rRefresh);
    connect(m_spinRefresh, QOverload<int>::of(&DSpinBox::valueChanged), this, [this](int) { setSysDirty(); });

    // 配置文件（只读信息行）：路径中部省略、不折行，窄窗不撑破布局
    auto *rInfo = new PanshiRow(secSys);
    rInfo->setTitle(tr("配置文件"));
    rInfo->setDescription(tr("保存系统参数的目标文件"));
    auto *infoPath = new Ui::ElidedLabel(
        QStringLiteral("/etc/deepin-immutable-ctl/deepin-immutable-ctl.conf"), rInfo);
    infoPath->setForegroundRole(DPalette::TextTips);
    DFontSizeManager::instance()->bind(infoPath, DFontSizeManager::T8);
    rInfo->addControl(infoPath, 0);
    secSys->content()->addWidget(rInfo);

    lay->addWidget(secSys);
    lay->addStretch();

    // ---- 设置搜索（阶段 4）：登记本页可定位的行 ----
    // key 用行的标题文案，与 searchableSettings() 的 targetKey 一致。
    // 只读信息行（配置文件）不是设置项，不参与搜索定位。
    for (PanshiRow *r : {m_rowMax, m_rowAutoPrune, m_rowRollback,
                         m_rowRb, m_rowDeploy, m_rowRefresh}) {
        if (r)
            registerSearchTarget(r->title(), r, scroll);
    }
}

QList<PanshiUi::SettingEntry> PolicyPage::searchableSettings() const
{
    QList<PanshiUi::SettingEntry> out;
    const int pid = static_cast<int>(PageId::Policy);

    // 分组名只用于结果列表的次级说明，帮助用户建立"它在哪一组"的空间感
    const QString grpBackup = tr("备份保留");
    const QString grpRollback = tr("更新与回滚");
    const QString grpSys = tr("磐石系统参数");

    const QList<QPair<PanshiRow *, QString>> defs = {
        {m_rowMax, grpBackup},
        {m_rowAutoPrune, grpBackup},
        {m_rowRollback, grpRollback},
        {m_rowRb, grpRollback},
        {m_rowDeploy, grpRollback},
        {m_rowRefresh, grpSys},
    };
    for (const auto &d : defs) {
        if (!d.first)
            continue;
        PanshiUi::SettingEntry e;
        e.pageId = pid;
        e.group = d.second;
        e.title = d.first->title();
        e.description = d.first->description();
        e.targetKey = e.title;
        out.append(e);
    }
    return out;
}

QJsonObject PolicyPage::loadAppConfig()
{
    QJsonObject cfg;
    cfg.insert(QStringLiteral("auto_prune"), true);
    cfg.insert(QStringLiteral("max_snapshots"), 0);
    cfg.insert(QStringLiteral("rb_auto_clean"), QStringLiteral("manual"));
    QFile f(configPath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject saved = doc.object();
            for (auto it = saved.constBegin(); it != saved.constEnd(); ++it)
                cfg.insert(it.key(), it.value());
        }
    }
    return cfg;
}

void PolicyPage::saveAppConfig(const QJsonObject &cfg)
{
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
    }
}

void PolicyPage::refresh()
{
    m_busy->setText(tr("正在读取配置..."));
    m_busy->start();
    setMaskLoadingEnabled(false);   // 快任务走 L2 局部加载，不盖整页
    runAsync([]() -> QVariant {
        QVariantMap d;
        const QMap<QString, QString> rawCfg = getCtlConfig();
        QVariantMap cfg;
        for (auto it = rawCfg.constBegin(); it != rawCfg.constEnd(); ++it)
            cfg.insert(it.key(), it.value());
        d.insert(QStringLiteral("cfg"), cfg);
        d.insert(QStringLiteral("state"), QVariant::fromValue(getRollbackState()));
        return d;
    }, [this](const QVariant &res) { onLoaded(res); });
}

void PolicyPage::onLoaded(const QVariant &res)
{
    m_busy->stop();

    const QVariantMap d = res.toMap();
    QMap<QString, QString> cfg;
    const QVariantMap cfgV = d.value(QStringLiteral("cfg")).toMap();
    for (auto it = cfgV.constBegin(); it != cfgV.constEnd(); ++it)
        cfg.insert(it.key(), it.value().toString());
    const RollbackState st = d.value(QStringLiteral("state")).value<RollbackState>();

    // 程序化回填不视为用户改动：置加载标志，抑制组3的未保存脏标记
    m_loadingSys = true;
    m_swRollback->setChecked(cfg.value(QStringLiteral("auto-rollback-on-failure"), QStringLiteral("true"))
                                     .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0);
    bool ok = false;
    int v = cfg.value(QStringLiteral("max-backup-deploys"), QStringLiteral("1")).toInt(&ok);
    m_spinDeploy->setValue(ok ? v : 1);
    v = cfg.value(QStringLiteral("auto-refresh-size-threshold"), QStringLiteral("200")).toInt(&ok);
    m_spinRefresh->setValue(ok ? v : 200);
    if (st.valid && st.strategy == QLatin1String("reboot3"))
        m_cbRb->setCurrentIndex(1);
    m_loadingSys = false;

    // 刷新（切页）后后端配置覆盖未保存修改，脏标记随之一并清除
    clearSysDirty();
}

void PolicyPage::saveSysConfig()
{
    QMap<QString, QString> updates;
    updates.insert(QStringLiteral("auto-rollback-on-failure"),
                   m_swRollback->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    updates.insert(QStringLiteral("max-backup-deploys"), QString::number(m_spinDeploy->value()));
    updates.insert(QStringLiteral("auto-refresh-size-threshold"), QString::number(m_spinRefresh->value()));

    m_busy->setText(tr("正在保存系统参数..."));
    m_busy->start();
    setMaskLoadingEnabled(false);
    runAsync([updates]() -> QVariant {
        const CommandResult r = setCtlConfig(updates);
        return QVariantMap{{QStringLiteral("ok"), r.ok}, {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            clearSysDirty();
            m_banner->setSemantic(Semantic::Success);
            m_banner->setText(tr("系统参数已保存"));
            m_banner->setClosable(true);
            m_banner->autoDismiss(3000);
        } else {
            // 保存失败：未保存修改仍在，保留脏标记
            m_banner->setSemantic(Semantic::Danger);
            m_banner->setText(tr("保存失败: %1").arg(m.value(QStringLiteral("msg")).toString()));
            m_banner->setClosable(true);
        }
    });
}

void PolicyPage::setSysDirty()
{
    if (m_loadingSys)
        return;
    m_sysDirty = true;
    m_badgeDirty->show();
}

void PolicyPage::clearSysDirty()
{
    m_sysDirty = false;
    m_badgeDirty->hide();
}
