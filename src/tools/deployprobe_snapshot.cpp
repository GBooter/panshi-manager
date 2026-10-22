// SPDX-License-Identifier: GPL-3.0-or-later
//
// 部署页「待生效」等行的视觉/层级快照探针 panshi-deployshot（开发期专用，不安装）
//
// 目的：把「删除部署」那张表的**视觉层级与禁用态**从肉眼观察变成可断言的值，并出图
//       供人工复核（排版 / 层级 / 「待生效」徽章位置 / 删除按钮置灰）。
//
// 背景（2026-10-05）：修复「隐藏主线不可删」时，曾出现版本列「待生效」徽章没真正落地
//       （只写了 tooltip 和报错文案，代码里却没有徽章），状态列还错显「历史」——
//       不悬停根本看不出为什么删不掉。这类「层级 / 禁用态」缺陷肉眼才敏感，但也能
//       用断言守住。
//
// 与 panshi-deployprobe 的分工：
//   · panshi-deployprobe  守**逻辑契约**（Hidden⇒不可删、删除传 commitId、回滚备份可删）；
//   · 本探针守**UI 把契约画对了**——按钮真的置灰、状态列真的显「待生效」而非「历史」、
//     「待生效」徽章真的绘制出来。两者互补，任一回退都会被抓。
//
// ⚠️ offscreen 限制（与 panshi-navshot 同款声明）：本容器无平台主题插件（libqdeepin.so），
//   DTK 控件拿不到主题样式，故**截图只能复核排版 / 层级 / 禁用态 / 文案**，配色以
//   panshi-deployprobe + panshi-colorprobe 为准；截图在真实会话下才是配色证据。
//
// 复刻来源：src/pages/deployspage.cpp 的 onLoaded() 单行渲染。若 onLoaded 改了徽章 /
//   按钮 / 状态文案的视觉，需同步本文件（本探针刻意不复用 onLoaded，以免 link 整个主窗口）。
//
// 用法：
//   panshi-deployshot                         # 亮主题 + 出图到 .tmp/deployshot.png
//   panshi-deployshot --png /path/out.png     # 指定出图路径
//   panshi-deployshot --no-png                # 只跑断言不出图
//
// 输出：stdout 纯 JSON；ok/FAIL 走 stderr；退出码 = 失败断言数 > 0 ? 1 : 0

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVector>
#include <QWidget>

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DLabel>
#include <DPushButton>

#include "backend.h"
#include "ui/components.h"
#include "pages/common.h"

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

namespace {

int g_fail = 0;
QJsonArray g_checks;

void check(const QString &name, bool ok, const QString &detail = QString())
{
    if (!ok) ++g_fail;
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("ok"), ok);
    if (!detail.isEmpty()) o.insert(QStringLiteral("detail"), detail);
    g_checks.append(o);
    QTextStream(stderr) << (ok ? "[ok]   " : "[FAIL] ") << name
                       << (detail.isEmpty() ? QString() : QStringLiteral("  -- ") + detail)
                       << Qt::endl;
}

// 与 deployspage.cpp 中 onLoaded 的状态文案保持一致（中文开发环境，直接用字面量对照）
QString statusTextOf(const Panshi::DeployInfo &d)
{
    if (d.isBooted)          return QStringLiteral("当前启动");
    if (d.hidden)            return QStringLiteral("待生效");
    if (d.isRollbackBackup)  return QStringLiteral("回滚备份");
    return QStringLiteral("历史");
}

struct RowRef {
    Panshi::DeployInfo d;
    DPushButton *delBtn = nullptr;
    QTableWidgetItem *statusItem = nullptr;
};

// 复刻 deployspage.cpp onLoaded() 的单行渲染（视觉一致；不 link 主窗口）
void buildRow(QTableWidget *table, int row, const Panshi::DeployInfo &d, RowRef &ref)
{
    ref.d = d;

    auto *idx = new QTableWidgetItem(QString::number(row));
    idx->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 0, idx);

    // 版本 + 徽章
    auto *nameW = new QWidget;
    auto *nl = new QHBoxLayout(nameW);
    nl->setContentsMargins(6, 2, 6, 2);
    nl->setSpacing(PanshiUi::Space::S);
    const QString verText = d.systemVersion.isEmpty() ? QStringLiteral("-") : d.systemVersion;
    auto *nm = new Ui::ElidedLabel(verText, nameW);
    if (d.isBooted) {
        QFont f = nm->font();
        f.setBold(true);
        nm->setFont(f);
    }
    nm->setToolTip(verText);
    nl->addWidget(nm, 1);
    if (d.isBooted) {
        auto *tag = new PanshiUi::PanshiBadge(QStringLiteral("当前"), PanshiUi::Semantic::Accent, nameW);
        tag->setToolTip(QStringLiteral("当前正在运行的系统版本"));
        nl->addWidget(tag, 0, Qt::AlignVCenter);
    } else if (d.hidden) {
        // 系统更新后待生效的新系统主线：受系统保护，删除按钮置灰（见下方 bDel）
        auto *tag = new PanshiUi::PanshiBadge(QStringLiteral("待生效"), PanshiUi::Semantic::Warning, nameW);
        tag->setToolTip(QStringLiteral("这是系统更新后待生效的新系统，重启电脑后才会启用，目前受系统保护，暂时无法删除。"));
        nl->addWidget(tag, 0, Qt::AlignVCenter);
    } else if (d.isRollbackBackup) {
        auto *tag = new PanshiUi::PanshiBadge(QStringLiteral("回滚前"), PanshiUi::Semantic::Warning, nameW);
        tag->setToolTip(QStringLiteral("回滚前自动创建的安全版本, 确认系统正常后可删除"));
        nl->addWidget(tag, 0, Qt::AlignVCenter);
    }
    table->setCellWidget(row, 1, nameW);

    auto *sit = new QTableWidgetItem(statusTextOf(d));
    sit->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 2, sit);
    ref.statusItem = sit;

    auto *time = new QTableWidgetItem(Panshi::formatTime(d.time));
    table->setItem(row, 3, time);

    auto *cid = new QTableWidgetItem(d.commitId.left(16) + QStringLiteral("..."));
    cid->setToolTip(d.commitId);
    cid->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 4, cid);

    auto *opw = new QWidget;
    auto *ol = new QHBoxLayout(opw);
    ol->setContentsMargins(4, 2, 4, 2);
    ol->setSpacing(6);
    auto *bDel = Ui::button(QStringLiteral("删除部署"), false, true);
    // 主线部署（hidden）受系统保护不可删：含当前运行项与待生效的替换主线
    bDel->setEnabled(Panshi::isDeployDeletable(d));
    if (d.isBooted)
        bDel->setToolTip(QStringLiteral("不能删除当前正在使用的系统版本。"));
    else if (d.hidden)
        bDel->setToolTip(QStringLiteral("这是系统更新后待生效的新版本，重启电脑后才会启用，目前受系统保护，暂时无法删除。"));
    else
        bDel->setToolTip(QStringLiteral("删除该旧版本以释放磁盘空间。"));
    ol->addWidget(bDel);
    ol->addStretch();
    table->setCellWidget(row, 5, opw);
    ref.delBtn = bDel;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");   // 必须在 DApplication 构造前
    DApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("panshi-deployshot"));

    QCommandLineParser parser;
    QCommandLineOption pngOpt(QStringList() << QStringLiteral("png"),
                              QStringLiteral("输出 PNG 路径"), QStringLiteral("path"));
    QCommandLineOption noPng(QStringList() << QStringLiteral("no-png"),
                             QStringLiteral("只跑断言不出图"));
    parser.addOption(pngOpt);
    parser.addOption(noPng);
    parser.process(app);

    const QString pngPath = parser.isSet(noPng) ? QString()
                                                : (parser.isSet(pngOpt) ? parser.value(pngOpt)
                                                        : QStringLiteral(".tmp/deployshot.png"));

    // 4 类典型行：当前启动 / 待生效 / 回滚备份 / 普通历史
    Panshi::DeployInfo booted;
    booted.systemVersion = QStringLiteral("25.2.2");
    booted.commitId = QStringLiteral("78c9ce08a1b2c3d4e5f60718293a4b5c6d7e8f90");
    booted.time = 1760000000;
    booted.isBooted = true;
    booted.hidden = true;

    Panshi::DeployInfo pending;
    pending.systemVersion = QString();   // 待生效数据部署常为空（UI 显 "-"）
    pending.commitId = QStringLiteral("399d240fb1c2d3e4f5a60718293b4c5d6e7f8091");
    pending.time = 1760100000;
    pending.hidden = true;

    Panshi::DeployInfo rollback;
    rollback.systemVersion = QStringLiteral("25.2.0");
    rollback.commitId = QStringLiteral("aabbccddeeff00112233445566778899aabbccdd");
    rollback.time = 1759900000;
    rollback.isRollbackBackup = true;

    Panshi::DeployInfo history;
    history.systemVersion = QStringLiteral("25.1.0");
    history.commitId = QStringLiteral("11223344556677889900aabbccddeeff00112233");
    history.time = 1759800000;

    QVector<Panshi::DeployInfo> rows{booted, pending, rollback, history};

    auto *table = new QTableWidget(rows.size(), 6);
    table->setHorizontalHeaderLabels({QStringLiteral("索引"), QStringLiteral("版本"),
                                      QStringLiteral("状态"), QStringLiteral("部署时间"),
                                      QStringLiteral("提交ID"), QStringLiteral("操作")});
    table->setColumnWidth(0, 72);
    table->setColumnWidth(1, 160);
    table->setColumnWidth(2, 100);
    table->setColumnWidth(3, 145);
    table->setColumnWidth(4, 150);
    table->setColumnWidth(5, 120);

    QVector<RowRef> refs(rows.size());
    for (int i = 0; i < rows.size(); ++i)
        buildRow(table, i, rows.at(i), refs[i]);

    table->resize(820, 200);
    table->show();
    // 离屏「首渲染遍」假象：先跑事件 + 丢弃式 grab，再取正式画面
    for (int i = 0; i < 6; ++i) QApplication::processEvents();
    (void)table->grab();
    QApplication::processEvents();

    // ---- 断言 ----
    // 1. 删除按钮可用态与 isDeployDeletable 完全一致（覆盖「隐藏主线不可删 / 回滚可删」）
    for (int i = 0; i < refs.size(); ++i) {
        const bool exp = Panshi::isDeployDeletable(refs.at(i).d);
        const bool got = refs.at(i).delBtn->isEnabled();
        check(QStringLiteral("行%1 删除按钮可用态 == isDeployDeletable").arg(i),
              exp == got,
              QStringLiteral("expect=%1 got=%2").arg(exp).arg(got));
    }

    // 2. 状态列文案正确（防「待生效」被回退成「历史」）
    const QStringList expStatus{QStringLiteral("当前启动"), QStringLiteral("待生效"),
                                QStringLiteral("回滚备份"), QStringLiteral("历史")};
    for (int i = 0; i < refs.size(); ++i)
        check(QStringLiteral("行%1 状态列文案").arg(i),
              refs.at(i).statusItem->text() == expStatus.at(i),
              QStringLiteral("got=%1 expect=%2").arg(refs.at(i).statusItem->text(), expStatus.at(i)));

    // 3. 徽章真的绘制出来（PanshiBadge 内部用 DLabel 承载文案）
    auto hasLabel = [&](const QString &txt) -> bool {
        for (DLabel *l : table->findChildren<DLabel *>())
            if (l->text() == txt) return true;
        return false;
    };
    check(QStringLiteral("行0 「当前」徽章已绘制"), hasLabel(QStringLiteral("当前")));
    check(QStringLiteral("行1 「待生效」徽章已绘制"), hasLabel(QStringLiteral("待生效")));
    check(QStringLiteral("行2 「回滚前」徽章已绘制"), hasLabel(QStringLiteral("回滚前")));

    // ---- 出图（offscreen 下仅作排版/层级复核，配色以 deployprobe + colorprobe 为准）----
    if (!pngPath.isEmpty()) {
        const QImage img = table->grab().toImage().convertToFormat(QImage::Format_RGB32);
        QDir().mkpath(QFileInfo(pngPath).absolutePath());
        const bool saved = img.save(pngPath);
        check(QStringLiteral("保存快照 PNG"), saved, pngPath);
        if (saved) QTextStream(stderr) << "PNG -> " << pngPath << Qt::endl;
    }

    QJsonObject root;
    root.insert(QStringLiteral("tool"), QStringLiteral("panshi-deployshot"));
    root.insert(QStringLiteral("ok"), g_fail == 0);
    root.insert(QStringLiteral("failCount"), g_fail);
    root.insert(QStringLiteral("checks"), g_checks);
    QTextStream(stdout) << QJsonDocument(root).toJson(QJsonDocument::Indented);

    return g_fail > 0 ? 1 : 0;
}
