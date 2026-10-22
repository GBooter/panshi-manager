#!/bin/bash
# 磐石设置 — 商店截图采集脚本（英文界面版）
# 与中文版 take-shots.sh 同构，差异：
#   · 预写 QSettings [General] language=en_US + 进程环境 LANGUAGE/LANG=en_US
#   · 清掉 LC_ALL（main.cpp：LC_ALL 优先级最高，会盖掉语言选择）
#   · 窗口定位按 WM_CLASS（英文下窗口标题是 "Panshi Manager"，不再是「磐石设置」）
set -u
REPO="/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置"
OUT="$REPO/store-assets/en/shots"
# ⚠️ 应用翻译加载顺序（main.cpp）：~/.local/share → /usr/share → exe 同级。
#    /usr/share/panshi-manager/translations 有旧版 qm 且只读不可改，
#    故把 HOME 指到临时目录，用优先级最高的 ~/.local/share 命中新译文。
SHOT_HOME="/tmp/shot-home"
CONF="$SHOT_HOME/.config/deepin/panshi-manager.conf"
mkdir -p "$OUT"
export DISPLAY=:0
WIN_W=1440; WIN_H=960

log() { echo "[shot-en] $*"; }

# ---------- 1. 语言偏好：en_US + 预置新 qm（采完整体清理） ----------
rm -rf "$SHOT_HOME"
mkdir -p "$SHOT_HOME/.config/deepin" "$SHOT_HOME/.local/share/panshi-manager/translations"
cp "$REPO/translations/panshi-manager_en_US.qm" \
   "$SHOT_HOME/.local/share/panshi-manager/translations/"
printf '[General]\nlanguage=en_US\n' > "$CONF"
# 后续命令（造数据 + 应用）统一用临时 HOME：磁盘历史与翻译都落在其中
export HOME="$SHOT_HOME"

# ---------- 2. 造磁盘页演示数据 ----------
python3 "$REPO/make-testdata.py" --go spike >/tmp/shot-testdata.log 2>&1
log "testdata rc=$?"

# ---------- 3. 清理旧实例 + 启动（英文环境） ----------
pkill -f panshi-manager 2>/dev/null; sleep 1
rm -rf /tmp/xdg-shot; mkdir -p /tmp/xdg-shot; chmod 700 /tmp/xdg-shot
cd "$REPO/build"
env -u LC_ALL LANG=en_US.UTF-8 LANGUAGE=en_US HOME="$SHOT_HOME" \
    XDG_RUNTIME_DIR=/tmp/xdg-shot QT_QPA_PLATFORMTHEME=deepin \
    ./panshi-manager >/tmp/shot-app-en.log 2>&1 &
APP_PID=$!
log "app pid=$APP_PID"

# 窗口定位：优先 WM_CLASS 含 panshi，其次 name 含 Panshi/Manager
find_win() {
  local w cls
  for w in $(xdotool search --onlyvisible --name "." 2>/dev/null); do
    cls=$(xprop -id "$w" WM_CLASS 2>/dev/null)
    if echo "$cls" | grep -qi "panshi"; then
      xwininfo -id "$w" | grep -q "Map State: IsViewable" && { echo "$w"; return; }
    fi
  done
  for w in $(xdotool search --onlyvisible --name "Panshi" 2>/dev/null); do
    xwininfo -id "$w" | grep -q "Map State: IsViewable" && { echo "$w"; return; }
  done
}

WID=""
for i in $(seq 1 60); do
  WID=$(find_win)
  [ -n "$WID" ] && break
  sleep 0.5
done
[ -z "$WID" ] && { log "FATAL: window not found"; tail -5 /tmp/shot-app-en.log; kill $APP_PID 2>/dev/null; exit 1; }
log "window=$WID title=[$(xdotool getwindowname $WID 2>/dev/null)] proc=$(ps -p $APP_PID -o comm= 2>/dev/null || echo DEAD)"

xdotool windowsize "$WID" $WIN_W $WIN_H; sleep 0.5
xdotool windowmove "$WID" 880 520; sleep 0.3
xdotool windowraise "$WID"; xdotool windowactivate "$WID"
sleep 4

# ---------- 4. 截图 + 差异验证 ----------
shot() { rm -f "$1"; import -window "$WID" "$1" 2>/dev/null; [ -s "$1" ] && log "OK $1 $(identify -format '%wx%h' "$1")" || log "FAIL $1"; }
ae_int() { compare -metric AE "$1" "$2" null: 2>&1 | head -1 | awk '{printf "%.0f", $1}'; }

BASELINE="$OUT/.baseline.png"
xdotool mousemove --window "$WID" 100 120 click 1; sleep 1
shot "$OUT/shot_1_overview.png"
cp "$OUT/shot_1_overview.png" "$BASELINE"

nav_to() {
  local name="$1" shotfile="$2" prevshot="$3" downs="$4"
  local ae ok=0
  xdotool key Down; sleep 0.2; xdotool key Return; sleep 3.5
  shot "$shotfile"
  ae=$(ae_int "$prevshot" "$shotfile")
  log "  nav $name -> AE=${ae}"
  [ "${ae:-0}" -gt 50000000 ] && ok=1
  if [ $ok -eq 0 ]; then
    log "  retry $name (reset)..."
    for i in $(seq 1 10); do xdotool key Up; sleep 0.12; done
    xdotool key Return; sleep 3
    for i in $(seq 1 $downs); do xdotool key Down; sleep 0.15; done
    xdotool key Return; sleep 3.5
    shot "$shotfile"
    ae=$(ae_int "$prevshot" "$shotfile")
    log "  nav $name (retry) -> AE=${ae}"
    [ "${ae:-0}" -gt 50000000 ] && ok=1
  fi
  [ $ok -eq 1 ] && { cp "$shotfile" "$prevshot"; log "  ✓ $name"; return 0; }
  log "  ✗ $name FAILED"; return 1
}

nav_to "Disk usage"  "$OUT/shot_2_disk.png"    "$BASELINE" 1
nav_to "Protection"  "$OUT/shot_3_protect.png" "$BASELINE" 2
nav_to "Backup"      "$OUT/shot_4_backup.png"  "$BASELINE" 3
nav_to "Updates"     "$OUT/shot_5_update.png"  "$BASELINE" 4
nav_to "Policy"      "$OUT/shot_6_policy.png"  "$BASELINE" 5

# ---------- 5. 收尾 + 语言恢复 ----------
kill $APP_PID 2>/dev/null; sleep 1
pkill -f panshi-manager 2>/dev/null
rm -f "$BASELINE"
rm -rf "$SHOT_HOME"
rm -rf /root/.local/share/panshi-manager/disk-history
rm -rf /tmp/xdg-shot
log "DONE."
ls -la "$OUT/"
