#!/usr/bin/env python3
"""一次性补全 panshi-manager 的 en_US / zh_CN 未完成翻译。

背景（2026-10-21）：
- en_US.ts 有 76 条 unfinished（磁盘页重写新增 + lupdate 新收录的 QObject
  context 串），Qt 对未翻译串回退中文源串 → 英文界面残留中文。
- zh_CN.ts 21 条 unfinished，源语言即中文，恒等填充。
用法：python3 tools/fill_translations.py
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------- en_US 译文
EN = {
    # ---- DiskUsagePage ----
    ("DiskUsagePage", "告警历史"): "Alert History",
    ("DiskUsagePage", "查看全部历史告警，包括已手动关闭的"):
        "View all alert history, including manually dismissed ones",
    ("DiskUsagePage", "正常"): "Normal",
    ("DiskUsagePage", "异常"): "Abnormal",
    ("DiskUsagePage", "选中"): "Selected",
    ("DiskUsagePage", "趋势线"): "Trend",
    ("DiskUsagePage", "所选范围内的增长结论与容量预测"):
        "Growth summary and capacity forecast for the selected range",
    ("DiskUsagePage", "容量预测"): "Capacity Forecast",
    ("DiskUsagePage", "增长起点"): "Growth Start",
    ("DiskUsagePage", "磁盘分析"): "Disk Analysis",
    ("DiskUsagePage", "同一份目录占用数据的三种看法：看结构（构成）、看变化（增长）、看全量（排行）"):
        "Three views of the same directory usage data: structure (Composition), "
        "change (Growth), and the full list (Ranking)",
    ("DiskUsagePage", " 小时"): " h",
    ("DiskUsagePage", " 天"): " days",
    ("DiskUsagePage", "%1 · %2 · 增长 %3"): "%1 · %2 · +%3",
    ("DiskUsagePage", "其它分区"): "Other Partitions",
    ("DiskUsagePage", "系统盘之外的其它分区（按设备去重，容量/已用取自 df）；点击行右侧按钮可查看该分区内部构成"):
        "Partitions other than the system disk (deduplicated by device; "
        "capacity/used taken from df). Click the button at the right of a row "
        "to view that partition's internal composition",
    ("DiskUsagePage", "未检测到其它分区（单分区布局）"):
        "No other partitions detected (single-partition layout)",
    ("DiskUsagePage", "%1 · %2 分区\n容量 %3 · 已用 %4"):
        "%1 · %2 partition\nCapacity %3 · Used %4",
    ("DiskUsagePage", "查看 %1 内部构成"): "View the internal composition of %1",
    ("DiskUsagePage", "每 %1 分钟采样一次 · 历史保留 %2 天"):
        "Sampled once every %1 minutes · history kept for %2 days",
    ("DiskUsagePage", "开启后自动建立基线并持续监控"):
        "Once enabled, a baseline is established automatically and monitoring continues",
    ("DiskUsagePage", " · 最近记录 %1 · 该日增长 %2"):
        " · Last record %1 · growth that day %2",
    ("DiskUsagePage", "时间线恢复为系统盘占用，列表回到顶层目录"):
        "The timeline returns to system disk usage and the list goes back to the top-level directory",
    ("DiskUsagePage", "回到上一层目录"): "Go up one directory level",
    ("DiskUsagePage", "构成"): "Composition",
    ("DiskUsagePage", "增长"): "Growth",
    ("DiskUsagePage", "排行"): "Ranking",
    ("DiskUsagePage", "磁盘分析表"): "Disk analysis table",
    ("DiskUsagePage", "构成：系统盘顶层目录的占用结构（前 5 项 + 其他），占比 = 占系统盘已用"):
        "Composition: usage structure of the top-level system disk directories "
        "(top 5 + others); percentage = share of used system disk space",
    ("DiskUsagePage", "增长：所选日期相对前一日有增长的目录/文件（最多 8 项），占比 = 该目录增量占当日总增量"):
        "Growth: directories/files that grew on the selected date versus the "
        "previous day (up to 8 items); percentage = that directory's increase "
        "as a share of the day's total increase",
    ("DiskUsagePage", "排行：当前目录下所有子项从大到小；单击行让上方时间线跟随，双击进入子目录"):
        "Ranking: all sub-items under the current directory, largest first; "
        "click a row to make the timeline above follow it, double-click to enter a subdirectory",
    ("DiskUsagePage", "点击时间线上任意数据点，查看该日的增长来源"):
        "Click any data point on the timeline to see that day's growth sources",
    ("DiskUsagePage", "正在读取子目录…"): "Reading subdirectory…",
    ("DiskUsagePage", "系统盘"): "System Disk",
    ("DiskUsagePage", "返回系统盘时间线与顶层目录"):
        "Return to the system disk timeline and top-level directory",
    ("DiskUsagePage", "回到 %1"): "Back to %1",
    ("DiskUsagePage", "告警历史（%1 条新增）"): "Alert History (%1 new)",
    ("DiskUsagePage", "告警历史（%1）"): "Alert History (%1)",
    ("DiskUsagePage", "告警已关闭，可随时在「告警历史」中查看"):
        'Alert dismissed; you can review it anytime in "Alert History"',
    ("DiskUsagePage", "历史告警"): "Alert History",
    ("DiskUsagePage", "暂无告警记录"): "No alert records",
    ("DiskUsagePage", "按时间倒序；手动关闭过的告警仍保留在此"):
        "In reverse chronological order; manually dismissed alerts are still kept here",
    ("DiskUsagePage", "清空历史"): "Clear History",
    ("DiskUsagePage", "清空告警历史"): "Clear alert history",
    ("DiskUsagePage", "将删除全部告警记录，且不可恢复。确定继续？"):
        "This will delete all alert records and cannot be undone. Continue?",
    ("DiskUsagePage", "清空"): "Clear",
    ("DiskUsagePage", "%1 ~ %2 · %3 天 · %4 条记录"):
        "%1 ~ %2 · %3 days · %4 records",
    ("DiskUsagePage", "区间起始 %1 → 结束 %2"): "Range start %1 → end %2",
    ("DiskUsagePage", "所选范围内暂无监控数据"):
        "No monitoring data in the selected range",
    ("DiskUsagePage", "约 %1 天后写满"): "Will be full in about %1 days",
    ("DiskUsagePage", "按当前增速 · 预计 %1"):
        "At the current growth rate · estimated %1",
    ("DiskUsagePage", "暂无风险"): "No risk",
    ("DiskUsagePage", "增长已趋稳，短期不会写满"):
        "Growth has stabilized; it will not fill up in the short term",
    ("DiskUsagePage", "暂无数据，无法预测"): "No data; unable to forecast",
    # ---- OverviewPage ----
    ("OverviewPage", "可用空间"): "Available Space",
    # 卡片标签已是「系统快照 / System Snapshots」，值只留计数本身
    ("OverviewPage", "%1 个"): "%1",
    ("OverviewPage", "刷新状态"): "Refresh Status",
    # ---- ProtectionPage ----
    ("ProtectionPage",
     "磐石系统将核心目录(/usr 等)以只读方式挂载, 防止误操作或恶意软件破坏系统。\n"
     "开启“可写层”将解除只读保护(适合开发调试), 关闭后恢复保护。"):
        "The Panshi system mounts core directories (such as /usr) read-only to "
        "prevent accidental operations or malware from damaging the system.\n"
        'Enabling the "writable layer" removes the read-only protection '
        "(suitable for development and debugging); turning it off restores the protection.",
    # ---- QObject（自由函数/lambda 的 QObject::tr；多数来自探针，无害但一并补全）----
    ("QObject", "条记录"): " records",
    ("QObject", "增长"): "Growth",
    ("QObject", "排行"): "Ranking",
    ("QObject", "磁盘分析表"): "Disk analysis table",
    ("QObject", "较前一日变化"): "Change vs. previous day",
    ("QObject", "前一日暂无数据"): "No data for the previous day",
    ("QObject", "净增长"): "Net growth",
    ("QObject", "容量预测"): "Capacity Forecast",
    ("QObject", "预计"): "Estimated",
    ("QObject", "系统盘"): "System Disk",
    ("QObject", "构成"): "Composition",
    ("QObject", "返回系统盘"): "Back to system disk",
    ("QObject", "返回上级"): "Back",
    ("QObject", "增长："): "Growth:",
    ("QObject", "检查更新"): "Check for updates",
    ("QObject", "有可用更新"): "Updates available",
    ("QObject", "已是最新"): "Up to date",
    ("QObject", "待更新软件包"): "Packages to update",
}


def unescape(s: str) -> str:
    return (s.replace("&lt;", "<").replace("&gt;", ">")
             .replace("&quot;", '"').replace("&apos;", "'")
             .replace("&amp;", "&"))


def escape(s: str) -> str:
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def fill(path: Path, table: dict, identity: bool = False) -> int:
    text = path.read_text(encoding="utf-8")
    stats = {"filled": 0, "missing": []}

    ctx_re = re.compile(r"(<context>\s*<name>)(.*?)(</name>.*?)(</context>)",
                        re.DOTALL)
    msg_re = re.compile(
        r"(<message>\s*)(.*?)(\s*</message>)", re.DOTALL)

    def fix_context(m: "re.Match") -> str:
        ctx_name = m.group(2).strip()
        body = m.group(3)

        def fix_msg(mm: "re.Match") -> str:
            block = mm.group(2)
            tr_m = re.search(
                r"<translation\s+type=\"unfinished\"\s*/?>(?:([^<]*))?</translation>"
                r"|<translation\s+type=\"unfinished\"\s*>([^<]*)</translation>",
                block)
            if not tr_m:
                return mm.group(0)
            src_m = re.search(r"<source>(.*?)</source>", block, re.DOTALL)
            if not src_m:
                return mm.group(0)
            src = unescape(src_m.group(1))
            if identity:
                new_tr = f"<translation>{escape(src)}</translation>"
            else:
                key = (ctx_name, src)
                if key not in table:
                    stats["missing"].append(key)
                    return mm.group(0)
                new_tr = f"<translation>{escape(table[key])}</translation>"
            stats["filled"] += 1
            new_block = block[:tr_m.start()] + new_tr + block[tr_m.end():]
            return mm.group(1) + new_block + mm.group(3)

        new_body = msg_re.sub(fix_msg, body)
        return m.group(1) + m.group(2) + new_body + m.group(4)

    text = ctx_re.sub(fix_context, text)
    path.write_text(text, encoding="utf-8")
    return stats


def main() -> int:
    en_path = ROOT / "translations" / "panshi-manager_en_US.ts"
    zh_path = ROOT / "translations" / "panshi-manager_zh_CN.ts"

    st = fill(en_path, EN)
    print(f"en_US: 填充 {st['filled']} 条")
    for k in st["missing"]:
        print(f"  ★ 缺译文: [{k[0]}] [{k[1][:50]}]")

    st2 = fill(zh_path, {}, identity=True)
    print(f"zh_CN: 恒等填充 {st2['filled']} 条")

    # 复核：两个文件都不应再有 unfinished
    for p in (en_path, zh_path):
        root = ET.parse(p).getroot()
        left = sum(1 for msg in root.iter("message")
                   if (t := msg.find("translation")) is not None
                   and t.get("type") == "unfinished")
        print(f"复核 {p.name}: 剩余 unfinished = {left}")
        if left:
            return 1
    return 0 if not st["missing"] else 2


if __name__ == "__main__":
    sys.exit(main())
