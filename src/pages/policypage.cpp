// SPDX-License-Identifier: GPL-3.0-or-later
#include "policypage.h"
#include "common.h"
#include "../mainwindow.h"
#include "../backend.h"

#include <DSwitchButton>
#include <DSpinBox>
#include <DComboBox>

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
    // 注册为基类反馈宿主：否则 showFeedback()/showFeedbackWithDetails()
    // 因 m_feedback == nullptr 而**静默跳过**，本页反馈全部丢失（O11 连带修复）
    setFeedbackBanner(m_banner);
    m_banner->hide();
    lay->addWidget(m_banner);

    m_busy = new PanshiLocalLoading(QString(), scrollContent);
    m_busy->hide();
    lay->addWidget(m_busy);

    // ---- 分组1: 备份保留策略 ----
    auto *secPolicy = new PanshiSection(tr("备份保留策略"), QString(), scrollContent);
    auto *policyDesc = makeLabel(
        tr("磐石系统升级时会自动创建备份(系统内置, 不可关闭)。应用通过以下策略避免备份无限累积。"),
        TextLevel::Desc, Semantic::Muted);
    policyDesc->setWordWrap(true);
    secPolicy->content()->addWidget(policyDesc);

    auto *rAutoPrune = new PanshiRow(secPolicy);
    m_rowAutoPrune = rAutoPrune;
    rAutoPrune->setTitle(tr("自动清理超额备份"));
    rAutoPrune->setDescription(tr("超过数量上限时, 自动删除最旧的普通备份(初始备份永不删除)"));
    m_swAutoPrune = new DSwitchButton(rAutoPrune);
    m_swAutoPrune->setChecked(m_appCfg.value(QStringLiteral("auto_prune")).toBool(true));
    connect(m_swAutoPrune, &DSwitchButton::toggled, this, [this](bool v) {
        m_appCfg.insert(QStringLiteral("auto_prune"), v);
        saveAppConfig(m_appCfg);
    });
    rAutoPrune->addControl(m_swAutoPrune);
    secPolicy->content()->addWidget(rAutoPrune);

    auto *rMax = new PanshiRow(secPolicy);
    m_rowMax = rMax;
    rMax->setTitle(tr("备份数量上限"));
    rMax->setDescription(tr("超过上限后自动删除最旧的普通备份, 释放存储空间"));
    m_spinMax = new DSpinBox(rMax);
    m_spinMax->setRange(0, 99);
    m_spinMax->setSuffix(tr(" 个 (0=不限)"));
    m_spinMax->setMinimumWidth(120);
    m_spinMax->setValue(m_appCfg.value(QStringLiteral("max_snapshots")).toInt(0));
    connect(m_spinMax, QOverload<int>::of(&DSpinBox::valueChanged), this, [this](int v) {
        m_appCfg.insert(QStringLiteral("max_snapshots"), v);
        saveAppConfig(m_appCfg);
    });
    rMax->addControl(m_spinMax);
    secPolicy->content()->addWidget(rMax);

    auto *rRb = new PanshiRow(secPolicy);
    m_rowRb = rRb;
    rRb->setTitle(tr("回滚前备份自动清理"));
    rRb->setDescription(tr("还原前自动创建“回滚前备份”, 回滚成功后重启3次正常即自动删除该备份"));
    m_cbRb = new DComboBox(rRb);
    m_cbRb->addItems({tr("手动删除(默认, 最安全)"), tr("重启3次正常后自动删除")});
    m_cbRb->setMinimumWidth(180);
    m_cbRb->setCurrentIndex(m_appCfg.value(QStringLiteral("rb_auto_clean")).toString()
                                    == QLatin1String("reboot3") ? 1 : 0);
    connect(m_cbRb, QOverload<int>::of(&DComboBox::currentIndexChanged), this, [this](int idx) {
        m_appCfg.insert(QStringLiteral("rb_auto_clean"), idx == 1 ? QStringLiteral("reboot3")
                                                                  : QStringLiteral("manual"));
        saveAppConfig(m_appCfg);
    });
    rRb->addControl(m_cbRb);
    secPolicy->content()->addWidget(rRb);

    lay->addWidget(secPolicy);

    // ---- 分组2: 磐石系统参数（保存按钮上移为组级操作）----
    auto *secSys = new PanshiSection(
        tr("磐石系统参数"),
        tr("写入 /etc/deepin-immutable-ctl/deepin-immutable-ctl.conf (需管理员权限)"),
        scrollContent);
    auto *bSave = Ui::button(tr("保存系统参数"), true);
    connect(bSave, &DPushButton::clicked, this, &PolicyPage::saveSysConfig);
    secSys->addHeaderWidget(bSave);

    auto *rRollback = new PanshiRow(secSys);
    m_rowRollback = rRollback;
    rRollback->setTitle(tr("升级失败自动回滚"));
    rRollback->setDescription(tr("系统升级失败时自动回滚到升级前状态, 避免系统无法启动"));
    m_swRollback = new DSwitchButton(rRollback);
    m_swRollback->setChecked(true);
    rRollback->addControl(m_swRollback);
    secSys->content()->addWidget(rRollback);

    auto *rDeploy = new PanshiRow(secSys);
    m_rowDeploy = rDeploy;
    rDeploy->setTitle(tr("最大备份部署数"));
    rDeploy->setDescription(tr("升级/回滚时保留的旧系统部署数量(默认1, 新备份自动覆盖旧备份)"));
    m_spinDeploy = new DSpinBox(rDeploy);
    m_spinDeploy->setRange(0, 9);
    m_spinDeploy->setSuffix(tr(" 个"));
    m_spinDeploy->setMinimumWidth(100);
    m_spinDeploy->setValue(1);
    rDeploy->addControl(m_spinDeploy);
    secSys->content()->addWidget(rDeploy);

    auto *rRefresh = new PanshiRow(secSys);
    m_rowRefresh = rRefresh;
    rRefresh->setTitle(tr("自动刷新阈值"));
    rRefresh->setDescription(tr("修改层超过该阈值时自动刷新合并到系统层"));
    m_spinRefresh = new DSpinBox(rRefresh);
    m_spinRefresh->setRange(50, 2000);
    m_spinRefresh->setSuffix(tr(" MiB"));
    m_spinRefresh->setMinimumWidth(110);
    m_spinRefresh->setValue(200);
    rRefresh->addControl(m_spinRefresh);
    secSys->content()->addWidget(rRefresh);

    lay->addWidget(secSys);
    lay->addStretch();

    // ---- 设置搜索（阶段 4）：登记本页可定位的行 ----
    // key 用行的标题文案，与 searchableSettings() 的 targetKey 一致。
    for (PanshiRow *r : {m_rowAutoPrune, m_rowMax, m_rowRb,
                         m_rowRollback, m_rowDeploy, m_rowRefresh}) {
        if (r)
            registerSearchTarget(r->title(), r, scroll);
    }
}

QList<PanshiUi::SettingEntry> PolicyPage::searchableSettings() const
{
    QList<PanshiUi::SettingEntry> out;
    const int pid = static_cast<int>(PageId::Policy);

    // 分组名只用于结果列表的次级说明，帮助用户建立"它在哪一组"的空间感
    const QString grpPolicy = tr("备份保留策略");
    const QString grpSys = tr("磐石系统参数");

    const QList<QPair<PanshiRow *, QString>> defs = {
        {m_rowAutoPrune, grpPolicy},
        {m_rowMax, grpPolicy},
        {m_rowRb, grpPolicy},
        {m_rowRollback, grpSys},
        {m_rowDeploy, grpSys},
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

    m_swRollback->setChecked(cfg.value(QStringLiteral("auto-rollback-on-failure"), QStringLiteral("true"))
                                     .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0);
    bool ok = false;
    int v = cfg.value(QStringLiteral("max-backup-deploys"), QStringLiteral("1")).toInt(&ok);
    m_spinDeploy->setValue(ok ? v : 1);
    v = cfg.value(QStringLiteral("auto-refresh-size-threshold"), QStringLiteral("200")).toInt(&ok);
    m_spinRefresh->setValue(ok ? v : 200);
    if (st.valid && st.strategy == QLatin1String("reboot3"))
        m_cbRb->setCurrentIndex(1);
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
            m_banner->setSemantic(Semantic::Success);
            m_banner->setText(tr("系统参数已保存"));
            m_banner->setClosable(true);
            m_banner->autoDismiss(3000);
        } else {
            m_banner->setSemantic(Semantic::Danger);
            m_banner->setText(tr("保存失败: %1").arg(m.value(QStringLiteral("msg")).toString()));
            m_banner->setClosable(true);
        }
    });
}
