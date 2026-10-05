#!/usr/bin/env python3
"""
磐石设置 — B6 造数据脚本
========================
用途：为「必须造数据才能验证」的视觉项准备可复现的测试数据。

覆盖 B6 三项：
  1. 服务徽章 Danger 态   （配置已开启但 systemd timer 非 active）
  2. 使用率 75~90 / ≥90 两档（写 df 的 shim 改容量比例）
  3. 图例「异常增长」色点 + 告警横幅（写 alert.json + 造飙升日）

⚠️ 安全性：
  · 默认干跑（--go 才写）；写前自动备份原数据到 *.bak-<时间戳>
  · 只写 ~/.local/share/panshi-manager/disk-history/ 下的文件
  · 不碰系统配置、不碰 /usr、不动快照

用法：
  ./make-testdata.py                  # 干跑：打印将要做什么
  ./make-testdata.py --go             # 实际执行
  ./make-testdata.py --go badge       # 只造服务徽章场景
  ./make-testdata.py --go usage       # 只造使用率两档
  ./make-testdata.py --go spike       # 只造异常增长 + 告警横幅
  ./make-testdata.py --clean          # 清理测试数据（还原备份）
"""
import json
import os
import shutil
import sys
import time
from datetime import date, timedelta

DRY = 1
ONLY = ""
for a in sys.argv[1:]:
    if a == "--go":
        DRY = 0
    elif a == "--clean":
        ONLY = "clean"
    elif a in ("badge", "usage", "spike"):
        ONLY = a
    elif a == "--help":
        print(__doc__)
        sys.exit(0)

# 注意：必须是 ~/.local/share/panshi-manager（无 deepin/ 组织段）。
# GUI 里 DiskHistoryStore::historyDir() 用 GenericDataLocation 显式拼 appName，
# 刻意避开 AppDataLocation（会因 setOrganizationName("deepin") 多拼一层，
# 与守护进程目录分叉 —— 2026-10-03 实测踩过）。
HIST = os.path.expanduser("~/.local/share/panshi-manager/disk-history")
BAK_SUFFIX = ".bak-%s" % time.strftime("%Y%m%d-%H%M%S")
MB = 1024 * 1024
GB = 1024 * MB

# shim 落点：⚠️ **不放 /tmp** —— 本机 /tmp 仅 10MB tmpfs 且会被安全层清理。
# 放项目内 .shim/（已列入 .gitignore）。
SHIM_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".shim")

# 构建目录自动探测（2026-10-03 起）：顺序 build2 → build。
# 旧 build/ 的 CMakeCache 指向已消失的 linglong 工具链，实际产物在 build2。
REPO = os.path.dirname(os.path.abspath(__file__))
for _c in (os.environ.get("PANSHI_BUILD"), os.path.join(REPO, "build2"),
           os.path.join(REPO, "build")):
    if _c and os.access(os.path.join(_c, "panshi-manager"), os.X_OK):
        BUILDDIR = _c
        break
else:
    BUILDDIR = os.path.join(REPO, "build2")
APP_REL = os.path.join(BUILDDIR, "panshi-manager")

def say(msg):
    print(msg)

def act(msg):
    if DRY == 1:
        say("  [DRY-RUN] %s" % msg)
    else:
        say("  [写入] %s" % msg)
        return True
    return False

def backup(path):
    if not os.path.exists(path):
        return
    dst = path + BAK_SUFFIX
    if act("备份 %s → %s" % (path, dst)):
        shutil.copy2(path, dst)
    # 干跑不复制（早期版本在 else 里也 copy，违反「干跑零副作用」约定）

def write_json(path, obj):
    backup(path)
    did = act("写 %s" % path)
    if did:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        tmp = path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(obj, f, ensure_ascii=False, indent=2)
        os.replace(tmp, path)   # 原子替换，与应用内 saveDayRecord 一致
    else:
        say("    （干跑：would write %d bytes）" % len(json.dumps(obj)))

def banner(t):
    say("\n========== %s ==========" % t)


# ---------------------------------------------------------------- 场景 1
def _qs_set_enabled(path, on):
    """在 QSettings INI 回退文件里写 [monitor] enabled=（与应用 setValue 落盘格式一致）。"""
    import configparser
    cp = configparser.ConfigParser()
    cp.optionxform = str  # 保留大小写
    if os.path.exists(path):
        cp.read(path, encoding="utf-8")
    if not cp.has_section("monitor"):
        cp.add_section("monitor")
    cp.set("monitor", "enabled", "true" if on else "false")
    if DRY == 0:
        with open(path, "w", encoding="utf-8") as f:
            cp.write(f, space_around_delimiters=False)


def scene_badge():
    """服务徽章 Danger 态：配置开启 + timer 未运行。

    代码依据（diskusagepage.cpp::applyServiceStatus）：
        state == "active"                  → Success「磁盘监控运行中」
        else if (m_config.enabled())      → Danger「磁盘监控未启动，无法自动记录」+ 显示修复按钮
        else                               → Muted「磁盘监控未开启」

    触发 Danger 只需：配置 enabled=true **且** timer 不是 active。
    配置写入两条路（与应用行为对齐：MonitorConfig::setValue 两条都写）：
      · DConfig 机：dconf /org/deepin/panshi-manager/monitor/enabled（斜杠路径，非点号）
      · 无 schema 开发机：~/.config/deepin/panshi-manager.conf 的 [monitor] enabled=true
    """
    import shutil as _sh
    import subprocess
    banner("场景 1：服务徽章 Danger 态")
    say("  原理：配置 enabled=true 且 timer 非 active → Danger + 显示「立即启动」按钮")
    say("")

    # 1) DConfig 主路径（机器没装 dconf / 无 schema 时静默跳过）
    if _sh.which("dconf"):
        if act("dconf write /org/deepin/panshi-manager/monitor/enabled true"):
            subprocess.run(["dconf", "write",
                            "/org/deepin/panshi-manager/monitor/enabled", "true"],
                           check=False)
    else:
        say("  dconf 未安装 —— 跳过（本机 DConfig 无效，应用走 QSettings 回退）")

    # 2) QSettings 回退路径（应用在任何机器上都会双写这个文件，写它无害且最可靠）
    qs = os.path.expanduser("~/.config/deepin/panshi-manager.conf")
    if act("QSettings 写入 %s [monitor] enabled=true" % qs):
        os.makedirs(os.path.dirname(qs), exist_ok=True)
        _qs_set_enabled(qs, True)

    # 3) 停 timer（用户单元，无需 root；已停则 no-op）
    if _sh.which("systemctl"):
        if act("systemctl --user stop panshi-monitor.timer（+ .service）"):
            subprocess.run(["systemctl", "--user", "stop",
                            "panshi-monitor.timer"], check=False)
            subprocess.run(["systemctl", "--user", "stop",
                            "panshi-monitor.service"], check=False)

    say("")
    say("  期望：徽章 Danger「磁盘监控未启动，无法自动记录」+ 出现「立即启动」按钮")
    say("  还原：systemctl --user start panshi-monitor.timer 或在「监控设置」里关开关；"
        "dconf reset -f /org/deepin/panshi-manager/monitor/")
    say("  ⚠️ 这个态在 V-29 已算代码层验证过；本场景用于**实机确认**徽章颜色与按钮")


# ---------------------------------------------------------------- 场景 2
def scene_usage():
    """使用率 75~90 / ≥90 两档。

    代码依据（diskusagepage.cpp::updateOverview）：
        pct = used * 100.0 / capacity
        pct >= 75.0 → TextWarning（现在两档同色，V-23 决策）
    取色来自 `df -B1 /`，故**必须 shim df** 才能造出指定比例。
    """
    banner("场景 2：使用率分档（df shim）")
    say("  原理：df shim 按目标比例返回 used/capacity，两档各测一次")
    say("")
    for pct in (78, 92, 40):
        say("  ── 目标 %d%% ──" % pct)
        shim = os.path.join(SHIM_DIR, "panshi-dfshim-%d" % pct, "df")
        did = act("写 shim %s（把 used 固定为 %d%%）" % (shim, pct))
        if did:
            os.makedirs(os.path.dirname(shim), exist_ok=True)
            cap_bytes = 100 * GB
            used_bytes = cap_bytes * pct // 100
            with open(shim, "w", encoding="utf-8") as f:
                # GUI 以 `df -B1 /` 调用：列单位是 1 字节，直接输出字节数，
                # 容量 100 GiB，界面会显示成 ~107 GB 的真实量级（早期版本
                # 输出 1K-blocks 数值，导致容量显示成 100 MB）。
                f.write("#!/bin/bash\n"
                        "# 造数用：让 df 报出 %d%% 已用（忽略参数，按 -B1 字节列输出）\n"
                        "echo \"Filesystem 1B-blocks Used Available Use%% Mounted on\"\n"
                        "echo \"overlay %d %d %d %d%% /\"\n"
                        % (pct, cap_bytes, used_bytes,
                           cap_bytes - used_bytes, pct))
            os.chmod(shim, 0o755)
        say("     运行：source .toolchain/env.sh; PATH=%s:$PATH %s"
            % (os.path.dirname(shim), APP_REL))
        say("     期望：%d%% → 「已用」大数值 + 百分比同时为 %s"
            % (pct, "TextTitle（正常）" if pct < 75 else "TextWarning（警示）"))
    say("")
    say("  ⚠️ shim 只影响 df，du/ostree 仍走真实系统 —— 这是刻意的（最小注入面）")
    say("  清理：rm -rf %s/panshi-dfshim-*" % SHIM_DIR)


# ---------------------------------------------------------------- 场景 3
def scene_spike():
    """图例「异常增长」色点 + 告警横幅。

    数据格式见 docs/开发文档.md §7.2：
      index.json  { version, first, last, storage, budget,
                    days: [ { date, f, c(容量), t(已用), g(增长), spike(飙升), top: [] } ] }
      YYYY-MM-DD.json  { date, capacity, total, complete, topLevel, drillCache, samples }
      alert.json    { lastAlert, spikes: [ { date, growth, causeDir, alerted, causes } ] }
    """
    banner("场景 3：异常增长 + 告警横幅")
    os.makedirs(HIST, exist_ok=True) if not DRY else None

    today = date.today()
    # 造 14 天：前 12 天平稳，第 13 天 +8GB（飙升），第 14 天 +1GB
    base_total = 900 * GB
    days, records = [], {}
    cur = base_total
    for i in range(14):
        d = today - timedelta(days=13 - i)
        if i == 12:
            growth = 8 * GB          # 飙升日
        else:
            # 平日抖动必须全部 < monitor.minAlertMB 默认阈值 512 MB，
            # 否则 rebuildSpikeSet() 会把平日也二次判成飙升（环标记泛滥，
            # 2026-10-03 实测 768/1280MB 抖动造出 9 个假异常日）
            growth = (i % 3) * 64 * MB + 64 * MB     # 64 / 128 / 192 MB
        cur += growth
        capacity = 1000 * GB
        # 飙升日的 topLevel 给一个明显的元凶目录，供「增长详情」下钻
        top = [{"p": "/var", "s": 3 * GB, "d": True},
               {"p": "/home", "s": 2 * GB, "d": True},
               {"p": "/usr", "s": 1500 * MB, "d": True},
               {"p": "/var/log", "s": 2500 * MB, "d": True}] if i == 12 else \
              [{"p": "/var", "s": 1200 * MB, "d": True},
               {"p": "/home", "s": 900 * MB, "d": True}]
        days.append({
            "date": d.isoformat(),
            "f": "%s.json" % d.isoformat(),
            "c": capacity,
            "t": cur,
            "g": growth,
            "spike": i == 12,       # 飙升标记（应用读字段名 "spike"，非 "sp"）
            "top": top,
        })
        records["%s.json" % d.isoformat()] = {
            "date": d.isoformat(),
            "capacity": capacity,
            "total": cur,
            "complete": True,
            "topLevel": top,
            "drillCache": {"/var": [{"p": "/var/log", "s": 900 * MB, "d": True},
                                     {"p": "/var/cache", "s": 300 * MB, "d": True}]},
            "samples": [{"t": "%sT12:00:00.000" % d.isoformat(), "total": cur, "h": {}}],
        }

    index = {
        "version": 1,
        "first": days[0]["date"],
        "last": days[-1]["date"],
        "storage": 1 * MB,
        "budget": 512 * MB,
        "days": days,
    }
    alert = {
        "lastAlert": {"/var/log": "%sT12:00:00.000" % (today - timedelta(days=1)).isoformat()},
        "spikes": [{
            "date": (today - timedelta(days=1)).isoformat(),
            "growth": 8 * GB,
            "causeDir": "/var/log",
            "alerted": True,
            "causes": [{"p": "/var/log", "s": 6 * GB, "d": True},
                       {"p": "/var/cache", "s": 2 * GB, "d": True}],
        }],
    }

    write_json(os.path.join(HIST, "index.json"), index)
    for name, rec in records.items():
        write_json(os.path.join(HIST, name), rec)
    write_json(os.path.join(HIST, "alert.json"), alert)

    say("")
    say("  期望（应用内）：")
    say("    · 时间线：第 13 天为**异常增长**（图例 Danger 色点 + 该柱高亮）")
    say("    · 概览顶部：**告警横幅**显示 Danger 态（红圆白叉图标 + 日期/目录/增长量）")
    say("    · 点该日 → 增长详情表列出 /var/log、/var/cache 等元凶")
    say("    · 图例「异常增长」色点为 Danger 色（与 V-23 后 Danger/Warning 同色，")
    say("      靠时间线上的**柱体标记**而非图例颜色区分 —— 请确认这一点可接受")
    say("")
    say("  清理：删掉 %s 下的 index.json / *.json / alert.json（备份已留 %s）"
        % (HIST, BAK_SUFFIX))


# ---------------------------------------------------------------- 清理
def do_clean():
    banner("清理测试数据")
    say("  将删除 %s 下的 index.json / YYYY-MM-DD.json / alert.json" % HIST)
    say("  ⚠️ 若这些是你**真实**的监控数据，请先手工备份！")
    if DRY == 1:
        say("  [DRY-RUN] 未执行。加 --go --clean 实际删除。")
        return
    n = 0
    if not os.path.isdir(HIST):
        say("  目录不存在，无需清理")
        return
    for name in os.listdir(HIST):
        p = os.path.join(HIST, name)
        if name == "index.json" or name == "alert.json" or \
           (len(name) == 15 and name.endswith(".json")):
            os.remove(p)
            say("  删除 %s" % name)
            n += 1
    say("  共删除 %d 个文件" % n)
    say("  如需还原备份：%s/*.bak-*" % HIST)


# ---------------------------------------------------------------- main
say("磐石设置 B6 造数据 —— 模式：%s" % ("干跑" if DRY else "实际执行"))
say("历史目录：%s" % HIST)
if ONLY == "clean":
    do_clean()
elif ONLY == "badge":
    scene_badge()
elif ONLY == "usage":
    scene_usage()
elif ONLY == "spike":
    scene_spike()
else:
    scene_badge()
    scene_usage()
    scene_spike()
if DRY == 1:
    say("")
    say("干跑模式不写任何文件。确认无误后加 --go。")
