#!/bin/bash
# 磐石设置 — 商店截图采集脚本 v4
# 基于 v3 实证：键盘导航有效（focus 稳）、磁盘演示数据渲染完美
# v4 修正：① 侧栏自然顺序逐页 Down×1（v3 累计 downs 切错页）
#           ② AE 科学计数法转整数（v3 阈值判断静默失败）
#           ③ 每页 AE 验证，失败才重置重试
set -u
REPO="/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置"
OUT="$REPO/store-assets/shots"
rm -rf "$OUT"; mkdir -p "$OUT"
export DISPLAY=:0
WIN_W=1440; WIN_H=960

log() { echo "[shot] $*"; }

# ---------- 1. 造磁盘页演示数据 ----------
python3 "$REPO/make-testdata.py" --go spike >/tmp/shot-testdata.log 2>&1
log "testdata rc=$?"

# ---------- 2. 清理旧实例 + 启动 ----------
pkill -f panshi-manager 2>/dev/null; sleep 1
rm -rf /tmp/xdg-shot; mkdir -p /tmp/xdg-shot; chmod 700 /tmp/xdg-shot
cd "$REPO/build"
env XDG_RUNTIME_DIR=/tmp/xdg-shot QT_QPA_PLATFORMTHEME=deepin \
    ./panshi-manager >/tmp/shot-app.log 2>&1 &
APP_PID=$!
log "app pid=$APP_PID"

WID=""
for i in $(seq 1 60); do
  for w in $(xdotool search --onlyvisible --name "磐石" 2>/dev/null); do
    if xwininfo -id "$w" | grep -q "Map State: IsViewable"; then WID=$w; break; fi
  done
  [ -n "$WID" ] && break
  sleep 0.5
done
[ -z "$WID" ] && { log "FATAL: window not found"; tail -5 /tmp/shot-app.log; kill $APP_PID 2>/dev/null; exit 1; }
log "window=$WID proc=$(ps -p $APP_PID -o comm= 2>/dev/null || echo DEAD)"

xdotool windowsize "$WID" $WIN_W $WIN_H; sleep 0.5
xdotool windowmove "$WID" 880 520; sleep 0.3
xdotool windowraise "$WID"; xdotool windowactivate "$WID"
sleep 4

# ---------- 3. 截图 + 差异验证 ----------
shot() { rm -f "$1"; import -window "$WID" "$1" 2>/dev/null; [ -s "$1" ] && log "OK $1 $(identify -format '%wx%h' "$1")" || log "FAIL $1"; }

# AE 可能是科学计数法，awk 转整数
ae_int() {
  local raw
  raw=$(compare -metric AE "$1" "$2" null: 2>&1 | head -1)
  echo "$raw" | awk '{printf "%.0f", $1}'
}

# ---------- 4. 初始：点侧栏让 list 获焦 + 总览截图 ----------
# ⚠️ 基线用独立文件：v4 曾因 PREV 复用 shot_1 路径，cp 覆盖丢了总览截图
BASELINE="$OUT/.baseline.png"
xdotool mousemove --window "$WID" 100 120 click 1; sleep 1
shot "$OUT/shot_1_overview.png"
cp "$OUT/shot_1_overview.png" "$BASELINE"

# ---------- 5. 逐页：Down×1 + Return（失败则 Up×10 重置后重试） ----------
nav_to() {
  local name="$1" shotfile="$2" prevshot="$3" downs="$4"
  local ae ok=0
  xdotool key Down; sleep 0.2; xdotool key Return; sleep 3.5
  shot "$shotfile"
  ae=$(ae_int "$prevshot" "$shotfile")
  log "  nav $name -> AE=${ae}"
  if [ "${ae:-0}" -gt 50000000 ]; then ok=1; fi
  if [ $ok -eq 0 ]; then
    # 重置：Up×10 回总览 → Return → Down×downs → Return
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
  log "  ✗ $name FAILED"
  return 1
}

# 侧栏顺序（总览起）：磁盘1 保护2 备份3 更新4 策略5
nav_to "磁盘空间"   "$OUT/shot_2_disk.png"    "$BASELINE" 1
nav_to "系统保护"   "$OUT/shot_3_protect.png" "$BASELINE" 2
nav_to "备份与版本" "$OUT/shot_4_backup.png"  "$BASELINE" 3
nav_to "系统更新"   "$OUT/shot_5_update.png"  "$BASELINE" 4
nav_to "策略与设置" "$OUT/shot_6_policy.png"  "$BASELINE" 5

# ---------- 6. 收尾 ----------
kill $APP_PID 2>/dev/null; sleep 1
pkill -f panshi-manager 2>/dev/null
rm -f "$BASELINE"
rm -rf /root/.local/share/panshi-manager/disk-history
rm -rf /tmp/xdg-shot
log "DONE."
ls -la "$OUT/"
