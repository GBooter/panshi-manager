// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitorconfig.h"

#include <QDebug>
#include <QStandardPaths>

namespace Panshi {
namespace {

/**
 * 是否强制降级到 QSettings（默认否）。
 * 排障开关：PANSHI_CONFIG_QSETTINGS=1 可在 DConfig 可用时对比降级路径的行为。
 */
bool envForceQSettings()
{
    const QString v = QString::fromLocal8Bit(qgetenv("PANSHI_CONFIG_QSETTINGS")).trimmed();
    return v == QLatin1String("1") || v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

} // namespace

MonitorConfig::MonitorConfig()
{
    m_dconfig = Dtk::Core::DConfig::create(QStringLiteral("org.deepin.panshi-manager"),
                                           QStringLiteral("org.deepin.panshi-manager"));
    if (m_dconfig && !m_dconfig->isValid()) {
        delete m_dconfig;
        m_dconfig = nullptr;
    }

    m_forceQSettings = envForceQSettings();

    // 配置源优先级（遵循 deepin 规范）：
    //
    //   1. DConfig（默认，主读取源）
    //      dtk 官方 gotchas.md §4.1 明确把「用 QSettings 替代 DConfig」列为反模式，
    //      原因是那样会失去系统策略覆盖能力、且不兼容 dde-application-manager。
    //      因此只要 DConfig 后端可用，就以它为准。
    //
    //   2. QSettings（降级）
    //      仅在 DConfig 不可用（schema 未安装到 /usr/share/dsg/configs、或后端不可达）
    //      时生效。写入时始终双写，保证两条路径的值一致、切换时配置不丢。
    //
    // 历史教训（2026-10-04 → 10-05 修正）：
    //   曾因 monitor.enabled 写入后读回恒为 false，误判为「本机 DConfig 不可写」，
    //   一度把 QSettings 提为唯一读取源。真实根因是 **schema 里所有键都缺
    //   `permissions` 字段** —— concepts.md 规定该字段「为空时只读」，于是
    //   dde-dconfig set 退出码 0 却被 daemon 拒绝。对照实验可复现：
    //     · permissions=readwrite 的键 → set/get 往返生效
    //     · permissions=readonly 的键  → 退出码 0，读回不变（与本故障现象一致）
    //   现已为全部 16 个键补上 "permissions": "readwrite"。
    if (!m_dconfig && !m_forceQSettings) {
        qWarning() << "[MonitorConfig] DConfig 不可用（schema 未安装或后端不可达），"
                   << "降级到 QSettings：" << QStandardPaths::writableLocation(
                          QStandardPaths::ConfigLocation) << "/deepin/panshi-manager.conf";
    }
}

MonitorConfig::~MonitorConfig()
{
    delete m_dconfig;
}

bool MonitorConfig::usingDConfig() const
{
    return m_dconfig != nullptr && !m_forceQSettings;
}

QVariant MonitorConfig::get(const QString &key, const QVariant &def) const
{
    if (usingDConfig())
        return m_dconfig->value(key, def);
    // QSettings 的 INI 层级用 '/' 分隔：monitor.enabled → monitor/enabled
    QString qk = key;
    qk.replace(QLatin1Char('.'), QLatin1Char('/'));
    return m_qs.value(qk, def);
}

void MonitorConfig::setValue(const QString &key, const QVariant &v)
{
    // 主源：DConfig（deepin 规范要求的持久化位置）
    if (m_dconfig)
        m_dconfig->setValue(key, v);

    // 镜像：QSettings。两条路径都写，保证 DConfig 不可用降级时配置不丢失。
    QString qk = key;
    qk.replace(QLatin1Char('.'), QLatin1Char('/'));
    m_qs.setValue(qk, v);
    // GUI 改完设置后，后台 --monitor 进程是另一个进程，必须立刻落盘
    m_qs.sync();
}

bool MonitorConfig::enabled() const
{
    return get(QStringLiteral("monitor.enabled"), false).toBool();
}

void MonitorConfig::setEnabled(bool v)
{
    setValue(QStringLiteral("monitor.enabled"), v);
}

int MonitorConfig::pollMinutes() const
{
    return get(QStringLiteral("monitor.pollMinutes"), 15).toInt();
}

int MonitorConfig::baselineDays() const
{
    return get(QStringLiteral("monitor.baselineDays"), 7).toInt();
}

QString MonitorConfig::alertMode() const
{
    return get(QStringLiteral("monitor.alertMode"), QStringLiteral("baseline")).toString();
}

int MonitorConfig::fixedThresholdMB() const
{
    return get(QStringLiteral("monitor.fixedThresholdMB"), 1024).toInt();
}

double MonitorConfig::baselineMultiplier() const
{
    return get(QStringLiteral("monitor.baselineMultiplier"), 3.0).toDouble();
}

int MonitorConfig::minAlertMB() const
{
    return get(QStringLiteral("monitor.minAlertMB"), 512).toInt();
}

int MonitorConfig::cooldownHours() const
{
    return get(QStringLiteral("monitor.cooldownHours"), 6).toInt();
}

QString MonitorConfig::quietStart() const
{
    return get(QStringLiteral("monitor.quietStart"), QStringLiteral("23:00")).toString();
}

QString MonitorConfig::quietEnd() const
{
    return get(QStringLiteral("monitor.quietEnd"), QStringLiteral("07:00")).toString();
}

QStringList MonitorConfig::excludes() const
{
    return get(QStringLiteral("monitor.excludes"), QStringList()).toStringList();
}

int MonitorConfig::retentionDays() const
{
    return get(QStringLiteral("monitor.retentionDays"), 400).toInt();
}

int MonitorConfig::storageBudgetMB() const
{
    return get(QStringLiteral("monitor.storageBudgetMB"), 512).toInt();
}

int MonitorConfig::samplesRetentionDays() const
{
    return get(QStringLiteral("monitor.samplesRetentionDays"), 90).toInt();
}

int MonitorConfig::memoryCacheDays() const
{
    return get(QStringLiteral("monitor.memoryCacheDays"), 90).toInt();
}

QString MonitorConfig::snoozeUntil() const
{
    return get(QStringLiteral("monitor.snoozeUntil"), QString()).toString();
}

void MonitorConfig::setSnoozeUntil(const QString &iso)
{
    setValue(QStringLiteral("monitor.snoozeUntil"), iso);
}

} // namespace Panshi
