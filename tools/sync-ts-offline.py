# -*- coding: utf-8 -*-
"""
离线同步 .ts（本机无 lupdate 时的等价补录）。

背景：本项目构建机（只读容器）没有 lupdate/lrelease，代码新增 tr() 后无法
走常规 `lupdate && lrelease` 链路。本脚本做等价的最小改动：
  - 删除已废弃的 source（随代码改动消失的串）
  - 在指定 context 末尾追加新 message
    （zh_CN 留空 translation = 走源语言回退；en_US 填译文）
  - 追加前先查重，重复执行不会堆积
  - 写回后用 ElementTree 复解析，确保 XML 仍合法

用法：python3 tools/sync-ts-offline.py [proj-root]
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

# (source, en_US 译文)
NEW_DISK = [
    ("监控服务%1失败：%2", "Failed to %1 the monitoring service: %2"),
    ("启动", "start"),
    ("停止", "stop"),
    ("未知错误", "unknown error"),
    ("启动失败：%1", "Failed to start: %1"),
    ("无法连接 systemd 用户会话", "Cannot connect to the systemd user session"),
    ("systemd 未识别监控单元（panshi-monitor.timer）",
     "systemd does not recognize the monitoring unit (panshi-monitor.timer)"),
    ("systemctl --user 执行失败", "systemctl --user failed"),
]
NEW_QOBJ = [
    ("systemctl --user %1 执行超时", "systemctl --user %1 timed out"),
]
# 随代码改动已不存在的串
OBSOLETE = ["启动失败：请打开“监控设置”检查，或重启系统后重试"]


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def block(src, tr):
    return ('    <message>\n'
            '        <source>%s</source>\n'
            '        <translation>%s</translation>\n'
            '    </message>\n' % (esc(src), esc(tr)))


def ctx_span(text, name):
    m = re.search(r'(<context>\s*<name>' + re.escape(name) + r'</name>\n)(.*?)(</context>\n)',
                  text, re.S)
    if not m:
        raise SystemExit('context not found: ' + name)
    return m


def msg_re(src):
    return (r'\n?[ ]*<message>(?:(?!</message>).)*?<source>' + re.escape(esc(src)) +
            r'</source>(?:(?!</message>).)*?</message>')


def process(path, with_translation):
    text = open(path, encoding='utf-8').read()
    print('== %s' % os.path.basename(path))

    # 1) 删除废弃串
    m = ctx_span(text, 'DiskUsagePage')
    body = m.group(2)
    for src in OBSOLETE:
        body, n = re.subn(msg_re(src), '', body, flags=re.S)
        print('   删除废弃串 %d 条: %s' % (n, src))
    text = text[:m.start(2)] + body + text[m.end(2):]

    # 2) 追加新串
    for ctx, items in (('DiskUsagePage', NEW_DISK), ('QObject', NEW_QOBJ)):
        m = ctx_span(text, ctx)
        body = m.group(2)
        added = 0
        for src, en in items:
            if re.search(msg_re(src), body, re.S):
                print('   已存在，跳过: %s' % src)
                continue
            body = body + block(src, en if with_translation else '')
            added += 1
        print('   新增 %d 条 -> %s' % (added, ctx))
        text = text[:m.start(2)] + body + text[m.end(2):]

    open(path, 'w', encoding='utf-8').write(text)
    ET.parse(path)          # XML 合法性回验
    print('   XML 解析 OK')


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    d = os.path.join(root, 'translations')
    process(os.path.join(d, 'panshi-manager_zh_CN.ts'), False)
    process(os.path.join(d, 'panshi-manager_en_US.ts'), True)


if __name__ == '__main__':
    main()
