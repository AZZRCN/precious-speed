#!/usr/bin/env python3
"""callgrind 指令级归因: 定位瓶颈用例的耗时函数 (用户赞同的精确计量)。

用法: python3 cg_div.py <bin> [case_name]
对指定官方用例跑 callgrind, 用 callgrind_annotate 取 Top 函数 (按 total instructions)。
"""
import subprocess, os, sys, shutil

BIN = sys.argv[1] if len(sys.argv) > 1 else "bin/div_D4"
CASE = sys.argv[2] if len(sys.argv) > 2 else "length_ratio_integer_02"
INDIR = os.path.expanduser("~/lcp/big_integer/division_of_big_integers/in")
cf = os.path.join(INDIR, CASE + ".in")

if not shutil.which("valgrind"):
    print("valgrind NOT installed; fall back: apt-get install valgrind")
    sys.exit(2)

print(f"=== callgrind {BIN} on {CASE} ===", flush=True)
r = subprocess.run(
    f"valgrind --tool=callgrind --callgrind-out-file=cg.out --instr-atstart=yes "
    f"{BIN} < {cf} > /dev/null 2>cg.err",
    shell=True, capture_output=True, text=True, timeout=900)
print("valgrind rc=", r.returncode, flush=True)
if r.stderr.strip():
    print("VG STDERR tail:", r.stderr.strip()[-500:], flush=True)

a = subprocess.run("callgrind_annotate --auto=yes --threshold=90 cg.out 2>/dev/null | head -55",
                   shell=True, capture_output=True, text=True, timeout=120)
print("=== TOP FUNCTIONS (total instructions) ===", flush=True)
print(a.stdout)
