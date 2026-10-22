// SPDX-License-Identifier: GPL-3.0-or-later
//
// 「部署删除」规则探针 panshi-deployprobe（开发期专用，不安装）
//
// 目的：把本次修复的两条规则从「肉眼 + 代码复查」变成可断言的值，防回退。
//
// 背景（2026-10-05 报障「删除部署输密码后仍失败」）：
//   ① 可删性只看 IsBooted，导致「待生效的替换主线」被渲染成可删的历史，
//      点了必然被 ctl 拒绝（cannot undeploy the replacement mainline deployment）；
//   ② 删除按下标传参，而隐藏主线两条 Index 都是 0，按下标删会误伤。
// 修复后：
//   判据改为 isDeployDeletable() = !hidden && commitId 非空；
//   删除改传 commitId（undeployArgs 末参）。
//
// 断言：
//   R1a  隐藏主线（hidden）必须不可删；
//   R1b  可删项必须非 hidden（反向，防判据被改回 !isBooted）；
//   R1c  至少存在一条 hidden（当前启动部署必然 hidden，0 条说明 Hidden 字段解析坏掉）；
//   R1d  当前启动项必须 hidden；
//   R1e  回滚备份（isRollbackBackup）绝不能被 hidden 误伤 —— 直击「备份能不能删」；
//   R2   取一条可删项，undeployArgs(commitId) 末参必须为该 commitId（长哈希），
//        且不等于其行下标（证明传的是 commitId 不是 index）。
//
// ⚠️ 直接调用 backend 真实函数，不另抄解析逻辑；ctl 不可用时 SKIP（与 probe_backend 一致）。
// 用法：./panshi-deployprobe    （退出码 0 = 全部通过，1 = 有断言失败）
//
// 输出：stdout 为可读的部署清单 + 统计；ok/FAIL 行走 stderr。

#include <QCoreApplication>
#include <QTextStream>

#include "backend.h"

using namespace Panshi;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QList<DeployInfo> deploys = getDeploys();
    if (deploys.isEmpty()) {
        err << "SKIP  无法获取部署列表（deepin-immutable-ctl 不可用或无权限）\n";
        return 0;   // 与 probe_backend 一致：环境缺失则跳过，不判失败
    }

    int fail = 0;
    auto check = [&](bool ok, const QString &what) {
        err << (ok ? "  ok   " : "  FAIL ") << what << "\n";
        if (!ok) ++fail;
    };

    int nHidden = 0, nDeletable = 0, nRollbackHidden = 0, nBootedHidden = 0;
    QString sampleDeletableCommit;
    int sampleDeletableRow = -1;

    for (int i = 0; i < deploys.size(); ++i) {
        const DeployInfo &d = deploys.at(i);
        const bool deletable = isDeployDeletable(d);
        out << "  [" << i << "] ver=\"" << d.systemVersion << "\""
            << " hidden=" << (d.hidden ? "true " : "false")
            << " booted=" << (d.isBooted ? "true " : "false")
            << " rollback=" << (d.isRollbackBackup ? "true " : "false")
            << " deletable=" << (deletable ? "true " : "false")
            << " commit=" << d.commitId.left(12) << "\n";

        check(!d.hidden || !deletable,
              QStringLiteral("[%1] 隐藏主线(hidden)不可删").arg(i));
        check(deletable || d.hidden,
              QStringLiteral("[%1] 可删项必须非 hidden").arg(i));
        if (d.isRollbackBackup && d.hidden)
            ++nRollbackHidden;
        if (d.hidden)
            ++nHidden;
        if (d.isBooted && d.hidden)
            ++nBootedHidden;
        if (deletable) {
            ++nDeletable;
            if (sampleDeletableCommit.isEmpty()) {
                sampleDeletableCommit = d.commitId;
                sampleDeletableRow = i;
            }
        }
    }

    check(nHidden > 0,
          QStringLiteral("至少存在一条 hidden 主线（当前启动必然 hidden），实际 %1 条").arg(nHidden));
    check(nBootedHidden > 0,
          QStringLiteral("当前启动项必须 hidden，实际 %1 条").arg(nBootedHidden));
    check(nRollbackHidden == 0,
          QStringLiteral("回滚备份未被 hidden 误伤（备份可删），实际 %1 条被误伤").arg(nRollbackHidden));

    if (!sampleDeletableCommit.isEmpty()) {
        const QStringList args = undeployArgs(sampleDeletableCommit);
        check(args.size() == 3
                  && args.at(0) == QStringLiteral("admin")
                  && args.at(1) == QStringLiteral("undeploy")
                  && args.at(2) == sampleDeletableCommit,
              QStringLiteral("undeployArgs 末参=%1（commitId），而非行下标").arg(sampleDeletableCommit.left(12)));
        check(args.at(2) != QString::number(sampleDeletableRow),
              QStringLiteral("删除未使用行下标(%1)，确实传了 commitId").arg(sampleDeletableRow));
        check(sampleDeletableCommit.length() >= 8,
              QStringLiteral("commitId 为长哈希(非短下标)：%1").arg(sampleDeletableCommit.left(12)));
    } else {
        check(false, QStringLiteral("没有可删项可用于验证删除命令参数"));
    }

    out << "\n统计：hidden=" << nHidden << " deletable=" << nDeletable
        << " rollback被误伤=" << nRollbackHidden << "\n";
    err << (fail ? QStringLiteral("\n探针失败 %1 项\n").arg(fail)
                 : QStringLiteral("\n全部断言通过\n"));
    return fail ? 1 : 0;
}
