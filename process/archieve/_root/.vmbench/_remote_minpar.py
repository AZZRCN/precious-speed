#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, os, re, statistics, json
CFG = json.loads(r"""{"name": "minpar", "base_src": "best/div_D47.cpp", "patch": [{"old": "constexpr size_t MIN_PAR = 2048;   // 小规模并行化不划算, 走原串行路径", "new": "constexpr size_t MIN_PAR = CPS_MIN_PAR;   // [vmbench] 参数化"}], "guard": "#ifndef CPS_MIN_PAR\n#define CPS_MIN_PAR 2048\n#endif\n", "variants": [{"name": "P2048", "defines": ["CPS_MIN_PAR=2048"]}, {"name": "P1024", "defines": ["CPS_MIN_PAR=1024"]}, {"name": "P512", "defines": ["CPS_MIN_PAR=512"]}, {"name": "P256", "defines": ["CPS_MIN_PAR=256"]}, {"name": "P128", "defines": ["CPS_MIN_PAR=128"]}, {"name": "P64", "defines": ["CPS_MIN_PAR=64"]}], "baseline": "P2048", "cases": ["r_nearly_zero_01", "burnikel_ziegler_bound_02", "a_max_b_random_02", "burnikel_ziegler_bound_00", "length_ratio_integer_03", "large_00", "medium_01"], "reps": 25, "warmup": 3, "metric": "cycles", "cflags": "-O2 -std=c++23 -march=x86-64-v3"}""")
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"
ROOT = "/home/azzr/divbench"
NAME = CFG["name"]; METRIC = CFG["metric"]
VARS = [v["name"] for v in CFG["variants"]]
BASE = CFG["baseline"]
PAT = re.compile(r"read=([\d.]+) parse=([\d.]+) div=([\d.]+) fmt=([\d.]+) write=([\d.]+)\s+total=([\d.]+)")
IDX = {"read":0,"parse":1,"div":2,"fmt":3,"write":4,"total":5}

PERF_EV = ("cycles", "instructions", "branch-misses", "cache-misses",
           "task-clock", "L1-dcache-load-misses", "dTLB-load-misses")
CG_RE = re.compile(r"I\s+refs:\s+([\d,]+)")

def one(vn, f):
    exe = "%s/bin/%s_%s" % (ROOT, NAME, vn)
    if METRIC == "wall":
        import time
        with open(f,"rb") as fi:
            t0=time.perf_counter()
            subprocess.run([exe], stdin=fi, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return (time.perf_counter()-t0)*1000.0
    if METRIC in PERF_EV:
        # 硬件计数器: 只在本进程实际执行时递增 => 免疫调度抢占
        with open(f,"rb") as fi:
            p = subprocess.run(["perf","stat","-x,","-e",METRIC,exe],
                               stdin=fi, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
        for line in p.stderr.decode("utf-8","replace").splitlines():
            fs = line.split(",")
            if len(fs) >= 3 and fs[2].strip() == METRIC:
                try: return float(fs[0])
                except ValueError: return None
        return None
    if METRIC == "ir":
        # callgrind: 确定性模拟, 噪声恒 0 => 只需跑 1 次
        cg = "/tmp/cg_%s_%s.out" % (NAME, vn)
        with open(f,"rb") as fi:
            p = subprocess.run(["valgrind","--tool=callgrind","--callgrind-out-file="+cg,
                                "--cache-sim=no", exe],
                               stdin=fi, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
        m = CG_RE.search(p.stderr.decode("utf-8","replace"))
        return float(m.group(1).replace(",","")) if m else None
    with open(f,"rb") as fi:
        p = subprocess.run([exe], stdin=fi, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    m = PAT.search(p.stderr.decode("utf-8","replace"))
    return float(m.groups()[IDX[METRIC]]) if m else None

if METRIC == "ir":            # 确定性 => 重复毫无意义, 且 valgrind 慢 50x
    CFG["reps"] = 1; CFG["warmup"] = 0

print("[vmbench] %s  metric=%s  reps=%d warmup=%d  baseline=%s"
      % (NAME, METRIC, CFG["reps"], CFG["warmup"], BASE), flush=True)
print("[vmbench] variants: %s" % ", ".join(VARS), flush=True)
print("", flush=True)

hdr = "%-28s" % "case" + "".join("%12s" % v for v in VARS)
print(hdr, flush=True); print("-"*len(hdr), flush=True)
allres = {}
for c in CFG["cases"]:
    f = IN + c + ".in"
    if not os.path.exists(f):
        print("%-28s MISSING" % c, flush=True); continue
    raw = {v: [] for v in VARS}
    ratios = {v: [] for v in VARS}
    for r in range(CFG["warmup"] + CFG["reps"]):
        order = VARS[r % len(VARS):] + VARS[:r % len(VARS)]   # 交替轮转
        cur = {}
        for v in order:
            t = one(v, f)
            if t is not None: cur[v] = t
        if r < CFG["warmup"]: continue
        for v, t in cur.items(): raw[v].append(t)
        if BASE in cur and cur[BASE] > 0:
            for v, t in cur.items(): ratios[v].append(t / cur[BASE])
    line = "%-28s" % c
    row = {}
    # 主判据 = ratio-of-mins: 2026-08-07 哨兵标定噪声 +-0.2% (median-of-ratios 为 +-0.7%,
    # wall 为 +-9%). 干扰几乎只会「增加」耗时, 故各自取 min 最接近无干扰真值。
    bmin = min(raw[BASE]) if raw.get(BASE) else None
    for v in VARS:
        if not raw[v] or not bmin: line += "%12s" % "-"; continue
        rm = min(raw[v]) / bmin
        row[v] = {"ratio": rm, "min": min(raw[v]),
                  "median_ratio": statistics.median(ratios[v]) if ratios[v] else None,
                  "n": len(raw[v])}
        line += "%12s" % ("%+.2f%%" % ((rm-1)*100))
    print(line, flush=True)
    allres[c] = row
print("-"*len(hdr), flush=True)
print("(ratio-of-mins vs %s; 交替轮转 %d reps, 丢 %d 预热; SENT=同源哨兵, 其值即本轮噪声)"
      % (BASE, CFG["reps"], CFG["warmup"]), flush=True)
sent = [abs(r["SENT"]["ratio"]-1)*100 for r in allres.values() if "SENT" in r]
if sent:
    nf = max(sent)
    print("[噪声地板] SENT 最大偏离 = %.3f%%  => 只有 |delta| > %.2f%% 的结论可信"
          % (nf, 2*nf), flush=True)
print("", flush=True)
print("(参考: median-of-ratios)", flush=True)
for c, row in allres.items():
    line = "%-28s" % c
    for v in VARS:
        mr = row.get(v, {}).get("median_ratio")
        line += "%12s" % (("%+.2f%%" % ((mr-1)*100)) if mr else "-")
    print(line, flush=True)
print("", flush=True)

hdr2 = "%-28s" % "case(绝对 ms, min)" + "".join("%12s" % v for v in VARS)
print(hdr2, flush=True); print("-"*len(hdr2), flush=True)
for c, row in allres.items():
    line = "%-28s" % c
    for v in VARS:
        line += "%12s" % (("%.2f" % row[v]["min"]) if v in row and row[v]["min"] else "-")
    print(line, flush=True)
print("-"*len(hdr2), flush=True)
print("", flush=True)
thr = 1 - 2*(max(sent) if sent else 1.0)/100.0     # 判优门槛 = 2x 实测噪声地板
for c, row in allres.items():
    ok = {v: d["ratio"] for v, d in row.items() if v != "SENT"}
    if not ok: continue
    b = min(ok, key=lambda k: ok[k])
    if ok[b] < thr:
        print("  ** %-28s 最优 %-10s %+.2f%%  (超噪声地板, 可信)"
              % (c, b, (ok[b]-1)*100), flush=True)
    else:
        print("     %-28s 全部落在噪声内 (最好 %s %+.2f%%), 无结论"
              % (c, b, (ok[b]-1)*100), flush=True)
json.dump(allres, open("%s/logs/%s.json" % (ROOT, NAME), "w"), indent=1)
print("DONE", flush=True)
