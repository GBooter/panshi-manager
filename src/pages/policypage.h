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
 * 策略与设置页（阶段 3-3 重写）：
 *  - 表单改 PanshiRow 行式（左标题+说明 / 右控件），替代手拼 QHBoxLayout+rowDesc；
 *  - 反馈走 PanshiInlineBanner（保存成功 3s 自动消失 / 失败常驻可关），不再弹模态窗；
 *  - 「保存系统参数」上移为「磐石系统参数」分组的组级操作（标题行右侧）；
 *  - 读取/保存系统参数走 L2 局部加载，不再盖整页。
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

    MainWindow *m_win = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    /** 设置搜索用的行引用（与上面控件成员一一对应；阶段 4） */
    PanshiUi::PanshiRow *m_rowAutoPrune = nullptr;
    PanshiUi::PanshiRow *m_rowMax = nullptr;
    PanshiUi::PanshiRow *m_rowRb = nullptr;
    PanshiUi::PanshiRow *m_rowRollback = nullptr;
    PanshiUi::PanshiRow *m_rowDeploy = nullptr;
    PanshiUi::PanshiRow *m_rowRefresh = nullptr;
    DSwitchButton *m_swAutoPrune = nullptr;
    DSpinBox *m_spinMax = nullptr;
    DComboBox *m_cbRb = nullptr;
    DSwitchButton *m_swRollback = nullptr;
    DSpinBox *m_spinDeploy = nullptr;
    DSpinBox *m_spinRefresh = nullptr;
    QJsonObject m_appCfg;
};
