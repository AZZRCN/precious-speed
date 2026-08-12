#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""local_verify.py —— 纯本地 554 红线对拍 (不依赖 VM)

    python local_verify.py D48              # D48 vs 真原版 best/div.cpp
    python local_verify.py D48 D47          # D48 vs D47
    python local_verify.py D48 D47 --gen 60 # 额外用官方 gen 生成 60 个随机例

为什么本地做:
    - VM 只剩「增益下界」价值, 正确性本来就该在本地逐字节 diff (官方 gen 在
      E:/library-checker-problems-master, 26 例官方输入已缓存在 lc_bench/cases/div)
    - VM 关机 / IP 漂移都不再阻塞红线验证

判据: 两个可执行文件在同一输入上的 **stdout 逐字节完全相同**。
      基线必须是已知正确的版本 (best/div.cpp = LC 上跑通的真原版, 或已验证的 Dxx)。
"""
import os
import subprocess
import sys
import time

PS = os.path.dirname(os.path.abspath(__file__))
CASES = os.path.join(PS, "lc_bench", "cases", "div")
GENBIN = os.path.join(PS, "gen_bin")
BUILD = os.path.join(PS, ".verify_build")
FLAGS = ["-O2", "-std=c++23", "-march=x86-64-v3"]


def src_of(tag):
    """tag 可以是 'D48' / 'base' / 直接给 .cpp 路径"""
    if tag.endswith(".cpp"):
        return tag if os.path.isabs(tag) else os.path.join(PS, tag)
    if tag.lower() in ("base", "orig", "div"):
        return os.path.join(PS, "best", "div.cpp")
    return os.path.join(PS, "best", "div_%s.cpp" % tag)


def build(tag):
    src = src_of(tag)
    assert os.path.exists(src), "找不到源码: " + src
    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "%s.exe" % tag.replace(".cpp", "").replace("/", "_"))
    if os.path.exists(exe) and os.path.getmtime(exe) > os.path.getmtime(src):
        print("  [skip] %-6s 已是最新" % tag)
        return exe
    t = time.time()
    r = subprocess.run(["g++"] + FLAGS + ["-o", exe, src],
                       capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        print(r.stderr[-3000:])
        sys.exit("编译失败: " + tag)
    print("  [build] %-6s %.1fs" % (tag, time.time() - t))
    return exe


def run_one(exe, data):
    return subprocess.run([exe], input=data, capture_output=True).stdout


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ngen = 0
    for a in sys.argv[1:]:
        if a.startswith("--gen"):
            ngen = int(a.split("=")[1]) if "=" in a else 0
    if "--gen" in sys.argv:
        i = sys.argv.index("--gen")
        if i + 1 < len(sys.argv):
            ngen = int(sys.argv[i + 1])
            args = [a for a in args if a != sys.argv[i + 1]]
    cand = args[0] if args else "D48"
    base = args[1] if len(args) > 1 else "base"

    print("候选 = %s   基线 = %s" % (cand, base))
    exe_c = build(cand)
    exe_b = build(base)

    files = sorted(f for f in os.listdir(CASES) if f.endswith(".in"))
    print("\n官方缓存例 %d 个:" % len(files))
    bad = []
    for f in files:
        data = open(os.path.join(CASES, f), "rb").read()
        t = time.time()
        ob = run_one(exe_b, data)
        oc = run_one(exe_c, data)
        ok = (ob == oc)
        if not ok:
            bad.append(f)
        print("  %-34s %s  base %7d B  cand %7d B  %5.2fs"
              % (f, "OK " if ok else "*** MISMATCH ***", len(ob), len(oc), time.time() - t))

    print("\n官方例结果: %d/%d 逐字节一致" % (len(files) - len(bad), len(files)))

    # ---- 定向随机 fuzz: 专打 AVX2 尾块 / 小尺寸 / 极端长度比 ----
    # 官方 26 例主要是大数, 覆盖不到「非 16 倍数 limb 的尾块」和小输入,
    # 而 SWAR 借位链恰好有两个站点在尾块里 -> 必须单独轰。
    if ngen:
        import random
        rnd = random.Random(0xD48)
        print("\n随机 fuzz %d 例 (定向轰尾块 / 小尺寸 / 长度比):" % ngen)
        nbad = 0
        for k in range(ngen):
            if k < ngen // 3:                      # 小 & 尾块敏感
                la = rnd.randint(1, 200); lb = rnd.randint(1, la)
            elif k < 2 * ngen // 3:                # 中等 + 各种非对齐长度
                la = rnd.randint(200, 20000); lb = rnd.randint(1, la)
            else:                                  # 长度比极端 (b 很短)
                la = rnd.randint(5000, 60000); lb = rnd.randint(1, 40)
            # 直接拼数字串, 不走 int (Python 3.11+ 对 >4300 位 int->str 有硬限制)
            def mk(L, nz):
                first = str(rnd.randint(1 if (L > 1 or nz) else 0, 9))
                return first + "".join(rnd.choice("0123456789") for _ in range(L - 1))
            a = mk(la, False)
            b = mk(lb, True)
            data = ("%s %s\n" % (a, b)).encode()
            if run_one(exe_b, data) != run_one(exe_c, data):
                nbad += 1
                print("  *** MISMATCH  len(a)=%d len(b)=%d" % (la, lb))
                open(os.path.join(PS, "fuzz_fail_%d.in" % k), "wb").write(data)
        print("  fuzz: %d/%d 一致" % (ngen - nbad, ngen))
        bad += ["fuzz"] * nbad

    if bad:
        print("\n不一致: " + ", ".join(bad))
        sys.exit(1)
    print("\n554 红线 (本地官方例 + fuzz) 全绿")


if __name__ == "__main__":
    main()
