#!/usr/bin/env python3
# div_opt 对拍 + 测速框架
# 1. 生成测试数据
# 2. 对拍: div_opt.exe vs div_1st_baseline.exe vs Python 独立验证
# 3. 测速: div_opt vs div_1st_baseline
import random, subprocess, sys, os, time
sys.set_int_max_str_digits(1000000)  # 允许大整数验证

def random_digits(n):
    if n == 0: return ""
    s = [str(random.randint(1,9))]
    for _ in range(1, n):
        s.append(str(random.randint(0,9)))
    return "".join(s)

def gen_cases(seed, count, mode="mixed"):
    random.seed(seed)
    cases = []
    for t in range(count):
        if mode == "extreme":  # 极端 unbalanced（Core2 主战场）
            dlen = random.randint(100, 500)
            nlen = dlen * random.randint(8, 20)
        elif mode == "unbalanced":
            dlen = random.randint(500, 2000)
            nlen = dlen * random.randint(2, 4)
        elif mode == "balanced":
            dlen = random.randint(1000, 5000)
            nlen = dlen + random.randint(0, dlen // 2)
        else:  # mixed
            r = t % 4
            if r == 0:
                dlen = random.randint(96, 400); nlen = dlen * random.randint(2, 7)
            elif r == 1:
                dlen = random.randint(1, 95); nlen = dlen + random.randint(0, dlen * 2)
            elif r == 2:
                dlen = random.randint(96, 300); nlen = dlen + random.randint(0, dlen)
            else:
                dlen = 1; nlen = random.randint(1, 50)
        a = random_digits(nlen); b = random_digits(dlen)
        if b == "0": b = "1"
        cases.append((a, b))
    return cases

def write_input(cases, path):
    with open(path, "w", newline="\n") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")

def run_exe(exe, in_path, out_path):
    with open(in_path, "r") as fin, open(out_path, "w") as fout:
        t0 = time.perf_counter()
        subprocess.run([f"./{exe}"], stdin=fin, stdout=fout, check=True)
        return time.perf_counter() - t0

def verify_output(cases, out_path):
    with open(out_path, "r") as f:
        lines = f.read().split("\n")
    fail = 0
    for i, (a, b) in enumerate(cases):
        if i >= len(lines): break
        line = lines[i].strip()
        if not line: fail += 1; continue
        parts = line.split(" ")
        if len(parts) != 2: fail += 1; continue
        A, B = int(a), int(b)
        try:
            q, r = int(parts[0]), int(parts[1])
        except: fail += 1; continue
        if q * B + r != A or not (0 <= r < B): fail += 1
    return fail

def run_test(name, exe, cases):
    in_path = f"_{name}_in.txt"; out_path = f"_{name}_out.txt"
    write_input(cases, in_path)
    elapsed = run_exe(exe, in_path, out_path)
    fail = verify_output(cases, out_path)
    os.remove(in_path); os.remove(out_path)
    return elapsed, fail

# === 测试矩阵 ===
tests = [
    ("mixed",      200, 42),   # 综合（含小规模）
    ("extreme",    100, 43),   # 极端 unbalanced（Core2 主战场）
    ("unbalanced", 100, 44),   # 中等 unbalanced
    ("balanced",   100, 45),   # balanced
]

print(f"{'test':<14} {'div_opt(ms)':>12} {'baseline(ms)':>14} {'opt_fail':>9} {'base_fail':>10} {'speedup':>8}")
print("-" * 70)
for mode, count, seed in tests:
    cases = gen_cases(seed, count, mode)
    opt_t, opt_fail = run_test(f"opt_{mode}", "div_opt.exe", cases)
    base_t, base_fail = run_test(f"base_{mode}", "div_1st_baseline.exe", cases)
    speedup = base_t / opt_t if opt_t > 0 else 0
    status = "OK" if opt_fail == 0 and base_fail == 0 else f"FAIL({opt_fail}/{base_fail})"
    print(f"{mode:<14} {opt_t*1000:>12.1f} {base_t*1000:>14.1f} {opt_fail:>9} {base_fail:>10} {speedup:>7.2f}x  {status}")

print("\n注意: Windows 上 fread fallback，I/O 不是瓶颈。")
print("div_opt 用 toString()（含 string 构造），baseline 用 iostream。")
print("若 div_opt 慢于 baseline，说明 toString 开销 > iostream 优化收益。")
