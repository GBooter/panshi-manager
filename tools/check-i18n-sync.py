# -*- coding: utf-8 -*-
"""
离线 i18n 一致性检查（本机无 lupdate 时的替代品）。

做两件事：
  1. 括号平衡检查（跳过字符串字面量与注释），用于在没有编译器时
     先排掉漏括号这类低级语法错误；
  2. 源码 tr("…") / QObject::tr("…") 的源串是否已全部登记进 .ts
     —— 新增文案漏同步是静默缺陷：编译得过、界面却是中文（英文界面下）。

用法：python3 tools/check-i18n-sync.py [proj-root] [cpp相对路径...]
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

DEFAULT_CS = [
    "src/pages/diskusagepage.cpp",
]


def brace_balance(src):
    """返回 (错误列表, 未闭合列表)。空 = 平衡。"""
    i, n = 0, len(src)
    stack, bad, line = [], [], 1
    pairs_open = "([{"
    pairs_close = ")]}"
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            i += 2
            while i + 1 < n and not (src[i] == "*" and src[i + 1] == "/"):
                if src[i] == "\n":
                    line += 1
                i += 1
            i += 2
            continue
        if c in ("\"", "'"):
            q = c
            i += 1
            while i < n and src[i] != q:
                if src[i] == "\\":
                    i += 1
                if src[i] == "\n":
                    line += 1
                i += 1
            i += 1
            continue
        if c in pairs_open:
            stack.append((c, line))
        elif c in pairs_close:
            if not stack:
                bad.append(("多余的右括号", c, line))
            else:
                o, ol = stack.pop()
                if pairs_open.index(o) != pairs_close.index(c):
                    bad.append(("括号不匹配", o + c, ol))
        i += 1
    return bad, stack


STR_RE = r'((?:[^"\\]|\\.)*)'

# 相邻字符串字面量拼接（C++ 会把 "a" "b" 合成 "ab"，lupdate 记录的也是合并后的串）。
# 不先合并的话，所有多行 tr() 都会被误报成"未登记"。
_ADJACENT = re.compile(r'"((?:[^"\\]|\\.)*)"[ \t]*\n?[ \t]*"((?:[^"\\]|\\.)*)"')


_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}


def unescape(s):
    """展开 C 风格转义，使其与 lupdate 写入 .ts 的形态一致。"""
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append(_ESCAPES.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def merge_adjacent_strings(src):
    prev = None
    while prev != src:
        prev = src
        src = _ADJACENT.sub(lambda m: '"%s%s"' % (m.group(1), m.group(2)), src)
    return src


def collect_tr(src, cls):
    """按上下文收集 tr() 源串：普通 tr() 归 cls，QObject::tr() 归 QObject。"""
    src = merge_adjacent_strings(src)
    qobj = set(re.findall(r"QObject::tr\(\s*\"" + STR_RE + r"\"", src))
    own = set(re.findall(r"(?<!::)\btr\(\s*\"" + STR_RE + r"\"", src)) - qobj
    # lupdate 写入 .ts 的是**转义已展开**的真实字符（源码的 "\n" 在 .ts 里是换行），
    # 比较前必须同样展开，否则含 \n 的串会被误报成未登记。
    return {cls: {unescape(s) for s in own}, "QObject": {unescape(s) for s in qobj}}


def ctx_sources(tsfile, name):
    root = ET.parse(tsfile).getroot()
    for c in root.iter("context"):
        if c.find("name").text == name:
            return {m.find("source").text for m in c.iter("message")}
    return set()


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    cpp_files = sys.argv[2:] or DEFAULT_CS
    ts = [os.path.join(root, "translations", "panshi-manager_zh_CN.ts"),
          os.path.join(root, "translations", "panshi-manager_en_US.ts")]
    fail = 0
    for rel in cpp_files:
        p = os.path.join(root, rel)
        src = open(p, encoding="utf-8").read()
        cls = os.path.splitext(os.path.basename(rel))[0].replace("page", "Page").capitalize()
        print("== %s" % rel)
        bad, unclosed = brace_balance(src)
        if bad or unclosed:
            fail = 1
            print("   括号检查 FAIL:", bad[:5], "未闭合:", unclosed[:5])
        else:
            print("   括号平衡 OK")
        groups = collect_tr(src, "DiskUsagePage" if "diskusage" in rel else cls)
        for tf in ts:
            miss = []
            for ctx, strs in sorted(groups.items()):
                have = ctx_sources(tf, ctx)
                miss += [(ctx, s) for s in sorted(strs) if s not in have]
            if miss:
                fail = 1
                print("   %s 缺失 %d 条:" % (os.path.basename(tf), len(miss)))
                for ctx, s in miss[:10]:
                    print("      [%s] %s" % (ctx, s))
            else:
                print("   %s 全部已登记 OK" % os.path.basename(tf))
    print("RESULT:", "FAIL" if fail else "PASS")
    return fail


if __name__ == "__main__":
    sys.exit(main())
