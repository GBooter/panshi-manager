// SPDX-License-Identifier: GPL-3.0-or-later
// 后端取值探针 panshi-backendprobe（开发期专用，不安装）
//
// 用途：验证总览页各字段的**取值来源**是否正确 —— 测的是 backend.cpp 里
//       的真实函数，不是探针里复刻的解析逻辑。
//
// 背景（2026-10-05 报障「总览页磐石部署后面没有信息」）：
//   getDeployVersion() 取了 deploy_list 的 **first()**，而实测
//   deepin-immutable-ctl admin status -j 的输出里，第一项是**待生效的新部署**
//   （IsBooted=false，SystemVersion 为空），当前运行的部署要用 IsBooted 判定。
//   同文件的 getBootedDeployId() 本来就是这么写的 —— 属漏改的复制粘贴。
//
// 断言：
//   1. 结果必须非空（否则页面退化成 "-"）；
//   2. 必须等于「IsBooted=true 那一项的 SystemVersion」；
//   3. 必须不等于 deploy_list[0].SystemVersion  —— 除非两者恰好相同。
//   第 3 条是**反向对照**：修复前它取的就是 [0]，若仍等于 [0] 说明没修好。
//
// 用法：./panshi-backendprobe    （退出码 0 = 全部通过）

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include "backend.h"

using namespace Panshi;

static QJsonArray deployList()
{
    QProcess p;
    p.start(QStringLiteral("/usr/sbin/deepin-immutable-ctl"),
            {QStringLiteral("admin"), QStringLiteral("status"), QStringLiteral("-j")});
    if (!p.waitForFinished(30000))
        return QJsonArray();
    const QByteArray out = p.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (doc.isNull())
        return QJsonArray();
    return doc.object().value(QStringLiteral("data")).toObject()
               .value(QStringLiteral("deploy_list")).toArray();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QJsonArray dl = deployList();
    if (dl.isEmpty()) {
        out << "SKIP  无法获取 deploy_list（deepin-immutable-ctl 不可用或无权限）\n";
        return 0;
    }

    QString firstVer, bootedVer;
    int n = 0;
    for (const QJsonValue &v : dl) {
        const QJsonObject o = v.toObject();
        if (n == 0)
            firstVer = o.value(QStringLiteral("SystemVersion")).toString();
        if (o.value(QStringLiteral("IsBooted")).toBool())
            bootedVer = o.value(QStringLiteral("SystemVersion")).toString();
        out << "  [" << n << "] IsBooted=" << (o.value(QStringLiteral("IsBooted")).toBool() ? "true " : "false")
            << "  SystemVersion=\"" << o.value(QStringLiteral("SystemVersion")).toString() << "\"\n";
        ++n;
    }

    const QString got = getDeployVersion();
    out << "\n被测函数 getDeployVersion() = \"" << got << "\"\n";
    out << "期望（IsBooted=true 那一项）      = \"" << bootedVer << "\"\n";
    out << "反例（deploy_list[0]，修复前的值）= \"" << firstVer << "\"\n";

    bool ok = true;
    if (got.isEmpty()) {
        out << "FAIL  结果为空 —— 页面会显示 \"-\"\n";
        ok = false;
    } else if (!bootedVer.isEmpty() && got != bootedVer) {
        out << "FAIL  不等于当前启动部署的版本\n";
        ok = false;
    } else if (!firstVer.isEmpty() && got == firstVer && firstVer != bootedVer) {
        out << "FAIL  仍取到了 deploy_list[0]（旧行为，说明没修好）\n";
        ok = false;
    }

    out << (ok ? "PASS  取值来源正确\n" : "FAIL\n");
    return ok ? 0 : 1;
}
