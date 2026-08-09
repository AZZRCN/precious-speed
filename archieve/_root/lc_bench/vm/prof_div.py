#!/usr/bin/env python3
"""编译 D4 (PROFILE_DIV), 跑瓶颈用例, 打印 prof_detail.log 末尾 (大除法分解)。"""
import subprocess, os
subprocess.run("g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV -o bin/div_D4_prof src/div_D4.cpp",
               shell=True, check=True, capture_output=True, text=True)
print("compiled div_D4_prof")
for case in ["length_ratio_integer_02", "a_max_b_random_02"]:
    cf = f"~/lcp/big_integer/division_of_big_integers/in/{case}.in"
    subprocess.run("rm -f prof_detail.log", shell=True)
    r = subprocess.run(f"bin/div_D4_prof < {cf} > /dev/null 2>&1", shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(case, "RUN FAIL rc=", r.returncode, r.stderr[-500:])
        continue
    out = subprocess.run("cat prof_detail.log", shell=True, capture_output=True, text=True)
    print(f"\n===== {case} (prof_detail.log tail) =====")
    print(out.stdout[-5000:])
