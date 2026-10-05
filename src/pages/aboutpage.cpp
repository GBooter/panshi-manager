// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutpage.h"
#include "common.h"
#include "ui/components.h"

#include <DLabel>
#include <DFontSizeManager>
#include <DIconTheme>
#include <DPushButton>

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QTimer>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE
using namespace PanshiUi;

// makeCopyButton / makeCommandRow 已上移到 common.h：
// 自绘的「关于」对话框（ui/aboutdialog.cpp）也要用同一份实现，
// 两处各写一份必然分叉（复制行为与命令文案会走偏）。


AboutPage::AboutPage(QWidget *parent)
    : BasePage(parent)
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

    lay->addWidget(Ui::pageHeader(tr("关于"), QString(), QStringLiteral("help-about")));

    // ---- ① 应用信息卡（居中，方案 §3.3 P6）----
    auto *info = new PanshiPanel(this);
    auto *il = info->body();
    il->setSpacing(Space::S);

    auto *icon = new DLabel(info);
    icon->setFixedSize(64, 64);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(DIconTheme::findQIcon(QStringLiteral("panshi")).pixmap(64, 64));
    il->addWidget(icon, 0, Qt::AlignHCenter);

    auto *name = new DLabel(tr("磐石设置"), info);
    QFont nf = name->font();
    nf.setBold(true);
    name->setFont(nf);
    DFontSizeManager::instance()->bind(name, DFontSizeManager::T3);
    il->addWidget(name, 0, Qt::AlignHCenter);

    const QString versionLine = tr("版本 1.0.0  ·  Deepin 25 磐石系统管理工具");
    auto *ver = makeLabel(versionLine, TextLevel::Desc, Semantic::Muted);
    ver->setWordWrap(true);
    il->addWidget(ver, 0, Qt::AlignHCenter);

    auto *d = makeLabel(tr("基于 deepin-immutable-ctl / deepin-immutable-writable / OSTree 开发\n"
                            "支持 Deepin 25 (X11 / Wayland), 采用 DDE 桌面风格 (DTK)"),
                        TextLevel::Desc, Semantic::Muted);
    d->setWordWrap(true);
    il->addWidget(d, 0, Qt::AlignHCenter);

    // 作者：与自绘「关于」对话框（ui/aboutdialog.cpp）保持同一处事实来源。
    // 两处各写一份必然分叉 —— 页内写「Feelup」而对话框漏掉，用户会以为
    // 应用没有作者信息（许可证与致谢本来就该显眼）。
    auto *author = makeLabel(tr("作者：Feelup"), TextLevel::Desc, Semantic::Normal);
    il->addWidget(author, 0, Qt::AlignHCenter);

    il->addSpacing(Space::S);
    auto *bCopyVer = new DSuggestButton(tr("复制版本信息"), info);
    bCopyVer->setCursor(Qt::PointingHandCursor);
    connect(bCopyVer, &DPushButton::clicked, bCopyVer, [bCopyVer, versionLine] {
        QApplication::clipboard()->setText(
            QObject::tr("磐石设置  ") + versionLine);
        bCopyVer->setText(QObject::tr("已复制"));
        QTimer::singleShot(2000, bCopyVer, [bCopyVer] {
            bCopyVer->setText(QObject::tr("复制版本信息"));
        });
    });
    il->addWidget(bCopyVer, 0, Qt::AlignHCenter);

    lay->addWidget(info);

    // ---- ② 技术参考（默认折叠，方案 §3.3 P6 / 修 I17）----
    auto *refBox = new PanshiCollapsible(tr("磐石系统技术参考"), this);
    auto *ref = makeLabel(
        tr("· 核心目录只读保护: /usr 只读 overlay, /etc /opt 可写 overlay\n"
           "· 原子更新: 升级失败自动回滚 (auto-rollback-on-failure)\n"
           "· 快照备份: 基于 OSTree 秒级增量快照, 支持导出/导入\n"
           "· 初始备份受系统保护, 不可删除/修改\n"
           "· 快照还原需重启生效, 可写层开启时禁止还原\n"
           "· 系统与数据分区分层: /home 等数据分区不受还原影响"),
        TextLevel::Desc, Semantic::Normal);
    ref->setWordWrap(true);
    refBox->setContent(ref);
    lay->addWidget(refBox);

    // ---- ③ 常用命令（默认折叠，每行带复制按钮）----
    auto *cmdBox = new PanshiCollapsible(tr("常用命令参考"), this);
    auto *cmdHost = new QWidget(cmdBox);
    auto *cmdLay = new QVBoxLayout(cmdHost);
    cmdLay->setContentsMargins(0, 0, 0, 0);
    cmdLay->setSpacing(Space::S);

    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-ctl snapshot list"),
        tr("列出快照"), cmdHost));
    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-ctl snapshot create <名称> <描述>"),
        tr("创建快照"), cmdHost));
    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-ctl snapshot rollback <id>"),
        tr("还原快照"), cmdHost));
    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-writable status"),
        tr("保护状态"), cmdHost));
    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-writable enable|disable"),
        tr("开/关可写层"), cmdHost));
    cmdLay->addWidget(Ui::makeCommandRow(
        QStringLiteral("sudo deepin-immutable-ctl admin status"),
        tr("部署状态"), cmdHost));

    cmdBox->setContent(cmdHost);
    lay->addWidget(cmdBox);

    lay->addStretch();
}
