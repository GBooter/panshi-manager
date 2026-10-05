// SPDX-License-Identifier: GPL-3.0-or-later
#include "updatepage.h"
#include "common.h"
#include "../mainwindow.h"
#include "../backend.h"

#include <QProcess>
#include <QVariantMap>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

using namespace Panshi;
using namespace PanshiUi;

UpdatePage::UpdatePage(MainWindow *win)
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
    lay->setSpacing(Space::L);

    lay->addWidget(Ui::pageHeader(tr("系统更新"), tr("通过磐石系统原子更新机制升级系统"),
                                 QStringLiteral("software-updates")));

    // ---- 结果反馈条（内联，替代模态弹窗）----
    m_banner = new PanshiInlineBanner(scrollContent);
    // 注册为基类反馈宿主：否则 showFeedback()/showFeedbackWithDetails()
    // 因 m_feedback == nullptr 而**静默跳过**，本页反馈全部丢失（O11 连带修复）
    setFeedbackBanner(m_banner);
    m_banner->hide();
    lay->addWidget(m_banner);
    // 「立即重启」动作只创建一次，升级成功后才显示，避免重复升级时按钮堆积
    m_restartBtn = m_banner->addAction(tr("立即重启"), true);
    m_restartBtn->hide();
    connect(m_restartBtn, &DPushButton::clicked, this, [] {
        QProcess::startDetached(QStringLiteral("systemctl"), QStringList() << QStringLiteral("reboot"));
    });

    // ---- 更新说明 ----
    auto *secInfo = new PanshiSection(tr("更新说明"), tr("磐石系统升级特性"), scrollContent);
    auto *desc = makeLabel(
        tr("· 升级前系统自动创建备份, 升级失败自动回滚到之前状态\n"
           "· 原子更新: 更新过程要么全部成功, 要么回退, 无需手动干预\n"
           "· 升级会创建新的系统部署, 旧部署按“最大备份部署数”策略保留\n"
           "· 升级完成需重启后进入新系统"), TextLevel::Desc, Semantic::Normal);
    desc->setWordWrap(true);
    secInfo->content()->addWidget(desc);
    lay->addWidget(secInfo);

    // ---- 操作 ----
    auto *secOps = new PanshiSection(tr("操作"), QString(), scrollContent);

    auto *row = new QHBoxLayout;
    row->setSpacing(Space::M);
    auto *bCheck = Ui::button(tr("检查更新"));
    connect(bCheck, &DPushButton::clicked, this, &UpdatePage::onCheck);
    auto *bUpgrade = Ui::button(tr("下载并升级"), true);
    connect(bUpgrade, &DPushButton::clicked, this, &UpdatePage::onUpgrade);
    auto *bClean = Ui::button(tr("清理下载缓存"));
    connect(bClean, &DPushButton::clicked, this, &UpdatePage::onClean);
    row->addWidget(bCheck);
    row->addWidget(bUpgrade);
    row->addWidget(bClean);
    row->addStretch();
    secOps->content()->addLayout(row);

    // L2 局部加载：任务进行时显示在按钮下方，其余内容保持可读
    m_busy = new PanshiLocalLoading(QString(), secOps);
    m_busy->hide();
    secOps->content()->addWidget(m_busy);

    // 长输出收进折叠区（默认整块隐藏，有输出才出现）
    m_output = new PanshiCollapsible(tr("完整输出"), secOps);
    m_outputText = makeLabel(QString(), TextLevel::Mono, Semantic::Normal);
    m_outputText->setWordWrap(true);
    m_outputText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_output->setContent(m_outputText);
    m_output->hide();
    secOps->content()->addWidget(m_output);

    lay->addWidget(secOps);
    lay->addStretch();
}

void UpdatePage::showResult(PanshiUi::Semantic s, const QString &text, bool autoHide)
{
    m_restartBtn->hide();
    m_banner->setSemantic(s);
    m_banner->setText(text);
    m_banner->setClosable(!autoHide);
    if (autoHide)
        m_banner->autoDismiss(3000);
}

void UpdatePage::setOutput(const QString &out)
{
    const QString trimmed = out.trimmed();
    m_outputText->setText(trimmed);
    m_output->setVisible(!trimmed.isEmpty());
}

void UpdatePage::onCheck()
{
    if (m_running)
        return;
    m_running = true;
    setOutput(QString());
    m_busy->setText(tr("正在检查更新..."));
    m_busy->start();
    setMaskLoadingEnabled(false);   // 快任务走 L2 局部加载，不盖整页
    runAsync([]() -> QVariant {
        const CommandResult r = checkUpgrade();
        return QVariantMap{{QStringLiteral("ok"), r.ok},
                           {QStringLiteral("out"), r.stdoutText},
                           {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            showResult(Semantic::Success, tr("检查完成"), true);
            setOutput(m.value(QStringLiteral("out")).toString());
        } else {
            showResult(Semantic::Danger,
                       tr("检查更新失败: %1").arg(m.value(QStringLiteral("msg")).toString()),
                       false);
        }
    });
}

void UpdatePage::onUpgrade()
{
    if (m_running)
        return;
    if (!Ui::confirmDialog(this, QObject::tr("系统升级"),
                       tr("将开始下载并升级系统。\n\n升级前系统会自动创建备份; 升级期间请勿关机或断电。\n确定继续吗？"),
                       tr("开始升级")))
        return;
    m_running = true;
    setOutput(QString());
    m_busy->setText(tr("正在下载并升级系统, 请耐心等待..."));
    m_busy->start();
    setMaskLoadingEnabled(false);
    runAsync([]() -> QVariant {
        const CommandResult r = runUpgrade();
        return QVariantMap{{QStringLiteral("ok"), r.ok},
                           {QStringLiteral("out"), r.stdoutText},
                           {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            // 成功反馈不弹窗：结论进 Banner，「立即重启」随结果出现（方案 §4.4）
            m_banner->setSemantic(Semantic::Success);
            m_banner->setText(tr("系统升级完成，需重启后进入新系统。"));
            m_banner->setClosable(true);
            m_restartBtn->setVisible(true);
            setOutput(m.value(QStringLiteral("out")).toString());
        } else {
            showResult(Semantic::Danger,
                       tr("升级失败: %1").arg(m.value(QStringLiteral("msg")).toString()),
                       false);
        }
    });
}

void UpdatePage::onClean()
{
    if (m_running)
        return;
    m_running = true;
    setOutput(QString());
    m_busy->setText(tr("正在清理下载缓存..."));
    m_busy->start();
    setMaskLoadingEnabled(false);
    runAsync([]() -> QVariant {
        const CommandResult r = cleanUpgradeCache(false);
        return QVariantMap{{QStringLiteral("ok"), r.ok},
                           {QStringLiteral("out"), r.stdoutText},
                           {QStringLiteral("msg"), r.errorMessage}};
    }, [this](const QVariant &res) {
        m_busy->stop();
        m_running = false;
        const QVariantMap m = res.toMap();
        if (m.value(QStringLiteral("ok")).toBool()) {
            showResult(Semantic::Success, tr("缓存清理完成"), true);
        } else {
            showResult(Semantic::Danger,
                       tr("清理失败: %1").arg(m.value(QStringLiteral("msg")).toString()),
                       false);
        }
    });
}
