// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file confirmdialog.h
 * 全应用唯一的危险/决策确认组件（UI 重构方案 §4.3）
 *
 * 规则：
 *  - 信息类 / 成功类 / 失败类反馈不再用模态弹窗，改用 PanshiInlineBanner；
 *    本文件只服务"需要用户决策"的场景。
 *  - 确认按钮文案必须是动作动词（「删除」「还原」「完全关闭」）。
 *  - countdownSeconds > 0 时走倒计时对话框，保留不可逆操作的安全等待。
 */

#include "components.h"

#include <QString>
#include <QStringList>
#include <QWidget>

DWIDGET_USE_NAMESPACE

namespace PanshiUi {

enum class DialogLevel
{
    Normal,    // 普通决策（Recommend 按钮）
    Warning,   // 会造成不可逆改变（Warning 按钮）
    Danger,    // 会破坏数据/降低安全性（Warning 按钮 + 后果清单强制）
};

struct ConfirmRequest
{
    QString title;              // 标题，必须是动作："删除备份"
    QString message;            // 一句话说明（可空）
    QStringList facts;          // 摘要行："目标备份: xxx"
    QStringList consequences;   // 后果清单，level >= Warning 时应填写
    QString confirmText;        // 动作动词，禁止「确定」
    QString cancelText;         // 默认「取消」
    DialogLevel level = DialogLevel::Normal;
    int countdownSeconds = 0;   // >0 使用倒计时对话框（不可跳过的安全等待）
    QStringList options;        // 高级选项（单选，可空）
    int optionDefault = 0;
    int *optionResult = nullptr;
};

/** 通用确认：返回用户是否确认 */
bool askConfirm(QWidget *parent, const ConfirmRequest &req);

/** 便捷：普通确认 */
bool askConfirm(QWidget *parent, const QString &title, const QString &message,
                const QString &confirmText);

/** 便捷：危险确认（Warning 按钮 + 后果清单） */
bool askDanger(QWidget *parent, const QString &title, const QString &message,
               const QString &confirmText, const QStringList &consequences = QStringList());

/**
 * 「显示详情」折叠块：把后端原始输出折叠起来（方案 §4.4）。
 * 用户默认只看到可行动的说明。
 */
QWidget *createDetailsBox(const QString &title, const QString &rawText,
                          QWidget *parent = nullptr);

} // namespace PanshiUi
