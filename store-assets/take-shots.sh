#!/usr/bin/env bash
# 磐石设置 中文界面商店截图采集（单次调用完成：启动+导航+截图+后处理+收尾）
set -u
REPO="/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置"
SHOTS="$REPO/store-assets/shots"
mkdir -p "$SHOTS"
export DISPLAY=:0
WIN_W=1440; WIN_H=960

SHOT_HOME="/tmp/shot-home-zh"
rm -rf "$SHOT_HOME"
mkdir -p "$SHOT_HOME/.local/share/panshi-manager/translations" "$SHOT_HOME/.config/deepin"
printf '[General]\nlanguage=zh_CN\n' > "$SHOT_HOME/.config/deepin/panshi-manager.conf"
export HOME="$SHOT_HOME"
export XDG_RUNTIME_DIR="/tmp/xdg-shot-zh"
mkdir -p "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"

log(){ echo "[zh] $*"; }

find_win(){
  xdotool search --onlyvisible --name ".*" 2>/dev/null | while read w; do
    cls=$(xprop -id "$w" WM_CLASS 2>/dev/null)
    echo "$cls" | grep -qi "panshi" || continue
    st=$(xwininfo -id "$w" 2>/dev/null | awk -F': ' '/Map State/{print $2}')
    [ "$st" = "IsViewable" ] && { echo "$w"; break; }
  done
}

# 造磁盘页演示数据（写入临时 HOME）
cd "$REPO"
python3 make-testdata.py --go spike >/tmp/mkdata-zh.log 2>&1 && log "testdata ok" || log "testdata FAIL $(tail -2 /tmp/mkdata-zh.log)"

# 启动应用
cd "$REPO/build"
env -u LC_ALL LANG=zh_CN.UTF-8 HOME="$SHOT_HOME" XDG_RUNTIME_DIR=/tmp/xdg-shot-zh QT_QPA_PLATFORMTHEME=deepin \
  ./panshi-manager >/tmp/shot-zh.log 2>&1 &
APP=$!
sleep 8
W=$(find_win)
log "W=$W"
[ -z "$W" ] && { log "FATAL: no window"; tail -20 /tmp/shot-zh.log; exit 1; }
xdotool windowsize "$W" $WIN_W $WIN_H; sleep 1
xdotool windowmove "$W" 880 520; sleep 1
xdotool windowraise "$W"; xdotool windowactivate "$W"; sleep 3

shot(){ import -window "$W" "$1"; }

# 页面：0总览 1磁盘 2保护 3备份 4更新 5策略
declare -a NAMES=(overview disk protect backup update policy)
declare -a IDX=(0 1 2 3 4 5)

PREV=""
for i in 0 1 2 3 4 5; do
  k=${IDX[$i]}; name=${NAMES[$i]}
  xdotool windowactivate "$W"; sleep 0.4
  xdotool mousemove --window "$W" 90 200 click 1; sleep 0.5   # 让侧栏 list 获焦
  xdotool key --window "$W" Home; sleep 0.3                   # 选中总览(顶)
  for d in $(seq 1 $k); do xdotool key --window "$W" Down; sleep 0.25; done
  xdotool key --window "$W" Return; sleep 1.4
  cur="$SHOTS/_cur.png"
  shot "$cur"
  ok=0
  if [ -n "$PREV" ]; then
    diffv=$(compare -metric AE "$PREV" "$cur" /dev/null 2>&1 | awk '{printf "%.0f",$1}')
    [ "${diffv:-0}" -gt 100000000 ] && ok=1
  else
    ok=1
  fi
  if [ "$ok" -eq 0 ]; then
    log "page $name diff=$diffv too small, retry"
    xdotool windowactivate "$W"; sleep 0.3
    xdotool mousemove --window "$W" 90 200 click 1; sleep 0.4
    xdotool key --window "$W" Home; sleep 0.3
    for d in $(seq 1 $k); do xdotool key --window "$W" Down; sleep 0.25; done
    xdotool key --window "$W" Return; sleep 1.4
    shot "$cur"
  fi
  cp "$cur" "$SHOTS/shot_$((i+1))_${name}.png"
  PREV="$SHOTS/shot_$((i+1))_${name}.png"
  log "saved shot_$((i+1))_${name}.png"
done

# 后处理：裁剪到精确 3:2 并缩放到 1440x960
cd "$REPO/store-assets"
for n in 1_overview 2_disk 3_protect 4_backup 5_update 6_policy; do
  convert "$SHOTS/shot_${n}.png" -gravity center -crop 1610x1073+0+0 +repage -resize 1440x960! -strip "panshi-manager-${n}.png"
done
log "postprocess done"
identify -format "%f %wx%h %b\n" panshi-manager-*.png
md5sum panshi-manager-*.png | awk '{print $1}' | sort | uniq -c

# 收尾
kill $APP 2>/dev/null
rm -rf "$SHOT_HOME"
log "cleanup done"
