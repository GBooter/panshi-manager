// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QSettings>
#include <DConfig>

namespace Panshi {

/**
 * 监控配置：以 DConfig（org.deepin.panshi-manager）为主，
 * 开发环境无 DConfig meta 时自动回退 QSettings（deepin/panshi-manager）。
 * GUI 与监控进程共用，键名见 data/dsg/configs 下的 schema。
 */
class MonitorConfig
{
public:
    MonitorConfig();
    ~MonitorConfig();

    bool enabled() const;
    void setEnabled(bool v);

    int pollMinutes() const;
    int baselineDays() const;
    QString alertMode() const;            // baseline / fixed
    int fixedThresholdMB() const;
    double baselineMultiplier() const;
    int minAlertMB() const;
    int cooldownHours() const;
    QString quietStart() const;
    QString quietEnd() const;
    QStringList excludes() const;         // 原始字符串（含 ~），展开见工具函数
    int retentionDays() const;
    int storageBudgetMB() const;
    int samplesRetentionDays() const;
    int memoryCacheDays() const;
    QString snoozeUntil() const;
    void setSnoozeUntil(const QString &iso);

    /** 通用写接口（GUI 设置页使用），键名见 DConfig schema */
    void setRawValue(const QString &key, const QVariant &v) { setValue(key, v); }

    /**
     * 当前是否以 DConfig 为读取源。
     * DConfig 后端可用时为 true（deepin 规范要求的默认路径）；
     * 只有在 DConfig 不可用（schema 未安装 / 后端不可达）时才降级到 QSettings。
     * 排障时可用环境变量 PANSHI_CONFIG_QSETTINGS=1 强制降级，对比两种源的行为。
     */
    bool usingDConfig() const;

private:
    QVariant get(const QString &key, const QVariant &def) const;
    void setValue(const QString &key, const QVariant &v);

    Dtk::Core::DConfig *m_dconfig = nullptr;
    /** 排障开关：强制降级到 QSettings（默认 false，即优先 DConfig） */
    bool m_forceQSettings = false;
    QSettings m_qs{QStringLiteral("deepin"), QStringLiteral("panshi-manager")};
};

} // namespace Panshi
