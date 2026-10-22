// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Panshi {

/**
 * 后台监控入口（panshi-manager --monitor）：
 * 无 GUI，执行一轮监控周期后退出（由 systemd user timer 周期性拉起）。
 */
int runMonitor(int argc, char *argv[]);

} // namespace Panshi
