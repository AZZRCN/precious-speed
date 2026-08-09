#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""vmbench.py —— 通用 VM(Linux) 计时框架。**以后所有计时实验统一走这里，不在 Windows 本机跑。**

铁律来源 (2026-08-07 用户明令):
    除超优化器暴力搜索外, 一切在 VM(192.168.1.55) 上跑。
理由 (已实测坐实, 不是洁癖):
    div_*.cpp 的 initInput() 在 __linux__ 走 mmap 零拷贝, 在 Windows 退化为 fread 拷 8MB。
    同一份 D47 的五段画像:  read 占比  本机 13~20%  vs  VM Linux 0%
                            非除法    本机 22~28%  vs  VM Linux 5.5~6.4%
    => 本机测量是**结构性失真**, 据此归因会把战场指错方向。

测量纪律 (全部内建, 不用每个实验重写):
    - 预热轮丢弃 (warmup)
    - **交替轮转**: 每一 rep 内按轮转顺序跑全部 variant, 消除机器状态漂移
    - **配对比值 median-of-ratios**: 每 rep 内算 v/baseline, 对 rep 取中位数
      (比 "各自取最小再相除" 抗噪得多, 且能直接给出置信区间)
    - 同时报 min 绝对值供参考

用法:
    python vmbench.py <config.json> up     # 生成变体源码 + 上传 + VM 编译
    python vmbench.py <config.json> go     # 后台跑
    python vmbench.py <config.json> poll   # 看结果

config.json:
{
  "name":     "thr2",                       # 实验名 (决定远程路径/日志名)
  "base_src": "best/div_D47.cpp",
  "patch":    [{"old": "...", "new": "..."}],   # 可选, 源码参数化 (每条须唯一命中)
  "guard":    "#ifndef X\\n#define X 64\\n#endif\\n",  # 可选, 插到首个 #include 前
  "variants": [ {"name":"L64","defines":["DIV_BASIC_LIMIT=64"]}, ... ],
  "baseline": "L64",                        # 比值分母
  "cases":    ["r_nearly_zero_01", ...],
  "reps":     7,
  "warmup":   1,
  "metric":   "div",       # TOPPROF 段名 (read/parse/div/fmt/write/total) 或 "wall"
  "cflags":   "-O2 -std=c++23 -march=x86-64-v3"
}

注意: 需 paramiko => 必须用 "C:/Program Files/Python311/python.exe" 跑。
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

RROOT = "/home/azzr/divbench"
IN_DIR = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"


def load_cfg(p):
    cfg = json.load(open(p, "r", encoding="utf-8"))
    cfg.setdefault("reps", 7)
    cfg.setdefault("warmup", 1)
    cfg.setdefault("metric", "div")
    cfg.setdefault("cflags", "-O2 -std=c++23 -march=x86-64-v3")
    cfg.setdefault("patch", [])
    cfg.setdefault("guard", "")
    cfg.setdefault("baseline", cfg["variants"][0]["name"])
    return cfg


def gen_src(cfg):
    """套用 patch + guard, 产出参数化源码 (保持原换行)。"""
    src = os.path.join(HERE, cfg["base_src"].replace("/", os.sep))
    raw = open(src, "r", encoding="utf-8", newline="").read()
    crlf = "\r\n" in raw
    for pt in cfg["patch"]:
        n = raw.count(pt["old"])
        if n != 1:
            print("FATAL: patch 命中 %d 次 (需恰好 1): %r" % (n, pt["old"][:70]))
            sys.exit(1)
        raw = raw.replace(pt["old"], pt["new"])
    if cfg["guard"]:
        i = raw.find("#include")
        if i < 0:
            print("FATAL: 找不到 #include")
            sys.exit(1)
        g = cfg["guard"]
        if crlf:
            g = g.replace("\r\n", "\n").replace("\n", "\r\n")
        raw = raw[:i] + g + raw[i:]
    out = os.path.join(HERE, ".vmbench", "%s_param.cpp" % cfg["name"])
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w", encoding="utf-8", newline="").write(raw)
    return out


def with_sentinel(cfg):
    """插入哨兵变体 = baseline 的逐字节副本(仅二进制名不同)。

    它与 baseline 的比值**理论上恒为 1.000**, 实测偏差就是当次运行的真实噪声地板。
    没有它, 任何 <10%% 的结论都是自欺 (实测: 逻辑上不受参数影响的 case 曾报 -9.04%%)。
    """
    if any(v["name"] == "SENT" for v in cfg["variants"]):
        return cfg
    base = next(v for v in cfg["variants"] if v["name"] == cfg["baseline"])
    s = {"name": "SENT", "defines": list(base.get("defines", []))}
    cfg["variants"] = cfg["variants"] + [s]
    return cfg


def cmd_up(cfg):
    cfg = with_sentinel(cfg)
    src = gen_src(cfg)
    rsrc = "%s/src/%s_param.cpp" % (RROOT, cfg["name"])
    put(src, rsrc)
    print("[put] %s -> %s" % (os.path.basename(src), rsrc))
    need_prof = cfg["metric"] in ("read", "parse", "div", "fmt", "write", "total")
    parts = ["mkdir -p %s/bin %s/logs && cd %s" % (RROOT, RROOT, RROOT)]
    for v in cfg["variants"]:
        d = " ".join("-D%s" % x for x in v.get("defines", []))
        if need_prof:
            d += " -DTOPPROF"
        exe = "bin/%s_%s" % (cfg["name"], v["name"])
        parts.append("g++ %s %s %s -o %s 2>&1 | tail -8 && echo 'OK %s'"
                     % (cfg["cflags"], d, rsrc, exe, v["name"]))
    rc, out, err = run(" && ".join(parts), timeout=1800)
    print(out or err)


def cmd_poll(cfg):
    log = "%s/logs/%s.log" % (RROOT, cfg["name"])
    rc, out, err = run("cat %s 2>/dev/null; echo '--- alive:'; "
                       "pgrep -fa vmbench_%s | head -3" % (log, cfg["name"]),
                       timeout=60)
    print(out)


REMOTE = r'''#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, os, re, statistics, json
CFG = json.loads(r"""%(cfg)s""")
IN = "%(indir)s"
ROOT = "%(root)s"
NAME = CFG["name"]; METRIC = CFG["metric"]
VARS = [v["name"] for v in CFG["variants"]]
BASE = CFG["baseline"]
PAT = re.compile(r"read=([\d.]+) parse=([\d.]+) div=([\d.]+) fmt=([\d.]+) write=([\d.]+)\s+total=([\d.]+)")
IDX = {"read":0,"parse":1,"div":2,"fmt":3,"write":4,"total":5}

PERF_EV = ("cycles", "instructions", "branch-misses", "cache-misses",
           "task-clock", "L1-dcache-load-misses", "dTLB-load-misses")
CG_RE = re.compile(r"I\s+refs:\s+([\d,]+)")

def one(vn, f):
    exe = "%%s/bin/%%s_%%s" %% (ROOT, NAME, vn)
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
        cg = "/tmp/cg_%%s_%%s.out" %% (NAME, vn)
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

print("[vmbench] %%s  metric=%%s  reps=%%d warmup=%%d  baseline=%%s"
      %% (NAME, METRIC, CFG["reps"], CFG["warmup"], BASE), flush=True)
print("[vmbench] variants: %%s" %% ", ".join(VARS), flush=True)
print("", flush=True)

hdr = "%%-28s" %% "case" + "".join("%%12s" %% v for v in VARS)
print(hdr, flush=True); print("-"*len(hdr), flush=True)
allres = {}
for c in CFG["cases"]:
    f = IN + c + ".in"
    if not os.path.exists(f):
        print("%%-28s MISSING" %% c, flush=True); continue
    raw = {v: [] for v in VARS}
    ratios = {v: [] for v in VARS}
    for r in range(CFG["warmup"] + CFG["reps"]):
        order = VARS[r %% len(VARS):] + VARS[:r %% len(VARS)]   # 交替轮转
        cur = {}
        for v in order:
            t = one(v, f)
            if t is not None: cur[v] = t
        if r < CFG["warmup"]: continue
        for v, t in cur.items(): raw[v].append(t)
        if BASE in cur and cur[BASE] > 0:
            for v, t in cur.items(): ratios[v].append(t / cur[BASE])
    line = "%%-28s" %% c
    row = {}
    # 主判据 = ratio-of-mins: 2026-08-07 哨兵标定噪声 +-0.2%% (median-of-ratios 为 +-0.7%%,
    # wall 为 +-9%%). 干扰几乎只会「增加」耗时, 故各自取 min 最接近无干扰真值。
    bmin = min(raw[BASE]) if raw.get(BASE) else None
    for v in VARS:
        if not raw[v] or not bmin: line += "%%12s" %% "-"; continue
        rm = min(raw[v]) / bmin
        row[v] = {"ratio": rm, "min": min(raw[v]),
                  "median_ratio": statistics.median(ratios[v]) if ratios[v] else None,
                  "n": len(raw[v])}
        line += "%%12s" %% ("%%+.2f%%%%" %% ((rm-1)*100))
    print(line, flush=True)
    allres[c] = row
print("-"*len(hdr), flush=True)
print("(ratio-of-mins vs %%s; 交替轮转 %%d reps, 丢 %%d 预热; SENT=同源哨兵, 其值即本轮噪声)"
      %% (BASE, CFG["reps"], CFG["warmup"]), flush=True)
sent = [abs(r["SENT"]["ratio"]-1)*100 for r in allres.values() if "SENT" in r]
if sent:
    nf = max(sent)
    print("[噪声地板] SENT 最大偏离 = %%.3f%%%%  => 只有 |delta| > %%.2f%%%% 的结论可信"
          %% (nf, 2*nf), flush=True)
print("", flush=True)
print("(参考: median-of-ratios)", flush=True)
for c, row in allres.items():
    line = "%%-28s" %% c
    for v in VARS:
        mr = row.get(v, {}).get("median_ratio")
        line += "%%12s" %% (("%%+.2f%%%%" %% ((mr-1)*100)) if mr else "-")
    print(line, flush=True)
print("", flush=True)

hdr2 = "%%-28s" %% "case(绝对 ms, min)" + "".join("%%12s" %% v for v in VARS)
print(hdr2, flush=True); print("-"*len(hdr2), flush=True)
for c, row in allres.items():
    line = "%%-28s" %% c
    for v in VARS:
        line += "%%12s" %% (("%%.2f" %% row[v]["min"]) if v in row and row[v]["min"] else "-")
    print(line, flush=True)
print("-"*len(hdr2), flush=True)
print("", flush=True)
thr = 1 - 2*(max(sent) if sent else 1.0)/100.0     # 判优门槛 = 2x 实测噪声地板
for c, row in allres.items():
    ok = {v: d["ratio"] for v, d in row.items() if v != "SENT"}
    if not ok: continue
    b = min(ok, key=lambda k: ok[k])
    if ok[b] < thr:
        print("  ** %%-28s 最优 %%-10s %%+.2f%%%%  (超噪声地板, 可信)"
              %% (c, b, (ok[b]-1)*100), flush=True)
    else:
        print("     %%-28s 全部落在噪声内 (最好 %%s %%+.2f%%%%), 无结论"
              %% (c, b, (ok[b]-1)*100), flush=True)
json.dump(allres, open("%%s/logs/%%s.json" %% (ROOT, NAME), "w"), indent=1)
print("DONE", flush=True)
'''


def cmd_go(cfg):
    body = REMOTE % {"cfg": json.dumps(cfg, ensure_ascii=False),
                     "indir": IN_DIR, "root": RROOT}
    p = os.path.join(HERE, ".vmbench", "_remote_%s.py" % cfg["name"])
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w", encoding="utf-8", newline="\n").write(body)
    rp = "/home/azzr/vmbench_%s.py" % cfg["name"]
    put(p, rp)
    log = "%s/logs/%s.log" % (RROOT, cfg["name"])
    run("mkdir -p %s/logs && rm -f %s" % (RROOT, log))
    run("cd /home/azzr && nohup python3 %s > %s 2>&1 & echo started" % (rp, log),
        timeout=30)
    print("started; poll: python vmbench.py %s poll" % sys.argv[1])


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    cfg = load_cfg(sys.argv[1])
    c = sys.argv[2] if len(sys.argv) > 2 else "poll"
    {"up": cmd_up, "go": cmd_go, "poll": cmd_poll}[c](cfg)
