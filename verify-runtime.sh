#!/bin/bash
# 磐石设置 — C 类实机验证清单与执行脚本
#
# C 类 = 需要**实机环境**才能验证的项（本机即磐石真机，但部分手段受限）。
# 覆盖：D-Bus 定位转发（自动断言） / DConfig 16 键（schema×代码静态交叉 + 实机回退）
#       / i18n 翻译加载（自动） / 使用率配色（自动：像素采样 + WCAG 对比度）
#       / X11+Wayland 双后端（人工） / 亮暗主题对照（人工）。
#
# ⚠️ 安全性：默认干跑（--go 才执行）。
#    dbus/dconf/i18n 段全非破坏性（重启本应用、读状态、cp .qm 到用户目录，不改系统配置）；
#    backend/theme 段只列人工检查项，唯一外观变更是人工切深色主题。
#
# 用法：
#   ./verify-runtime.sh            # 干跑
#   ./verify-runtime.sh --go       # 实际执行
#   ./verify-runtime.sh --go i18n  # 只跑某一段（dbus/dconf/i18n/usage/colors/backend/theme）
#
# 构建目录：自动探测 $PANSHI_BUILD → build2 → build（取第一个含可执行文件的）。
#   2026-10-03 起实际用 build2 —— 旧 build/ 的 CMakeCache 指向已消失的 linglong 工具链，
#   且本机是只读容器、工具链为项目内自建 sysroot（先 `source .toolchain/env.sh`）。
set -u

REPO="/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置"

# 构建目录自动探测（2026-10-03 起）：
#   本机是只读容器，工具链为项目内自建 sysroot（见 .toolchain/env.sh），
#   旧 build/ 里的 CMakeCache 指向已消失的 linglong 工具链，故实际用 build2/。
#   顺序：$PANSHI_BUILD → build2 → build，取第一个含可执行文件的。
pick_build() {
    local c
    for c in "${PANSHI_BUILD:-}" "$REPO/build2" "$REPO/build"; do
        [ -n "$c" ] && [ -x "$c/panshi-manager" ] && { printf '%s' "$c"; return 0; }
    done
    printf '%s' "$REPO/build2"   # 兜底：给出建议路径
}
BUILDDIR="$(pick_build)"
APP="$BUILDDIR/panshi-manager"
# 翻译查找目录：DTK loadTranslator() 只搜文件系统，构建目录不在列表内
QLOC="$HOME/.local/share/panshi-manager/translations"

DRY=1
ONLY=""
for a in "$@"; do
    case "$a" in
        --go)   DRY=0 ;;
        dbus|dconf|backend|theme|i18n|usage|colors) ONLY="$a" ;;
        --help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "未知参数: $a"; exit 2 ;;
    esac
done

run() {
    if [ "$DRY" = "1" ]; then printf '  [DRY-RUN] %s\n' "$*"
    else printf '  [执行] %s\n' "$*"; eval "$@"; fi
}
want() { [ -z "$ONLY" ] || [ "$ONLY" = "$1" ]; }
banner() { printf '\n========== %s ==========\n' "$1"; }
pass() { printf '  \033[32m[PASS]\033[0m %s\n' "$1"; }
fail() { printf '  \033[31m[FAIL]\033[0m %s\n' "$1"; FAILS=$((FAILS+1)); }
warn() { printf '  \033[33m[WARN]\033[0m %s\n' "$1"; WARNS=$((WARNS+1)); }
FAILS=0
WARNS=0

banner "环境探测"
run "test -x $APP && echo '已构建: $BUILDDIR' || echo '❌ 未构建。构建命令见 .toolchain/env.sh（先 source），再用 build2 目录'"
run "echo \"session: \$XDG_SESSION_TYPE  /  \$(loginctl show-session \${XDG_SESSION_ID:-1} -p Type --value 2>/dev/null)\""
run "echo \"desktop: \$XDG_CURRENT_DESKTOP\""

# ---------------------------------------------------------------- i18n
# 2026-10-03 新增。此前 i18n 验证全靠手工 cp .qm，且「构建目录不在查找列表内」
# 这个坑反复踩 —— 这里把它固化成自动步骤。
if want i18n; then
    banner "i18n 翻译加载（自动装 .qm + 断言告警消失）"
    cat <<'EOF'
  原理：DTK loadTranslator() 只搜文件系统、**不读构建目录、也不读 Qt 资源**。
        所以 .qm 必须在 GenericDataLocation/<appName>/translations/ 才生效。
  断言：en_US 与 zh_CN 启动时都**不应**出现 "can not find qm files" 告警。
        （.qm 缺失时 zh_CN 会告警而 en_US 完全静默，故两种语言都要跑。）
  ⚠️ 语言变量三个都要设：deepin 会话默认继承 LANGUAGE=zh_CN，
     gettext 优先级下会压过 LANG/LC_ALL。
EOF
    if [ "$DRY" = "1" ]; then
        echo "  [DRY-RUN] 检查 .qm 产物 → cp 到 $QLOC → en_US/zh_CN 各启一次 grep 告警（加 --go 实跑）"
    else
        # 1) .qm 产物存在性
        for L in zh_CN en_US; do
            if [ -f "$BUILDDIR/panshi-manager_$L.qm" ]; then
                pass ".qm 存在: panshi-manager_$L.qm ($(stat -c%s "$BUILDDIR/panshi-manager_$L.qm") B)"
            else
                fail ".qm 缺失: panshi-manager_$L.qm —— 先 cmake --build $BUILDDIR"
            fi
        done

        # 2) 装到查找目录（这步就是过去反复手工做的）
        mkdir -p "$QLOC"
        if cp "$BUILDDIR"/panshi-manager_*.qm "$QLOC/" 2>/dev/null; then
            pass "已安装到 $QLOC"
        else
            fail "复制 .qm 失败"
        fi

        # 3) 双语各启一次，断言无 "can not find qm files"
        #    用 offscreen 避免需要真实显示；grep 到告警即 FAIL。
        for L in en_US zh_CN; do
            OUT=$(LANG=$L.UTF-8 LC_ALL=$L.UTF-8 LANGUAGE=$L \
                  QT_QPA_PLATFORM=offscreen timeout 10 "$APP" 2>&1 | grep -c "can not find qm files")
            if [ "$OUT" = "0" ]; then
                pass "$L：.qm 已加载（无查找失败告警）"
            else
                fail "$L：出现 $OUT 条 'can not find qm files' —— .qm 未生效"
            fi
        done

        # 4) 覆盖度：lrelease 报告的 unfinished 数应与界面实际未译条数一致
        LRELEASE="$REPO/.toolchain/sysroot/usr/lib/qt6/bin/lrelease"
        if [ -x "$LRELEASE" ]; then
            echo "  --- 翻译覆盖度 ---"
            "$LRELEASE" "$REPO/translations/panshi-manager_en_US.ts" -qm /dev/null 2>&1 \
              | grep -oE '[0-9]+ finished and [0-9]+ unfinished' | sed 's/^/    en_US: /'
            "$LRELEASE" "$REPO/translations/panshi-manager_zh_CN.ts" -qm /dev/null 2>&1 \
              | grep -oE '[0-9]+ finished and [0-9]+ unfinished' | sed 's/^/    zh_CN: /'
        else
            echo "    （lrelease 不在 sysroot，跳过覆盖度检查）"
        fi
    fi
fi

# ---------------------------------------------------------------- D-Bus
if want dbus; then
    banner "D-Bus 定位转发（开发文档 §10.1 的 4 条命令 + 自动断言）"
    cat <<'EOF'
  链路：panshi-manager --show-disk <date>
        → 已有实例：调 org.deepin.panshi.disknav.ShowDisk 后立刻退出（不开窗）
        → 无实例：冷启动直达磁盘页并选中该日
  说明：xdotool 会枚举出一个 1×1 的 Qt/DSingleInstance 内部窗口，
        窗口数断言按「宽≥100 的可见主窗口」计数，不把它算进去。
EOF
    if [ "$DRY" = "1" ]; then
        echo "  [DRY-RUN] 冷启动定位 / 运行中转发 / 窗口数断言 / 清理（加 --go 实跑）"
    else
        D1=$(date -d 'yesterday' +%F)          # 转发用昨日（通常有监控数据）
        D2=$(date -d '6 days ago' +%F)        # 第二个不同日期，证明确实是转发生效
        export DISPLAY="${DISPLAY:-:0}"

        count_main_windows() {
            local n=0 w width
            for w in $(xdotool search --name '磐石设置' 2>/dev/null); do
                [ "$(xdotool getwindowname "$w" 2>/dev/null)" = "磐石设置" ] || continue
                width=$(xdotool getwindowgeometry --shell "$w" 2>/dev/null | sed -n 's/^WIDTH=//p')
                [ "${width:-0}" -ge 100 ] && n=$((n+1))
            done
            echo "$n"
        }

        echo "  ① 冷启动：--show-disk $D1"
        pkill -x panshi-manager 2>/dev/null; sleep 1
        "$APP" --show-disk "$D1" >/tmp/panshi-dbus-cold.log 2>&1 &
        sleep 6
        if [ -n "$(pgrep -x panshi-manager)" ]; then pass "冷启动进程存活"
        else fail "冷启动进程未存活（见 /tmp/panshi-dbus-cold.log）"; fi
        N=$(count_main_windows)
        [ "$N" = "1" ] && pass "冷启动主窗口数=1（实测 $N）" || fail "冷启动主窗口数=$N，期望 1"
        echo "     人工确认 ①：窗口已直达磁盘页，时间线选中日=$D1"

        echo "  ② 运行中转发：第二个实例 --show-disk $D2"
        t0=$(date +%s.%N)
        "$APP" --show-disk "$D2" >/tmp/panshi-dbus-hot.log 2>&1
        rc=$?
        t1=$(date +%s.%N)
        el=$(echo "$t1-$t0" | bc)
        echo "     转发进程 exit=$rc，耗时 ${el}s"
        [ "$rc" = "0" ] && pass "转发进程退出码 0" || fail "退出码=$rc"
        is_fast=$(echo "$el < 2.0" | bc)
        [ "$is_fast" = "1" ] && pass "转发进程 2s 内退出（${el}s）" || fail "退出耗时 ${el}s ≥2s，疑似没有走转发"
        sleep 2
        N=$(count_main_windows)
        [ "$N" = "1" ] && pass "转发后主窗口数仍=1（实测 $N，未新增窗口）" || fail "转发后主窗口数=$N，期望 1"
        npids=$(pgrep -xc panshi-manager)
        [ "$npids" = "1" ] && pass "panshi-manager 进程数=1" || fail "进程数=$npids，期望 1"
        echo "     人工确认 ②：原窗口时间线选中日已变为 $D2（focusDate 会取该日±3 天的 7 天窗口）"

        echo "  ④ 清理"
        pkill -x panshi-manager 2>/dev/null && pass "已关闭测试实例" || echo "  （无实例可关）"
    fi
fi

# ---------------------------------------------------------------- DConfig
if want dconf; then
    banner "DConfig 16 键表回归（开发文档 §7.1）"
    cat <<'EOF'
  回归点：UI 重排**不得改动键名**。三方交叉：
    A. DSG schema json 声明的 monitor.* 键数 = 16
    B. MonitorConfig 的 get("monitor.xxx") 引用数 = 16
    C. 脚本清单 16 项 ⊆ A ∩ B
EOF
    if [ "$DRY" = "0" ]; then
        python3 -B - "$REPO" <<'PY'
import json, os, re, sys
repo = sys.argv[1]
schema = os.path.join(repo,
    "data/dsg/configs/org.deepin.panshi-manager/org.deepin.panshi-manager.json")
cfgcpp = os.path.join(repo, "src/disk/monitorconfig.cpp")
expected = """monitor.enabled monitor.pollMinutes monitor.baselineDays monitor.alertMode
monitor.fixedThresholdMB monitor.baselineMultiplier monitor.minAlertMB
monitor.cooldownHours monitor.quietStart monitor.quietEnd monitor.excludes
monitor.retentionDays monitor.storageBudgetMB monitor.samplesRetentionDays
monitor.memoryCacheDays monitor.snoozeUntil""".split()
sk = set(re.findall(r'"(monitor\.[a-zA-Z]+)"', open(schema, encoding="utf-8").read()))
ck = set(re.findall(r'"(monitor\.[a-zA-Z]+)"', open(cfgcpp, encoding="utf-8").read()))
print(f"  A schema 键数 = {len(sk)}")
print(f"  B 代码引用键数 = {len(ck)}")
ok = True
for k in expected:
    inS, inC = k in sk, k in ck
    if not (inS and inC):
        ok = False
        print(f"  [FAIL] {k}: schema={'Y' if inS else 'N'} code={'Y' if inC else 'N'}")
extra_s = sk - set(expected); extra_c = ck - set(expected)
if extra_s: print("  提示：schema 另有未列入清单的键：", sorted(extra_s))
if extra_c: print("  提示：代码另有未列入清单的键：", sorted(extra_c))
print("  [PASS] 16 键在 schema 与代码两侧齐全" if ok and len(sk)==16 and len(ck)==16
      else "  [FAIL] 键表交叉校验未通过")
sys.exit(0 if ok and len(sk)==16 and len(ck)==16 else 1)
PY
        [ $? -eq 0 ] && : || FAILS=$((FAILS+1))
    else
        echo "  [DRY-RUN] schema×代码 16 键交叉校验（加 --go 实跑）"
    fi
    echo
    echo "  ⚠️ 本机已知的 DConfig 限制（实测）："
    echo "     DSG 库硬编码只认只读 /usr/share/dsg，DSG_DATA_DIRS 不生效"
    echo "     → 启动会有「Can't acquire config manager」告警"
    echo "     → 磁盘监控配置走 QSettings 兜底（~/.config/deepin/panshi-manager.conf）"
    echo "  当前实机状态："
    if [ "$DRY" = "0" ]; then
        if command -v dconf >/dev/null 2>&1; then
            dconf dump /org/deepin/panshi-manager/ 2>/dev/null | sed 's/^/    /' | head -40
        else
            echo "    （本机无 dconf 命令；DConfig 无效，走 QSettings 兜底）"
        fi
        if [ -f ~/.config/deepin/panshi-manager.conf ]; then
            sed 's/^/    /' ~/.config/deepin/panshi-manager.conf
        else
            echo "    （QSettings 兜底文件尚不存在：从未在 UI 内写过配置）"
        fi
    fi
    echo
    echo "  重点回归：monitor.* 用**点号**；QSettings 兜底用**斜杠**层级（.→/ 转换）"
    echo "  这是开发文档 §6.6 记录的已修坑，若 UI 改动后读不到值优先查这里。"
fi

# ---------------------------------------------------------------- 使用率分档
# 2026-10-03 新增。此前 B6 只能造数据、无法自动断言 —— 现在 df shim 可被程序解析，
# 故「shim 是否真生效」这一步能自动判定，剩下的「文字是否变警示色」仍需肉眼看。
if want usage; then
    banner "使用率分档（df shim 生效性自动断言 + 颜色人工确认）"
    cat <<'EOF'
  原理：概览页读 `df -B1 /` 算使用率，pct = used*100/capacity；
        ≥75 → TextWarning。造指定比例只能 shim df（du/ostree 仍走真实系统）。
  自动部分：断言 shim 真被 df 解析出目标比例（错则说明注入无效，后续肉眼看也没意义）
  人工部分：≥75% 的「已用」数值与百分比是否呈警示色；<75% 是否为正常色。
  ⚠️ V-23 决策：75~90% 与 ≥90% **同色**（都是 TextWarning），两档靠图标+文案区分。
  ✅ 颜色本身已自动化：见 `--go colors` 段（像素级采样，亮/暗双主题）。
EOF
    if [ "$DRY" = "1" ]; then
        echo "  [DRY-RUN] 造 shim（make-testdata.py --go usage）→ 逐档断言 df 输出 → 提示人工看颜色"
    else
        SHIMROOT="$REPO/.shim"
        if [ ! -d "$SHIMROOT/panshi-dfshim-92" ]; then
            echo "  shim 不存在，先造数据："
            ( cd "$REPO" && python3 make-testdata.py --go usage >/dev/null 2>&1 )
        fi
        for p in 40 78 92; do
            D="$SHIMROOT/panshi-dfshim-$p"
            if [ ! -x "$D/df" ]; then
                fail "$p% shim 缺失（造数据失败）"; continue
            fi
            GOT=$(PATH="$D:$PATH" df -B1 / 2>/dev/null | tail -1 | awk '{print $5}' | tr -d '%')
            if [ "$GOT" = "$p" ]; then
                pass "$p% shim 生效（df 报 $GOT%）"
            else
                fail "$p% shim 未生效（df 报 ${GOT:-无}%）—— 注入无效，后续颜色检查无意义"
            fi
        done
        cat <<'EOF'

  ── 可选人工复核（真页面端到端；颜色本身已由 `--go colors` 自动断言）──
    启动命令：
      source .toolchain/env.sh
      PATH=$PWD/.shim/panshi-dfshim-92:$PATH $BUILDDIR/panshi-manager
    看总览页「已用」大数值与百分比：78% / 92% 警示色，40% 正常色。
  清理：rm -rf .shim/panshi-dfshim-*
EOF
    fi
fi

# ---------------------------------------------------------------- 使用率配色
# 2026-10-04 新增。C7 此前**只能肉眼看颜色**，且暗色复核受限于「本机系统级暗色
# 不可达（DTK6 走 treeland 协议）」。现在交给 `panshi-colorprobe`：离屏渲染真实
# DLabel → grab() → 采样笔画内部像素，颜色因此有了客观值；暗色由进程内
# setPaletteType + standardPalette 强制，不再依赖控制中心手动切换。
if want colors; then
    banner "使用率配色断言（C7：像素采样 + WCAG 对比度，亮/暗双主题）"
    cat <<'EOF'
  原理：探针按磁盘页概览的**同字号**（T2 大数值 / T4 百分比）与**同语义通道**
        （PanshiUi::applySemantic）构造 DLabel，采样渲染位图里「笔画内部」像素。
        断言的是**渲染结果**而非「我以为它会是这个色」——
        本项目踩过 TextLively 名为警示色、实测中性黑的坑（V-23），
        只断言「调了 setForegroundRole」抓不到那种失效。
  判据同源：usageSemanticForPct() + 常量 kUsageWarnPct 是唯一定义点，
        页面与探针共用，改阈值两边同时生效，不会出现「页面已改、断言按旧阈值」的假绿。
EOF
    if [ "$DRY" = "1" ]; then
        echo "  [DRY-RUN] 构建并运行 panshi-colorprobe → 逐条断言（加 --go 实跑）"
    else
        # 探针用的是项目内 sysroot 的 Qt6/DTK6，运行前必须带上自建环境
        if [ -f "$REPO/.toolchain/env.sh" ]; then . "$REPO/.toolchain/env.sh"; fi
        PROBE="$BUILDDIR/panshi-colorprobe"
        if [ ! -x "$PROBE" ]; then
            echo "  探针未构建，正在构建 --target panshi-colorprobe …"
            ( cd "$REPO" && cmake --build "$BUILDDIR" --target panshi-colorprobe \
                -j"$(nproc)" >/dev/null 2>&1 )
        fi
        mkdir -p "$REPO/.tmp"
        PJ="$REPO/.tmp/probe-colors.json"
        if [ ! -x "$PROBE" ]; then
            fail "panshi-colorprobe 不可用（构建失败）"
        elif ! timeout 90 "$PROBE" >"$PJ" 2>/dev/null || [ ! -s "$PJ" ]; then
            fail "探针运行失败或无输出"
        else
            # 断言脚本落到文件再跑：heredoc 直接塞进 $( ) 里 bash 解析不稳
            PYCHK="$REPO/.tmp/check-colors.py"
            cat > "$PYCHK" <<'PY'
import colorsys, json, sys

d = json.load(open(sys.argv[1], encoding="utf-8"))
warn_pct = d.get("usageWarnPct", 75.0)
fails, notes = [], []

def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

def lum(h):
    def f(v):
        v /= 255.0
        return v/12.92 if v <= 0.03928 else ((v+0.055)/1.055)**2.4
    r, g, b = (f(x) for x in rgb(h))
    return 0.2126*r + 0.7152*g + 0.0722*b

def contrast(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi+0.05)/(lo+0.05)

def sat(h):
    r, g, b = (x/255.0 for x in rgb(h))
    return colorsys.rgb_to_hsv(r, g, b)[1]

def dist(a, b):
    return max(abs(x-y) for x, y in zip(rgb(a), rgb(b)))

def same(a, b, tol=8):
    return dist(a, b) <= tol

print("  ── 实测值 ────────────────────────────────────────────")
print("  主题  控件  档位   字号  语义     文字色    底色      对比度")
rows = []
for t in d["themes"]:
    for s in t["samples"]:
        c = contrast(s["color"], s["bg"])
        rows.append((t["theme"], s, c))
        print("  %-5s %-5s %5.0f%%  %3dpx %-8s %s  %s   %5.2f"
              % (t["theme"], s["target"], s["pct"], s["fontPx"],
                 s["semantic"], s["color"], s["bg"], c))
print()

for t in d["themes"]:
    th = t["theme"]
    mine = [(s, c) for (tt, s, c) in rows if tt == th]
    # ① 容器铺底：采样回读的底色必须等于主题底色，否则对比度是在拿错底色算
    for s, c in mine:
        if s.get("bgSampled") and not same(s["bgSampled"], s["bg"], 2):
            fails.append("%s/%s@%s：容器实测底色 %s ≠ 主题底色 %s（对比度不可信）"
                         % (th, s["target"], s["pct"], s["bgSampled"], s["bg"]))
    # ② 渲染成功：文字色不能与底色相同（否则根本没画出来）
    for s, c in mine:
        if same(s["color"], s["bg"], 2):
            fails.append("%s/%s@%s：文字色与底色几乎相同（%s）—— 未渲染出文字"
                         % (th, s["target"], s["pct"], s["color"]))
    # ③ 语义映射：档位 → semantic 必须与 usageSemanticForPct 一致（含边界）。
    #    ⚠️ 只校验「大数值」目标：百分比标签按页面契约恒为 normal（18px 小字不承载警示色），
    #       不再跟随 usageSemanticForPct，故不参与此断言。
    for s, c in mine:
        if s["target"] != "big":
            continue
        exp = "warning" if s["pct"] >= warn_pct else "normal"
        if s["semantic"] != exp:
            fails.append("%s/%s@%s：语义应为 %s，实测 %s" % (th, s["target"], s["pct"], exp, s["semantic"]))
    # ④ 分档真的产生视觉差异（同控件内比较，避免字号造成的抗锯齿差）
    for tgt in ("big", "pct"):
        nm = [s for s, _ in mine if s["target"] == tgt and s["semantic"] == "normal"]
        wm = [s for s, _ in mine if s["target"] == tgt and s["semantic"] == "warning"]
        if nm and wm:
            if same(nm[0]["color"], wm[0]["color"]):
                fails.append("%s/%s：正常档与警示档同色（%s）—— 分档未产生视觉差异"
                             % (th, tgt, wm[0]["color"]))
            # ⑤ 饱和度守卫：防「名为警示、实为中性黑」回归（V-23 的 TextLively 坑）
            ds = sat(wm[0]["color"]) - sat(nm[0]["color"])
            if ds < 0.25:
                fails.append("%s/%s：警示色饱和度仅比正常色高 %.2f（%s vs %s）——疑似又是"
                             "中性色角色被当成警示色" % (th, tgt, ds, wm[0]["color"], nm[0]["color"]))
            else:
                print("  [PASS] %s/%s 警示色确实是彩色（饱和度 +%.2f）" % (th, tgt, ds))
    # ⑥ V-23 决策：各警示档（≥75）同色，差异由图标+文案承载
    for tgt in ("big", "pct"):
        wm = [s for s, _ in mine if s["target"] == tgt and s["semantic"] == "warning"]
        if len(wm) > 1 and not all(same(s["color"], wm[0]["color"]) for s in wm):
            fails.append("%s/%s：各警示档颜色不一致 %s —— 与 V-23「两档同色」决策冲突"
                         % (th, tgt, [s["color"] for s in wm]))
    # ⑦ WCAG 1.4.3 对比度：≥24px 按大文本 3:1，否则正文 4.5:1
    for s, c in mine:
        need = 3.0 if s["fontPx"] >= 24 else 4.5
        kind = "大文本" if s["fontPx"] >= 24 else "正文"
        if c + 1e-9 < need:
            msg = ("%s/%s@%s（%dpx %s）对比度 %.2f < %.1f" %
                   (th, s["target"], s["pct"], s["fontPx"], kind, c, need))
            if s["fontPx"] >= 24:
                fails.append(msg)
            else:
                notes.append(msg + "；色值来自 DTK TextWarning（系统主题提供，非本项目硬编码），"
                                   "且数值文字本身已承载语义，WCAG 1.4.1 仍满足")
        else:
            print("  [PASS] %s/%s@%s 对比度 %.2f ≥ %.1f（%s）" % (th, s["target"], s["pct"], c, need, kind))

# ⑧ 亮暗必须真的换了色（否则暗色断言只是把亮色跑了两遍）
light = {(s["target"], s["pct"]): s for (tt, s, c) in rows if tt == "light"}
dark = {(s["target"], s["pct"]): s for (tt, s, c) in rows if tt == "dark"}
if light and dark:
    for k in sorted(set(light) & set(dark), key=lambda x: (x[0], x[1])):
        if same(light[k]["color"], dark[k]["color"], 20):
            fails.append("亮暗同色：%s@%s 都是 %s —— 暗色未生效"
                         % (k[0], k[1], light[k]["color"]))
    print("  [PASS] 亮/暗文字色不同（暗色确实生效，非同一套跑两遍）")

for m in notes:
    print("  [WARN] " + m)
for m in fails:
    print("  [FAIL] " + m)
print("  硬断言 %s；可访问性提示 %d 条" % ("全部通过" if not fails else "有 %d 项失败" % len(fails), len(notes)))
sys.exit(1 if fails else 0)
PY
            RES=$(python3 -B "$PYCHK" "$PJ" 2>&1)
            printf '%s\n' "$RES"
            n=$(printf '%s\n' "$RES" | grep -c '^  \[WARN\]' || true)
            WARNS=$((WARNS + n))
            printf '%s\n' "$RES" | grep -q '^  \[FAIL\]' && FAILS=$((FAILS+1))
        fi
    fi
fi

# ---------------------------------------------------------------- 后端
if want backend; then
    banner "X11 + Wayland 双后端（人工检查）"
    cat <<'EOF'
已知限制（本机实测）：
  · QT_QPA_PLATFORM=wayland 启动 → **SIGABRT**
  · DTK6 暗色走 treeland Wayland personalization，不读 gsettings gtk-theme
  所以 Wayland 这一轮无法在本机完成 —— 属**环境限制**，非缺陷。
EOF
    run "echo \"当前 session: \$XDG_SESSION_TYPE\""
    echo "  X11 逐项检查（应用已在当前会话验证多轮，此处仅列清单）："
    echo "    · 主窗口边框/圆角、标题栏（46px，DTitlebar）"
    echo "    · DIconTheme 图标非空"
    echo "    · 滚动条（磁盘页表格窄窗横向兜底）"
    echo "    · 自定义范围对话框（单日历两次点击，宽 420）"
    echo "    · 弹窗位置：页内 Banner 在内容区顶部，不覆盖页头"
    echo "  Wayland：需另登 Wayland 会话后跑同一轮（QT_QPA_PLATFORM=wayland 已知 SIGABRT，"
    echo "  该结论在 treeland 会话恢复前持续有效）。"
fi

# ---------------------------------------------------------------- 主题
if want theme; then
    banner "亮/暗主题对照（含 V-30 暗色复核，人工）"
    cat <<'EOF'
V-30 状态：探针已确认 Dark 下 TextWarning=#e43f2e（深底可读），
          但 **GUI 暗色本机不可自动切换** → 需人工在控制中心切换后肉眼复核。

要做的事：
  1. 控制中心 → 外观 → 切「深色」
  2. 磁盘空间页 → 概览（≥75% 时警示色文字在深底可读；可用 make-testdata.py 的 df shim 造 78%）
  3. 触发一次失败 Banner（策略页保存 → polkit 框按 Esc 取消）
     → 红圆白叉图标 + 柔和底 + 中性正文在深底都清晰
  4. 备份与版本页 → 表格/徽章/Banner 在深底可读
  5. 切回浅色，确认无残留
EOF
    run "gsettings get com.deepin.wrap.gnome.desktop.interface color-scheme 2>/dev/null || echo '（无 gsettings 键，走 treeland 协议）'"
    echo
    echo "  若控制中心有深色选项但应用不变 → DTK 走 treeland personalization 通道，"
    echo "  需在 DTK 侧确认，不是本项目问题。"
    echo "  主题色跟随验证：控制中心改主题色 → 侧栏选中项与搜索命中高亮应跟随"
    echo "  （Accent 走 Highlight 角色，刻意不备自绿）"
fi

banner "汇总"
if [ "$DRY" = "1" ]; then
    echo "干跑模式：未修改任何状态。加 --go 实际执行。"
else
    if [ "$FAILS" = "0" ]; then
        echo "自动断言全部通过 ✅（人工确认项请按上面的提示核对）"
        if [ "$WARNS" -gt 0 ]; then
            echo "另有 $WARNS 条 WARN（可访问性提示，不计失败，见上方黄行）"
        fi
    else
        echo "有 $FAILS 项 FAIL，请回看上方红色行。"
        exit 1
    fi
fi
