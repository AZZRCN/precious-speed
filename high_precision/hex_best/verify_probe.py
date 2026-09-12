#!/usr/bin/env python3
# GMP 同层探针: cyclic 路径输出 vs 精确线性(NOCYCLIC=1) 输出 逐 case 比对.
# 同一二进制, env 切换 NOCYCLIC; 两者逐字节一致 => cyclic 对 GMP 精确语义零误差.
import subprocess, os, sys, time

GENS = ["a_max_b_random","burnikel_ziegler_bound","large","length_ratio_integer",
        "max","medium","power","r_nearly_zero","small"]
SEEDS = [int(x) for x in sys.argv[2:]] if len(sys.argv) > 2 else list(range(1,9))
divbin = sys.argv[1]
GENBIN = "./genbin"
RES = "probe_result.txt"

def run_div(A, B, nocyclic):
    env = dict(os.environ)
    if nocyclic:
        env["NOCYCLIC"] = "1"
    else:
        env.pop("NOCYCLIC", None)
    inp = "1\n" + A + " " + B + "\n"
    r = subprocess.run([divbin], input=inp, capture_output=True, text=True, timeout=120, env=env)
    return r.stdout.split()

def gen_cases(gen, seed):
    out = subprocess.run([os.path.join(GENBIN, gen), str(seed)], capture_output=True, text=True).stdout
    lines = out.split("\n")
    T = int(lines[0])
    cases = []
    i = 1
    for _ in range(T):
        if i >= len(lines):
            break
        p = lines[i].split()
        if len(p) < 2:
            i += 1
            continue
        cases.append((p[0], p[1]))
        i += 1
    return cases

f = open(RES, "w")
total = 0
mismatch = 0
t0 = time.time()
for g in GENS:
    for s in SEEDS:
        cases = gen_cases(g, s)
        gm = 0
        for (A, B) in cases:
            total += 1
            cyc = run_div(A, B, False)
            lin = run_div(A, B, True)
            if cyc != lin:
                gm += 1
                mismatch += 1
                if gm <= 3:  # 每个 gen/seed 最多记 3 个样本
                    f.write(f"MISMATCH {g} seed={s} A={A[:40]}.. B={B[:40]}.. cyc={cyc} lin={lin}\n")
        line = f"{g} seed={s} cases={len(cases)} mismatch={gm}"
        print(line, flush=True)
        f.write(line + "\n")
        f.flush()
dt = time.time() - t0
summary = f"TOTAL cases={total} MISMATCH={mismatch} dt={dt:.1f}s VERDIKT={'PASS' if mismatch==0 else 'FAIL'}"
print(summary, flush=True)
f.write(summary + "\n")
f.close()
