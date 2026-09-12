"""关机前盘点: 列出 VM 上持久区(/home/azzr) 与临时区(/tmp) 的内容与体积,
判断有没有「只存在于远端、本地无副本」的东西。只读, 不删任何文件。"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run  # noqa: E402

CMDS = [
    ("持久区 /home/azzr 顶层(关机不丢)",
     "du -sh /home/azzr/* 2>/dev/null | sort -rh | head -25"),
    ("临时区 /tmp 顶层(关机清空)",
     "du -sh /tmp/* 2>/dev/null | sort -rh | head -20"),
    ("divbench 结构",
     "ls -la /home/azzr/divbench/ 2>/dev/null | head -25"),
    ("divbench/logs 内容",
     "ls -la /home/azzr/divbench/logs/ 2>/dev/null | tail -20; "
     "echo '(count)'; ls /home/azzr/divbench/logs/ 2>/dev/null | wc -l"),
    ("非源码/非用例的产物 (json/csv/ckpt)",
     "find /home/azzr -maxdepth 3 \\( -name '*.json' -o -name '*.csv' "
     "-o -name '*.ckpt*' -o -name '*.bin' \\) -printf '%s\\t%p\\n' "
     "2>/dev/null | sort -rn | head -20"),
    ("最近 24h 修改过的非临时文件",
     "find /home/azzr -maxdepth 3 -type f -mmin -1440 "
     "-printf '%TY-%Tm-%Td %TH:%TM  %8s  %p\\n' 2>/dev/null | sort | tail -25"),
    ("跑着的进程",
     "ps -eo pid,etime,pcpu,comm --sort=-pcpu | head -12"),
    ("磁盘 / tmpfs",
     "df -h / /tmp 2>/dev/null"),
]

for title, cmd in CMDS:
    print("=" * 68)
    print("## " + title)
    print("=" * 68)
    rc, out, err = run(cmd)
    print(out.strip() or "(empty)")
    if err.strip():
        print("[stderr] " + err.strip()[:300])
    print()
