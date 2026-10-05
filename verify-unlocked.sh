#!/bin/bash
# 磐石设置 — 权限解锁验证脚本
#
# 背景：这些项此前标记为「环境不可达」，因为本机无免密 sudo。
# 2026-10-03 用户提供了 sudo 密码 → B3 / C4 / C7 变为可执行。
#
# 现状（2026-10-03 复测）：
#   B3 系统级安装     ✅ 已通过（并揪出 2 个打包缺陷，已由用户代修）
#   C7 sudo 阻塞 4s   ✅ 已通过（SIGSTOP 冻结 ~90ms 的 ostree）
#   C4 还原冲突跳转   ⚠️ 前置通过；破坏性 5 步**只输出步骤，不代执行**
#
# ⚠️ 安全性：
#   · 默认 DRY-RUN（只打印将要执行什么），需显式 --go 才实际动手
#   · 破坏性步骤（C4 的开可写层 + 两次重启）**永不自动执行**
#   · 密码从环境变量 SUDO_PASS 取，不写死在命令历史里
#
# ⚠️ 磐石系统事实（实测，勿再假定 /usr）：
#   /usr 是 usr-overlay **只读**挂载，装不进去也不该 remount；
#   /usr/local 才是独立 rw 的 ext4，且在 PATH / XDG_DATA_DIRS 内。
#   → 正确做法就是默认的 `sudo cmake --install build`（不加 --prefix）。
#   本脚本从 install_manifest.txt 读实际前缀，不猜。
#
# 用法：
#   ./verify-unlocked.sh            # 干跑（默认）
#   ./verify-unlocked.sh --go B3    # 实际执行 B3
#   ./verify-unlocked.sh C7         # 看 C7 方法（已通过，含被否决的方案）
#   ./verify-unlocked.sh C4         # 看 C4 手工步骤（破坏性，不自动执行）
set -u

REPO="/media/Another/Date/AI-Date/WorkBuddy-Date/磐石设置/磐石设置"
# 密码**不写死**在脚本里（脚本会进版本库/被复制 → 明文密码会扩散）。
# 用法：SUDO_PASS='你的密码' ./verify-unlocked.sh --go B3
# 或：  read -rs SUDO_PASS && SUDO_PASS="$SUDO_PASS" ./verify-unlocked.sh --go B3
PASS="${SUDO_PASS:-}"
DRY=1
ONLY=""

for a in "$@"; do
    case "$a" in
        --go)  DRY=0 ;;
        B3|C4|C7) ONLY="$a" ;;
        *) echo "未知参数: $a"; exit 2 ;;
    esac
done

run() {
    if [ "$DRY" = "1" ]; then
        printf '  [DRY-RUN] %s\n' "$*"
    else
        printf '  [执行] %s\n' "$*"
        eval "$@"
    fi
}

# sudo 包装：干跑时不真正调用（避免无谓的密码提示）
# ⚠️ 不用密码写死文件；缺密码时**明确报错**而非静默送空串
#    （`printf '\n' | sudo -S` 会让 sudo 读到一个空密码并失败，
#      错误信息还被上面的 [执行] 行掩盖，看起来像"密码不对"）
sudorun() {
    if [ "$DRY" = "1" ]; then
        printf '  [DRY-RUN] sudo %s\n' "$*"
        return 0
    fi
    if [ -z "$PASS" ]; then
        echo "  ❌ 需要 sudo 密码但 SUDO_PASS 为空。" >&2
        echo "     用法：SUDO_PASS='你的密码' $0 --go ${ONLY:-}" >&2
        echo "     或：  read -rs SUDO_PASS && SUDO_PASS=\"\$SUDO_PASS\" $0 --go ${ONLY:-}" >&2
        return 1
    fi
    printf '  [执行] sudo %s\n' "$*"
    printf '%s\n' "$PASS" | sudo -S -p '' "$@"
}

banner() { printf '\n========== %s ==========\n' "$1"; }

# ---------------------------------------------------------------- 探测
banner "环境探测"
run "test -d $REPO && echo '仓库存在' || echo '仓库缺失'"
run "test -x $REPO/build/panshi-manager && echo '已构建' || echo '未构建（需先 cmake --build build）'"
run "test -d /ostree/repo && echo 'OSTree 仓库存在（是磐石系统）' || echo '非磐石系统'"
run "which dconf >/dev/null 2>&1 && echo 'dconf 可用' || echo 'dconf 不可用'"

# ---------------------------------------------------------------- B3
if [ -z "$ONLY" ] || [ "$ONLY" = "B3" ]; then
banner "B3：系统级安装（此前无免密 sudo 不可达）"
cat <<'EOF'
影响：按 CMAKE_INSTALL_PREFIX（默认 /usr/local）安装
      二进制 / 图标 / desktop / DConfig schema / systemd 单元 / .qm。
回滚：无（纯安装，可重复执行；卸载需手删对应路径）。

⚠️ 磐石系统事实（2026-10-03 实测纠正）：
   /usr 是 usr-overlay **只读**挂载 —— 装不进去，也不该 remount。
   /usr/local 才是独立 rw 的 ext4，且在 PATH / XDG_DATA_DIRS 内。
   → 正确做法就是**默认的 `sudo cmake --install build`（不加 --prefix）**。
   下面是「探测实际前缀」，不是假定 /usr。
EOF
sudorun "cmake --install $REPO/build"
# 实际安装前缀：从 install_manifest.txt 第一行（已安装文件绝对路径）反推。
# 注意 manifest 是**文件路径清单**，不含 CMAKE_INSTALL_PREFIX 变量
# （变量在 CMakeCache.txt；但安装核验应只依赖产物清单，源目录可能已清理）。
# 形如 /usr/local/bin/panshi-manager → 剥掉末尾 /bin/panshi-manager。
PREFIX_DIR=$(sed -n '1s|/bin/panshi-manager$||p' "$REPO/build/install_manifest.txt" 2>/dev/null | head -1)
# fallback 必须**显式告警**（2026-10-03 教训）：
#   我第一版用 `sed 's|^CMAKE_INSTALL_PREFIX:PATH=||p'` 读 manifest，
#   但 manifest 是**文件路径清单、不含该变量** → sed 永远落空，
#   而 fallback `/usr/local` 在本机恰好正确 → **静默掩盖了一个真 bug**：
#   装到 /usr 的形态会检测错前缀。
#   「探测失败」与「探测到别的值」必须能区分，不能都走同一条静默默认。
if [ -z "$PREFIX_DIR" ]; then
    echo "  ⚠️ 未能从 install_manifest.txt 推断前缀（文件缺失或格式变化）。" >&2
    echo "     回落为 /usr/local —— 若本次实际装到了别处，下面的检查路径会全错。" >&2
    PREFIX_DIR=/usr/local
fi
echo "  实际安装前缀：$PREFIX_DIR"
# 下面的检查在干跑时也要能给出**建议路径**（否则干跑无信息）
CHECK_DIR="$PREFIX_DIR"
if [ "$DRY" = "1" ] && [ ! -d "$CHECK_DIR" ]; then
    echo "  （干跑：$CHECK_DIR 尚不存在，以下为安装后应检查的位置）"
fi
run "ls -l $CHECK_DIR/bin/panshi-manager"
# .qm 必须真被安装 —— 这是 B3 揪出的真缺陷（RESOURCE_PREFIX 不生成 install 规则）
run "ls -l $CHECK_DIR/share/panshi-manager/translations/ 2>/dev/null || echo '❌ 未找到 .qm 目录：英文会静默回落中文'"
# desktop 的 Exec 应指向实际 bindir（configure_file + @PANSHI_BINDIR@ 生成）
run "grep '^Exec=' $CHECK_DIR/share/applications/org.deepin.panshi-manager.desktop 2>/dev/null"
echo
echo "验证点：LANGUAGE=en_US.UTF-8 启动 \$PREFIX/bin/panshi-manager"
echo "  → 标题栏应为 Panshi Manager；备份与版本页全英文"
echo "  ⚠️ 三条静默失效陷阱："
echo "     1) .qm 没装到文件系统（RESOURCE_PREFIX 只进内嵌资源，DTK 不读）"
echo "     2) LANGUAGE 未设（deepin 会话默认继承 zh_CN，优先级压过 LANG/LC_ALL）"
echo "     3) 装的不是本项目（desktop Exec 指向别处）"
fi

# ---------------------------------------------------------------- C4
if [ -z "$ONLY" ] || [ "$ONLY" = "C4" ]; then
banner "C4：还原冲突跳转（前置条件探测）"
cat <<'EOF'
C4 的完整验证需要「可写层开启 + 重启 + 还原」，属破坏性长流程。
本脚本只做**前置状态探测与准备**，不自动执行破坏性步骤。
EOF
run "deepin-immutable-writable status -j 2>/dev/null || echo '（status -j 免 root 可读，若失败换 -j 或看 journal）'"
echo
echo "C4 手工验证步骤（每步都需确认；破坏性，脚本不代执行）："
echo "  1) 保护页 → 展开「关闭保护（高级操作）」→「临时关闭保护」"
echo "     → 此时会弹 polkit 框（sudo -n 失败后回退 pkexec，zh_CN 也应触发）"
echo "  2) 重启系统"
echo "  3) 备份与版本 → 任一备份的「还原」"
echo "     → 期望：弹「可写层冲突」确认框，按钮为「前往设置」"
echo "  4) 点「前往设置」→ 应跳到系统保护页，且「开启保护 (恢复只读)」行高亮"
echo "  5) 收尾：保护页 →「开启保护」→ 重启回原状态"
echo
echo "⚠️ 第 5 步不做的话系统会一直处于可写层状态（核心目录可写）。"
echo "⚠️ 步骤 1 与 5 各触发一次真实提权（会弹 polkit）。"
fi

# ---------------------------------------------------------------- C7
if [ -z "$ONLY" ] || [ "$ONLY" = "C7" ]; then
banner "C7：sudo 阻塞 4s 超时（✅ 已复测通过）"
cat <<'EOF'
目的：让某个外部命令「长时间不返回」，验证 MainWindow 的
      kArrivalTimeoutMs(4s) 超时兜底 —— 动作仍会执行，不被静默丢弃。
EOF
cat <<'EOF'

⚠️ 已否决的两种方案（2026-10-03 实测，勿再尝试）：
   ✗ SUDO_ASKPASS 指向 sleep 脚本 —— 应用走 `sudo -n`（非交互），
     -n **不调用 askpass**，0.047s 即失败，构造不出阻塞。
   ✗ LD_PRELOAD 注入 —— 被宿主沙箱 sbox.so 清洗。

✅ 有效方案：SIGSTOP 冻结子进程（纯用户态、瞬时可回滚）
EOF
cat <<'EOF'

关键技巧（缺一不可）：
  · 冻「慢的那个」：ctl 太快抓不住（~90ms 的 ostree 才合适）
  · 轮询间隔要小：50ms 级；间隔大了命令已结束
  · pgrep -n -f 取最新进程：命令会连续起多个，只冻一个会被绕过
  · HOLD 取「略大于被验超时常数」：4s 超时 → 冻 5~8s 才能看出差别
  · 收尾：kill watcher + 确认目标进程已 SIGCONT
EOF
cat <<'EOF'

参考实现（本次实测有效）：
  watch() { local PATTERN="$1" HOLD="${2:-5}"
    while :; do
      PID=$(pgrep -n -f "$PATTERN")
      if [ -n "$PID" ]; then
        kill -STOP "$PID" 2>/dev/null; sleep "$HOLD"; kill -CONT "$PID" 2>/dev/null
      fi
      sleep 0.05
    done
  }
  watch ostree 5 &          # 冻 5 秒
  # …在总览页点「立即创建备份」…
  kill %1 2>/dev/null       # 收工

判定：注入路径 ≈ 4s+ 且动作出现 → 兜底生效
     注入路径 ≈ 正常耗时（~1.8s）→ 冻结没生效（抓错进程/间隔太大）
     本次实测：+4.24s 仍加载、+4.74s 对话框出现；对照正常 ~1.8s
EOF
fi

banner "汇总"
echo "干跑模式（默认）不修改任何状态。确认无误后加 --go 实际执行。"
