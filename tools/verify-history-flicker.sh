#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# 闪屏根因回归探针构建 + 运行脚本（2026-10-04）
#
# 缺陷背景：磁盘监控服务未开启时，disk-history 目录无任何日记录，
#   loadIndex() → rebuildIndex() 仍回写一份**空** index.json，mtime 变化触发
#   QFileSystemWatcher::directoryChanged → 防抖刷新 → requestRange →
#   onRangeLoaded → rebuildSpikeSet → loadIndex → …… 死循环，
#   每轮 setLoading + 重启图表动画 = 「数据区域一直闪屏」。
#
# 本脚本做两件事：
#   1. 编译探针（需要 moc，DiskHistoryStore 是 Q_OBJECT）
#   2. 在**隔离的 XDG_DATA_HOME** 里运行，避免污染真实 ~/.local/share
#
# 用法：bash tools/verify-history-flicker.sh
# 退出码：0 = 修复在位；1 = 缺陷复现

set -uo pipefail
cd "$(dirname "$0")/.."

# shellcheck source=/dev/null
source .toolchain/env.sh >/dev/null 2>&1 || {
    echo "FAIL: 无法加载 .toolchain/env.sh（请先构建过项目，确认工具链就位）"
    exit 1
}

SR="$PWD/.toolchain/sysroot/usr"
MOC="$SR/lib/qt6/libexec/moc"
OUT_DIR="$PWD/.shim"
PROBE="$OUT_DIR/probe_history_flicker"
mkdir -p "$OUT_DIR"

# XDG_DATA_HOME 隔离：historyDir() 走 GenericDataLocation + "/panshi-manager/disk-history"
export XDG_DATA_HOME="$OUT_DIR/xdg-flicker-probe"
rm -rf "$XDG_DATA_HOME"
export QT_QPA_PLATFORM=offscreen

QT_INC=(
    -I"$SR/include" -I"$SR/include/dtk6"
    -I"$SR/include/dtk6/DCore" -I"$SR/include/dtk6/DWidget"
    -I"$SR/include/x86_64-linux-gnu/qt6"
    -I"$SR/include/x86_64-linux-gnu/qt6/QtCore"
    -I"$SR/include/x86_64-linux-gnu/qt6/QtConcurrent"
)

echo "== 1/3 moc =="
"$MOC" -Isrc/disk "${QT_INC[@]}" src/disk/history.h -o "$OUT_DIR/moc_history.cpp" || exit 1

echo "== 2/3 编译探针 =="
g++ -std=c++17 -fPIC -o "$PROBE" \
    tools/probe_history_flicker.cpp "$OUT_DIR/moc_history.cpp" src/disk/history.cpp \
    -Isrc/disk -I"$OUT_DIR" "${QT_INC[@]}" \
    -L"$SR/lib/x86_64-linux-gnu" -lQt6Core -lQt6Concurrent || exit 1

echo "== 3/3 运行 =="
"$PROBE"
rc=$?

rm -rf "$XDG_DATA_HOME"
if [ $rc -eq 0 ]; then
    echo "RESULT: PASS —— 空目录不回写 index.json，闪屏死循环已断开"
else
    echo "RESULT: FAIL —— 缺陷复现，空目录仍在回写 index.json"
fi
exit $rc
