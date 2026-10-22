// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"

#include <QJsonObject>
#include <DSwitchButton>
#include <DSpinBox>
#include <DComboBox>
DWIDGET_USE_NAMESPACE

class MainWindow;

/**
 * 策略与设置页（2026-10-06 信息架构重排）：
 *  - 三组按流程主题重排：备份保留（上限+清理联动）/ 更新与回滚（回滚全链路）/
 *    磐石系统参数（阈值 + 配置文件信息 + 保存按钮）；
 *  - 组1/组2 组头挂「即时生效」徽章，组3 改动后出现「有未保存修改」警告徽章，
 *    保存成功清除 —— 两种保存模型（即时写 config.json vs 需权限手动保存）显性区分；
 *  - 备份数量上限与超限自动清理联动：上限=0（不限）时清理开关禁用；
 *  - 反馈走 PanshiInlineBanner，读取/保存走 L2 局部加载（阶段 3-3 行为保留）。
 */
class PolicyPage : public BasePage
{
    Q_OBJECT
public:
    explicit PolicyPage(MainWindow *win);

    void refresh() override;

    /** 自报可搜索设置项（阶段 4 设置搜索） */
    QList<PanshiUi::SettingEntry> searchableSettings() const override;

private slots:
    void saveSysConfig();

private:
    void onLoaded(const QVariant &res);
    QJsonObject loadAppConfig();
    void saveAppConfig(const QJsonObject &cfg);
    /** 组3任一控件被用户改动时置未保存标记（系统参数需点保存才生效） */
    void setSysDirty();
    void clearSysDirty();

    MainWindow *m_win = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    /** 组3「有未保存修改」徽章（默认隐藏，改动显示、保存成功清除） */
    PanshiUi::PanshiBadge *m_badgeDirty = nullptr;
    /** 设置搜索用的行引用（与控件成员一一对应；阶段 4） */
    PanshiUi::PanshiRow *m_rowMax = nullptr;       // 备份数量上限
    PanshiUi::PanshiRow *m_rowAutoPrune = nullptr; // 超限自动清理
    PanshiUi::PanshiRow *m_rowRollback = nullptr;  // 升级失败自动回滚
    PanshiUi::PanshiRow *m_rowRb = nullptr;        // 回滚前备份清理
    PanshiUi::PanshiRow *m_rowDeploy = nullptr;    // 保留旧部署数
    PanshiUi::PanshiRow *m_rowRefresh = nullptr;   // 自动刷新阈值
    DSwitchButton *m_swAutoPrune = nullptr;
    DSwitchButton *m_swRollback = nullptr;
    DSpinBox *m_spinMax = nullptr;
    DSpinBox *m_spinDeploy = nullptr;
    DSpinBox *m_spinRefresh = nullptr;
    DComboBox *m_cbRb = nullptr;
    QJsonObject m_appCfg;
    /** onLoaded 程序化回填值时置位，抑制脏标记（加载不是用户改动） */
    bool m_loadingSys = false;
    /** 组3是否有未保存修改（保存成功或刷新覆盖后清除） */
    bool m_sysDirty = false;
};
