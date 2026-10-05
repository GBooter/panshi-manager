// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"

DWIDGET_USE_NAMESPACE

class MainWindow;

/**
 * 系统更新页（阶段 3-2 重写）：
 *  - 反馈走 PanshiInlineBanner（成功自动消失 / 失败常驻可关闭），不再弹模态窗；
 *  - 「立即重启」挂在成功 Banner 上，随升级结果出现（成功反馈不弹窗，方案 §4.4）；
 *  - 任务进行中用 L2 局部加载（setMaskLoadingEnabled(false)），不再盖整页；
 *  - 命令长输出收进 PanshiCollapsible，主界面只留结论。
 */
class UpdatePage : public BasePage
{
    Q_OBJECT
public:
    explicit UpdatePage(MainWindow *win);

    void refresh() override {}

    /** 对外动作契约（阶段 5 跨页跳转）：从总览页跳来后自动触发一次检查更新 */
    Q_INVOKABLE void requestCheckUpdate() { onCheck(); }

private slots:
    void onCheck();
    void onUpgrade();
    void onClean();

private:
    void showResult(PanshiUi::Semantic s, const QString &text, bool autoHide);
    void setOutput(const QString &out);

    MainWindow *m_win = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiCollapsible *m_output = nullptr;
    DLabel *m_outputText = nullptr;
    DPushButton *m_restartBtn = nullptr;
    bool m_running = false;
};
