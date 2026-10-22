// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
#include "ui/components.h"
#include "../backend.h"

DWIDGET_USE_NAMESPACE

class MainWindow;

/**
 * 系统更新页（2026-10-06 界面重设计）：
 *  - 「更新检查」状态卡持久展示上次检查结果（图标 + 一句话结论 + 状态徽章），
 *    取代"结果只闪现 3 秒就消失"的旧体验；
 *  - 检查更新完成后把底层工具的英文摘要（Total count:… / Need download count:…）
 *    解析为中文友好文案（"发现 N 个可更新项，需下载 M 个" / "系统已是最新"）；
 *  - 「待更新软件包」区（2026-10-18 新增）：ctl 只报计数不报名单，用户看不到
 *    "要更新什么"。检查完成且存在可更新项时，从 apt 只读视图取明细，按用途
 *    分组展示（内核与驱动 / 桌面环境 / 输入法与字体 / …），回答"装的是什么"；
 *  - 反馈走 PanshiInlineBanner（成功自动消失 / 失败常驻可关闭）；
 *  - 「立即重启」挂在成功 Banner 上，随升级结果出现；
 *  - 任务进行中用 L2 局部加载，不盖整页；
 *  - 原始命令输出收进 PanshiCollapsible，主界面只留结论。
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
    /** 更新「更新检查」状态卡（图标/结论/徽章），并返回中文结论文案 */
    QString updateStatusCard(PanshiUi::Semantic s, const QString &summary);
    /**
     * 把 apt 明细按用途分类渲染进「待更新软件包」区（无明细时隐藏整区）。
     * 分组容器是构造时建好的内层 widget，重渲染前只清它的布局 ——
     * PanshiSection::content() 里还有 header 布局，整片 takeAt 会连标题一起删掉。
     */
    void renderPackageGroups(const QList<Panshi::UpgradablePackage> &pkgs);

    MainWindow *m_win = nullptr;
    PanshiUi::PanshiInlineBanner *m_banner = nullptr;
    PanshiUi::PanshiLocalLoading *m_busy = nullptr;
    PanshiUi::PanshiCollapsible *m_output = nullptr;
    DLabel *m_outputText = nullptr;
    DPushButton *m_restartBtn = nullptr;

    // 「更新检查」状态卡
    DLabel *m_statusIcon = nullptr;
    DLabel *m_statusLine = nullptr;
    PanshiUi::PanshiBadge *m_statusBadge = nullptr;

    // 「待更新软件包」区（有明细才可见）
    PanshiUi::PanshiSection *m_secPackages = nullptr;
    QWidget *m_pkgGroupsHost = nullptr;

    bool m_running = false;
};
