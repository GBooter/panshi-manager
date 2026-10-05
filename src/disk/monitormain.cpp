// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitormain.h"
#include "monitor.h"

#include <QCoreApplication>
#include <QTimer>

#include <cstdio>

namespace Panshi {

int runMonitor(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("panshi-manager"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    DiskMonitor monitor;
    QObject::connect(&monitor, &DiskMonitor::finished, &app,
                     [&app](bool ok, const QString &msg) {
                         // 后台服务原先没有任何输出：跑成功/跑失败在 journalctl 里
                         // 都是一片空白，「服务明明 active 却没数据」无法定位。
                         // 这里把结果写进 stderr，journalctl --user -u panshi-monitor.service 可见。
                         if (msg.isEmpty()) {
                             std::fprintf(stderr, "[panshi-monitor] %s\n", ok ? "ok" : "failed");
                         } else {
                             std::fprintf(stderr, "[panshi-monitor] %s: %s\n",
                                          ok ? "ok" : "failed", qPrintable(msg));
                         }
                         std::fflush(stderr);
                         app.quit();
                     });
    QTimer::singleShot(0, &monitor, [&monitor] { monitor.runCycle(); });
    return app.exec();
}

} // namespace Panshi
