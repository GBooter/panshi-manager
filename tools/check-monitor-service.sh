#!/bin/bash
# 磁盘监控服务自检 / 修复
#
# 背景：装完包后点「启用磁盘监控」没反应。排查中先后坐实两类根因，本脚本
#       对两者都有判据：
#   1) systemd **用户实例不认识**新装的单元 —— 单元文件落盘 ≠ 被扫描进管理器。
#      详见 src/pages/diskusagepage.cpp 里 ensureMonitorUnitLoaded() 的注释。
#   2) DConfig schema 的键缺 `permissions` 字段 —— 按 deepin 配置文件规范
#      「为空即只读」，set 退出码 0 却被 daemon 拒绝，读回恒为默认值，
#      表现为开关弹回「关」、后台进程零产出。已为全部键补 readwrite。
#
# ⚠️ 教训：只验「systemd 把进程拉起来了」是不够的 —— is-active=active 和
#    退出码 0 都证明不了进程干了活，必须验端到端产出（⑥）。
#
# 用法：
#   bash tools/check-monitor-service.sh          # 只诊断，不改任何状态
#   bash tools/check-monitor-service.sh --fix    # 诊断 + 修复（重载并启用）
#
# 退出码：0 = 全部通过；1 = 存在 FAIL

set -u

UNIT="panshi-monitor.timer"
SVC="panshi-monitor.service"
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

PASS=0
FAIL=0

ok()   { echo "  [PASS] $*"; PASS=$((PASS + 1)); }
bad()  { echo "  [FAIL] $*"; FAIL=$((FAIL + 1)); }
info() { echo "         $*"; }
step() { echo; echo "== $*"; }

# ---------------------------------------------------------------- ① 单元文件
step "① 单元文件是否落盘"
FOUND=""
for p in /usr/local/lib/systemd/user/$UNIT \
         /usr/lib/systemd/user/$UNIT \
         /etc/systemd/user/$UNIT \
         "$HOME/.config/systemd/user/$UNIT" \
         "$HOME/.local/share/systemd/user/$UNIT"; do
    [ -f "$p" ] && FOUND="$p" && break
done
if [ -n "$FOUND" ]; then
    ok "找到 $FOUND"
else
    bad "未找到 $UNIT（未安装或安装前缀不在查找路径内）"
    DEB_VER="$(sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' "$(dirname "$0")/../debian/changelog" 2>/dev/null)"
    info "→ 先装包：sudo dpkg -i panshi-manager_${DEB_VER:-<版本>}_$(dpkg --print-architecture 2>/dev/null || echo amd64).deb"
    info "  （只读系统上装不进 /usr 时，可用用户级部署：bash install.sh）"
fi

# -------------------------------------------------- ② systemd 用户会话可达
# ---- systemd 用户会话可达 ----
step "② systemd 用户会话是否可达"
SESSION_OK=0
if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ] && [ -z "${XDG_RUNTIME_DIR:-}" ]; then
    bad "既无 DBUS_SESSION_BUS_ADDRESS 也无 XDG_RUNTIME_DIR —— 当前不在桌面会话里"
    info "（sudo / ssh / tty 登录都是这样；请在图形会话的终端里跑）"
else
    info "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:--}  DBUS=${DBUS_SESSION_BUS_ADDRESS:--}"
fi
if systemctl --user show --property=Version >/dev/null 2>&1; then
    ok "systemctl --user 可用"
    systemctl --user show --property=Version 2>/dev/null | sed 's/^/         /'
    SESSION_OK=1
else
    bad "systemctl --user 连不上用户实例：$(systemctl --user show --property=Version 2>&1 | head -1)"
    info "（以 sudo/pkexec 运行时连的是 root 的实例，必然失败）"
fi

# -------------------------------------------- ③ systemd 是否已扫描到该单元
step "③ systemd 是否认识 $UNIT（这是「打不开」的直接判据）"
if [ "$SESSION_OK" != "1" ]; then
    # 同 ④：这里的失败可能只是「连不上实例」，不能据此断定单元没被扫描到
    echo "  [SKIP] 会话不可用，无法判断（连不上实例时 cat 也失败）"
    KNOWN=0
else
    # ⚠️ 只看出错码会误判：单元已被加载、只是磁盘上的文件更新过时，
    #    systemd 会输出 "changed on disk ... outdated" 并让 cat 返回非 0，
    #    看起来像「没识别」，其实只是需要 daemon-reload 刷新。
    CAT_OUT="$(systemctl --user cat "$UNIT" 2>&1)"
    CAT_RC=$?
    if [ "$CAT_RC" = "0" ]; then
        ok "已被 systemd 识别"
        KNOWN=1
    elif printf '%s' "$CAT_OUT" | grep -qi "changed on disk"; then
        ok "已被 systemd 识别（磁盘上的单元文件有更新，需 daemon-reload 刷新）"
        info "提示：加 --fix 可重载；或手工 systemctl --user daemon-reload"
        KNOWN=1
        NEED_RELOAD=1
    else
        bad "systemd 尚未识别 $UNIT：$(printf '%s' "$CAT_OUT" | head -1)"
        info "原因：用户实例只在启动 / daemon-reload 时扫描单元目录；"
        info "      装包时做不到这件事（dpkg 是 root，连不上用户会话总线）。"
        KNOWN=0
    fi
fi

# ---------------------------------------------------------- ④ 启用 / 运行状态
step "④ 启用与运行状态"
if [ "$SESSION_OK" != "1" ]; then
    # ⚠️ 关键坑：连不上用户实例时，systemctl 会对 is-enabled 走**离线判断**
    # （只看 ~/.config/systemd/user/*.wants 下有没有 symlink），于是明明
    # timer 根本没跑，它照样回答 "enabled"。这种假阳性会让人误判成
    # 「已启用但服务有问题」，从而查错方向 → 故此处一律标 SKIP，不判 PASS。
    echo "  [SKIP] 会话不可用，is-enabled / is-active 的结论不可信（离线误报）"
    info "离线 is-enabled = $(systemctl --user is-enabled "$UNIT" 2>&1 | head -1)（不可信）"
else
    IS_ENABLED="$(systemctl --user is-enabled "$UNIT" 2>&1 | head -1)"
    IS_ACTIVE="$(systemctl --user is-active "$UNIT" 2>&1 | head -1)"
    info "is-enabled = $IS_ENABLED"
    info "is-active  = $IS_ACTIVE"
    case "$IS_ENABLED" in
        enabled)  ok "已设为登录后自动启用" ;;
        *)        bad "未启用（$IS_ENABLED）" ;;
    esac
    case "$IS_ACTIVE" in
        active)   ok "timer 正在运行" ;;
        *)        bad "timer 未运行（$IS_ACTIVE）" ;;
    esac
fi

# ------------------------------------------------- ⑤ 配置源：到底谁说了算
# ⚠️ 这是「systemd 全绿但功能依然不可用」的真正判据：
#    systemctl 只证明进程被拉起了，证明不了进程读到的开关是开着的。
step "⑤ 监控开关的实际取值（GUI 与后台服务必须读同一份）"
CONF="$HOME/.config/deepin/panshi-manager.conf"
MON_EN=""
if [ -f "$CONF" ]; then
    ok "QSettings 配置存在：$CONF"
    MON_EN="$(awk '/^\[monitor\]/{f=1;next} /^\[/{f=0} f&&/^enabled=/{print substr($0,9)}' "$CONF")"
    info "monitor/enabled = ${MON_EN:-<未设置>}"
    case "$MON_EN" in
        true)  ok "QSettings 里监控是开启的" ;;
        *)     bad "QSettings 里监控未开启（$MON_EN）：后台服务会立即退出" ;;
    esac
else
    bad "QSettings 配置不存在：$CONF（GUI 从未成功写过设置）"
fi

# DConfig 主路径探测。
# ⚠️ 必须区分两种「写不进去」，它们的修法完全不同：
#   (a) schema 未安装到 /usr/share/dsg/configs → DConfig 整体不可用，
#       程序按设计降级到 QSettings，功能正常，不算故障；
#   (b) schema 已装但键缺 `permissions` → 按 deepin 规范「为空即只读」，
#       set 退出码 0 却被 daemon 拒绝 —— 这才是「开关打不开」的真凶。
DC="org.deepin.panshi-manager"
SCHEMA="/usr/share/dsg/configs/$DC/$DC.json"
if [ ! -f "$SCHEMA" ]; then
    info "DConfig schema 未安装：$SCHEMA"
    info "→ DConfig 不可用属预期（只读系统 / 用户级部署装不进 /usr），"
    info "  程序会降级到 QSettings，功能不受影响；要启用 DConfig 主路径请装包到 /usr。"
elif ! command -v dde-dconfig >/dev/null 2>&1; then
    info "未找到 dde-dconfig，跳过 DConfig 可写性探测"
else
    ok "DConfig schema 已安装：$SCHEMA"

    # (b) 先查 schema 里有没有缺 permissions 的键
    MISSING="$(python3 - "$SCHEMA" <<'PY' 2>/dev/null
import json, sys
try:
    j = json.load(open(sys.argv[1]))
except Exception as e:
    print("<无法解析: %s>" % e); sys.exit()
bad = [k for k, v in j.get('contents', {}).items() if v.get('permissions') != 'readwrite']
print(' '.join(bad))
PY
)"
    if [ -n "$MISSING" ]; then
        bad "schema 中有键缺少 permissions: readwrite → $MISSING"
        info "→ deepin 配置文件规范：permissions 为空即为**只读**，写入会被 daemon"
        info "  静默拒绝（set 退出码 0，读回仍是默认值）。这正是开关打不开的根因。"
    else
        ok "schema 全部键均为 permissions: readwrite"
    fi

    # 再做一次真实写入往返，确认后端可落盘
    CUR="$(dde-dconfig get -a "$DC" -r "$DC" -k monitor.snoozeUntil 2>/dev/null)"
    dde-dconfig set -a "$DC" -r "$DC" -k monitor.snoozeUntil -v "__panshi_probe__" >/dev/null 2>&1
    AFTER="$(dde-dconfig get -a "$DC" -r "$DC" -k monitor.snoozeUntil 2>/dev/null)"
    if [ "$AFTER" = "__panshi_probe__" ]; then
        ok "DConfig 写入可落盘（读回一致）"
        [ -n "$CUR" ] && dde-dconfig set -a "$DC" -r "$DC" -k monitor.snoozeUntil -v "$CUR" >/dev/null 2>&1
    else
        bad "DConfig 写入未生效：set 退出码 0，读回却是 '$AFTER'"
        info "（若上一项已报 permissions 缺失，先修 schema 再重装；"
        info "  程序当前会自动降级 QSettings，功能仍可用。）"
    fi
fi

# ------------------------------------------- ⑥ --monitor 端到端有没有产出
step "⑥ 端到端：跑一轮 --monitor，看数据到底动没动"
# 查找顺序：用户级部署（只读系统下唯一可行）→ 系统级安装 → 手工安装
BIN=""
for b in "$HOME/.local/bin/panshi-manager" \
         /usr/bin/panshi-manager \
         /usr/local/bin/panshi-manager; do
    [ -x "$b" ] && BIN="$b" && break
done
HIST="$HOME/.local/share/panshi-manager/disk-history"
TODAY="$HIST/$(date +%F).json"
if [ -z "$BIN" ]; then
    bad "找不到可执行的 panshi-manager"
else
    info "二进制：$BIN"
    info "今日数据：$TODAY"
    BEFORE="$(md5sum "$TODAY" 2>/dev/null | awk '{print $1}')"
    # ⚠️ 不要写成 `cmd | sed; RC=$?` —— 那样拿到的是 sed 的退出码，
    #    进程真实退出码（含 timeout 的 124）会被吞掉。
    OUT="$(timeout 120 "$BIN" --monitor 2>&1)"
    RC=$?
    [ -n "$OUT" ] && printf '%s\n' "$OUT" | sed 's/^/         /'
    AFTER="$(md5sum "$TODAY" 2>/dev/null | awk '{print $1}')"
    if [ "$RC" = "124" ]; then
        info "超过 120s 被中断 —— 全盘 du 扫描本来就慢，这本身不是故障；"
        info "请用 journalctl --user -u $SVC 确认最终结果。"
    elif [ "$BEFORE" = "$AFTER" ]; then
        bad "跑完一轮，今日数据文件毫无变化 → 进程被拉起后什么都没做"
        info "（退出码 $RC，通常还是 0 —— 「成功退出但零产出」最难发现）"
        info "典型原因：读到 monitor.enabled=false，runCycle() 首行即 finish() 返回。"
        info "排障：journalctl --user --since '10 min ago' -u $SVC"
    else
        ok "今日数据文件已更新：${BEFORE:-<无>} → ${AFTER:-<无>}"
    fi
fi

# ------------------------------------------------------------------- ⑦ 修复
if [ "$FIX" = "1" ]; then
    step "⑦ 修复"
    if [ "$SESSION_OK" != "1" ]; then
        bad "会话不可用：此时 enable 会走离线模式、只写一个 symlink，"
        info "并不能让 timer 真正跑起来，反而制造『已启用』的假象。拒绝执行。"
    elif [ -z "$FOUND" ]; then
        bad "单元文件不存在，无法修复（先装包）"
    else
        echo "  → systemctl --user daemon-reload"
        systemctl --user daemon-reload && ok "已重载" || bad "重载失败"
        if systemctl --user cat "$UNIT" >/dev/null 2>&1; then
            ok "重载后 systemd 已识别 $UNIT"
        else
            bad "重载后仍不识别：$(systemctl --user cat "$UNIT" 2>&1 | head -1)"
        fi
        echo "  → systemctl --user enable --now $UNIT"
        systemctl --user enable --now "$UNIT" && ok "已启用并启动" || bad "enable --now 失败"
        echo "  → systemctl --user start --no-block $SVC（立刻建基线，不等下一个整点）"
        systemctl --user start --no-block "$SVC" && ok "已触发首轮采样" || bad "start 失败"

        step "⑧ 修复后复检"
        info "is-enabled = $(systemctl --user is-enabled "$UNIT" 2>&1 | head -1)"
        info "is-active  = $(systemctl --user is-active "$UNIT" 2>&1 | head -1)"
        systemctl --user is-active "$UNIT" >/dev/null 2>&1 \
            && ok "timer 已 active" || bad "timer 仍未 active"
    fi
else
    echo
    echo "（仅诊断，未改任何状态。加 --fix 可一键重载并启用）"
fi

echo
echo "=============================================="
echo "  PASS=$PASS  FAIL=$FAIL"
[ "$FAIL" = "0" ] && echo "  结论：全部通过" || echo "  结论：存在问题，按上面的 [FAIL] 逐条处理"
exit $((FAIL > 0))
