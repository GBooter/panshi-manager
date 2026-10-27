#!/usr/bin/env bash
# 采集 README「界面一览」截图（七页：总览/磁盘/保护/备份/更新/策略/关于）
#
# 用法：  bash tools/capture-docs-shots.sh
# 产出：  docs/screenshots/0N-*.png（1200x800，供 README 直接引用）
#
# 为什么不用离屏探针出图：README 展示的是"真机上的样子"，
# 离屏渲染下 DTK 主题、字体、窗口边框都与真机不同（见开发指南"离屏假象"一节）。
#
# 两个必须保留的动作（都不是可省略的细节）：
#   1. 截图前把指针移到窗口外 —— 否则侧栏会留下 hover 底色，与当前项的选中框
#      同时高亮，图上看起来像"选中了两个"。
#   2. 每页截图后与上一页比差异（compare -metric AE），过小就重试 ——
#      "导航没生效但仍截了图"是这里唯一的假通过来源。
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
RAW="$REPO/.tmp/shots-raw"
OUT="$REPO/docs/screenshots"
mkdir -p "$RAW" "$OUT"
export DISPLAY="${DISPLAY:-:0}"

SHOT_HOME="/tmp/shot-home-docs"
rm -rf "$SHOT_HOME"
mkdir -p "$SHOT_HOME/.local/share/panshi-manager/translations" "$SHOT_HOME/.config/deepin"
printf '[General]\nlanguage=zh_CN\n' > "$SHOT_HOME/.config/deepin/panshi-manager.conf"
cp -f "$REPO/build/panshi-manager_zh_CN.qm" "$SHOT_HOME/.local/share/panshi-manager/translations/" 2>/dev/null
export HOME="$SHOT_HOME" XDG_RUNTIME_DIR=/tmp/xdg-shot-docs
mkdir -p "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"

# 磁盘页需要演示数据（写入临时 HOME，必须在 export HOME 之后）
cd "$REPO" && python3 make-testdata.py --go spike >/tmp/mkdata-docs.log 2>&1 \
  && echo "testdata ok" || echo "testdata FAIL（磁盘页可能为空）"

cd "$REPO/build"
env -u LC_ALL LANG=zh_CN.UTF-8 HOME="$SHOT_HOME" XDG_RUNTIME_DIR=/tmp/xdg-shot-docs \
  QT_QPA_PLATFORMTHEME=deepin ./panshi-manager >/tmp/shot-docs.log 2>&1 &
APP=$!
sleep 10

# 用 --pid + _NET_WM_WINDOW_TYPE 过滤：--name 会误匹配同名窗口
W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | while read w; do
  xprop -id $w _NET_WM_WINDOW_TYPE 2>/dev/null | grep -q NORMAL && { echo $w; break; }
done | head -1)
echo "W=$W"
[ -z "$W" ] && { tail -25 /tmp/shot-docs.log; kill $APP 2>/dev/null; exit 1; }

xdotool windowsize $W 1440 960; sleep 1
xdotool windowmove $W 200 100; sleep 1
xdotool windowraise $W; xdotool windowactivate $W; sleep 3

nav() {   # $1 = 侧栏条目下标（0 起）
  xdotool windowactivate $W; sleep 0.4
  xdotool mousemove --window $W 90 200 click 1; sleep 0.5   # 让侧栏列表获焦
  xdotool key --window $W Home; sleep 0.4
  local d=0
  while [ $d -lt $1 ]; do xdotool key --window $W Down; sleep 0.28; d=$((d+1)); done
  xdotool key --window $W Return; sleep 2.2
}

declare -a NAMES=(01-overview 02-disk 03-protection 04-backup 05-update 06-policy 07-about)
PREV=""
for i in 0 1 2 3 4 5 6; do
  name=${NAMES[$i]}
  nav $i
  xdotool mousemove 20 20; sleep 0.6        # ← 必须：消除侧栏 hover
  cur="$RAW/$name.png"
  import -window $W "$cur"
  diffv=""
  if [ -n "$PREV" ]; then
    diffv=$(compare -metric AE "$PREV" "$cur" /dev/null 2>&1 | awk '{printf "%.0f",$1}')
    echo "  $name diff=${diffv:-?}"
  fi
  if [ -n "$PREV" ] && [ "${diffv:-0}" -lt 20000 ]; then
    echo "  !! $name 与上一页几乎相同，重试"
    nav $i
    xdotool mousemove 20 20; sleep 0.4
    import -window $W "$cur"
  fi
  PREV="$cur"
  identify -format "  saved %f %wx%h\n" "$cur"
done

echo "--- postprocess（裁剪窗口阴影 → 1200x800）---"
for f in "$RAW"/*.png; do
  convert "$f" -gravity center -crop 1610x1073+0+0 +repage -resize 1200x800! -strip "$OUT/$(basename "$f")"
done
identify -format "%f %wx%h %b\n" "$OUT"/*.png

kill $APP 2>/dev/null; sleep 1
rm -rf "$SHOT_HOME"
echo "done"
