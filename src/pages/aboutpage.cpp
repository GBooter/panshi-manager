// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutpage.h"
#include "common.h"
#include "ui/components.h"

#include <DLabel>
#include <DFontSizeManager>
#include <DIconTheme>
#include <DPushButton>
#include <DSuggestButton>

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFont>
#include <QIcon>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

DWIDGET_USE_NAMESPACE
using namespace PanshiUi;

// makeCopyButton / makeCommandRow 已上移到 common.h：
// 自绘的「关于」对话框（ui/aboutdialog.cpp）也要用同一份实现，
// 两处各写一份必然分叉（复制行为与命令文案会走偏）。

namespace {

/** 信息行：左侧小字标签 + 右侧正文（值允许折行，窄窗口优雅退化） */
QWidget *makeInfoRow(const QString &label, const QString &value, QWidget *parent = nullptr)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(Space::L);
    auto *l = makeLabel(label, TextLevel::Aux, Semantic::Muted);
    h->addWidget(l, 0, Qt::AlignTop);
    h->addStretch();
    auto *v = makeLabel(value, TextLevel::Body, Semantic::Normal);
    v->setWordWrap(true);
    h->addWidget(v, 1);
    return w;
}

} // namespace

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
    lay->setContentsMargins(Space::XL, 20, Space::XL, 20);
    lay->setSpacing(Space::L);

    lay->addWidget(Ui::pageHeader(tr("关于"), tr("关于磐石设置"), QStringLiteral("info-badge")));

    // ---- ① 应用信息卡（2026-10-06 重设计：图标 + 信息行网格，替代堆叠文本）----
    auto *info = new PanshiPanel(this);
    auto *il = info->body();
    il->setSpacing(Space::M);

    // 头部：大图标 + 应用名 + 版本
    auto *hero = new QHBoxLayout;
    hero->setSpacing(Space::L);
    auto *icon = new DLabel(info);
    icon->setFixedSize(72, 72);
    icon->setAlignment(Qt::AlignCenter);
    // 新 Logo（2026-10-09）：内嵌资源优先（系统目录可能残留旧版图标）。
    QIcon appIcon(QStringLiteral(":/icons/panshi.svg"));
    if (appIcon.isNull())
        appIcon = DIconTheme::findQIcon(QStringLiteral("panshi"));
    icon->setPixmap(appIcon.pixmap(72, 72));
    hero->addWidget(icon, 0, Qt::AlignVCenter);

    auto *tl = new QVBoxLayout;
    tl->setSpacing(Space::XS);
    auto *name = new DLabel(tr("磐石设置"), info);
    QFont nf = name->font();
    nf.setBold(true);
    name->setFont(nf);
    DFontSizeManager::instance()->bind(name, DFontSizeManager::T2);
    tl->addWidget(name);
    // 版本号**运行时**取自 applicationVersion()（CMake 的 PANSHI_VERSION 注入），
    // 源串里只留 %1 占位符。此前把「版本 1.0.0」整串写进 tr()，发版时改代码
    // 与改翻译两件事必然有一件被漏掉 —— 包已到 1.0.5，界面仍显示 1.0.0。
    const QString versionLine =
        tr("版本 %1  ·  Deepin 25 磐石系统管理工具").arg(QCoreApplication::applicationVersion());
    auto *ver = makeLabel(versionLine, TextLevel::Desc, Semantic::Muted);
    ver->setWordWrap(true);
    tl->addWidget(ver);
    hero->addLayout(tl, 1);
    il->addLayout(hero);

    il->addWidget(new PanshiSeparator(info));

    // 信息行：技术基础 / 支持平台 / 作者（单列全宽，值有足够空间不别扭换行）
    il->addWidget(makeInfoRow(tr("技术基础"),
                              tr("deepin-immutable-ctl / deepin-immutable-writable / OSTree"),
                              info));
    il->addWidget(makeInfoRow(tr("支持平台"),
                              tr("Deepin 25（X11 / Wayland），DDE 桌面风格（DTK）"), info));
    il->addWidget(makeInfoRow(tr("作者"), tr("Feelup"), info));

    // 复制版本信息（与自绘「关于」对话框共用同一份版本串来源）
    auto *bCopyVer = new DSuggestButton(tr("复制版本信息"), info);
    bCopyVer->setCursor(Qt::PointingHandCursor);
    connect(bCopyVer, &DPushButton::clicked, bCopyVer, [bCopyVer, versionLine] {
        QApplication::clipboard()->setText(QObject::tr("磐石设置  ") + versionLine);
        bCopyVer->setText(QObject::tr("已复制"));
        QTimer::singleShot(2000, bCopyVer, [bCopyVer] {
            bCopyVer->setText(QObject::tr("复制版本信息"));
        });
    });
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(bCopyVer);
    btnRow->addStretch();
    il->addLayout(btnRow);

    lay->addWidget(info);

    // ---- ② 技术参考（参考卡，默认折叠）----
    //
    // 2026-10-27 重设计：原先这里是两条居中的「▸ 标题」细字，悬在页面下半幅
    // 留白中 —— 既没有容器暗示"这是可展开的分组"，也没有层级（用户反馈
    // 「这部分现在的方式不好看」）。现改为一张独立的参考卡：
    //   图块 + 标题 + 副标题 + 分隔线 + 两列机制网格。
    //
    // 默认折叠（用户 2026-10-27 复评后的定稿）：这两块是**查阅型**内容，
    // 一年看一次；「关于」页的主体是上面的应用信息。默认展开会让页面变成
    // 一屏半的清单，主体反被稀释。折叠态并非空壳：图块 + 标题 + 副标题
    // （"不可变系统的六条核心机制"）已经说清这条折叠项里是什么，caret
    // 指示可展开，标题行整行可点。
    auto *refCard = new PanshiRefCard(QStringLiteral("shield-check"),
                                      tr("磐石系统技术参考"),
                                      tr("不可变系统的六条核心机制"), this);
    // objectName 给探针（panshi-aboutpageshot）作稳定钩子：组件类不带 Q_OBJECT，
    // findChildren<具体类型>() 用不了，只能按名字找。
    refCard->setObjectName(QStringLiteral("panshiAboutTechCard"));
    const QVector<Ui::ReferenceItem> feats = {
        {tr("核心目录只读保护"), tr("/usr 只读 overlay，/etc /opt 可写 overlay")},
        {tr("原子更新"), tr("升级失败自动回滚（auto-rollback-on-failure）")},
        {tr("快照备份"), tr("基于 OSTree 秒级增量快照，支持导出/导入")},
        {tr("初始备份保护"), tr("初始备份受系统保护，不可删除/修改")},
        {tr("还原需重启"), tr("快照还原需重启生效，可写层开启时禁止还原")},
        {tr("分层存储"), tr("系统与数据分区分层，/home 等数据分区不受还原影响")},
    };
    refCard->content()->addWidget(Ui::referenceGrid(feats, 2, refCard));
    refCard->setExpanded(false);   // 默认折叠：查阅型内容，不该占主体视野
    lay->addWidget(refCard);

    // ---- ③ 常用命令（参考卡，命令条 + 行尾复制 + 组级「复制全部」）----
    auto *cmdCard = new PanshiRefCard(QStringLiteral("utilities-terminal"),
                                      tr("常用命令参考"),
                                      tr("点击命令右侧图标复制，可直接粘贴到终端执行"), this);
    cmdCard->setObjectName(QStringLiteral("panshiAboutCommandCard"));

    const QVector<Ui::CommandRef> cmds = {
        {QStringLiteral("sudo deepin-immutable-ctl snapshot list"), tr("列出快照")},
        {QStringLiteral("sudo deepin-immutable-ctl snapshot create <名称> <描述>"), tr("创建快照")},
        {QStringLiteral("sudo deepin-immutable-ctl snapshot rollback <id>"), tr("还原快照")},
        {QStringLiteral("sudo deepin-immutable-writable status"), tr("保护状态")},
        {QStringLiteral("sudo deepin-immutable-writable enable|disable"), tr("开/关可写层")},
        {QStringLiteral("sudo deepin-immutable-ctl admin status"), tr("部署状态")},
    };
    cmdCard->content()->addWidget(Ui::commandList(cmds, cmdCard));

    // 组级操作：一次复制全部命令（逐条复制六次是多余劳动）
    auto *bCopyAll = new DPushButton(tr("复制全部"), cmdCard);
    bCopyAll->setCursor(Qt::PointingHandCursor);
    QObject::connect(bCopyAll, &DPushButton::clicked, bCopyAll, [bCopyAll, cmds] {
        QStringList lines;
        lines.reserve(cmds.size());
        for (const Ui::CommandRef &c : cmds)
            lines << c.cmd;
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        bCopyAll->setText(QObject::tr("已复制"));
        QTimer::singleShot(2000, bCopyAll, [bCopyAll] {
            bCopyAll->setText(QObject::tr("复制全部"));
        });
    });
    cmdCard->addHeaderWidget(bCopyAll);
    // 折叠态下「复制全部」仍留在标题行（组级操作挂在 header 上，不属于内容区）——
    // 这是刻意的：折叠只收起条款清单，不收起"一键拿走全部命令"这个入口。
    cmdCard->setExpanded(false);
    lay->addWidget(cmdCard);

    lay->addStretch();
}
