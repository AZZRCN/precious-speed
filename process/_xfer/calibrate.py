#!/usr/bin/env python3
# Calibration harness -- runs ONLY on VM Ubuntu.
# Methodology (per user硬律 2026-08-14):
#   * DEC GEN is the single source of scale spectrum (D:\library-checker-problems-master\big_integer\*\gen).
#   * ONE input (same integer) is fed to TWO programs: DEC BEST (decimal) and HEX BEST (hex, converted in place).
#   * metric = perf instructions:u  (Windows never measures; VM only).
#   * cost-effectiveness = I/O bytes / instructions  (raw + I/O-subtracted + log-transform).
#   * DEC 29ms yardstick: scale point nearest ~290M instructions (~10M ins/ms AMD).
import os, sys, subprocess, math, json, shutil
sys.set_int_max_str_digits(0)  # disable 4300-digit limit: we convert up to 2e6-digit integers

LC = "/home/azzr/lc"
COMMON = os.path.join(LC, "common")
PREC = "/home/azzr/prec"
DEC_BEST = os.path.join(PREC, "dec_best/origin/div.cpp")
HEX_BEST = os.path.join(PREC, "hex_best/div.cpp")
WORK = "/home/azzr/calib"
CXX = "g++-15" if shutil.which("g++-15") else "g++"
CXXFLAGS = "-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"

GEN_DIR = f"{LC}/big_integer/division_of_big_integers/gen"
# DEC div GEN groups (from info.toml). seed counts = test "number".
GROUPS = {"small":1,"medium":3,"large":2,"max":3,"a_max_b_random":3,
          "r_nearly_zero":3,"length_ratio_integer":6,"burnikel_ziegler_bound":4}

def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)

def compile(src, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    r = run(f"{CXX} {CXXFLAGS} -o {out} {src} 2>&1 | head -20")
    return r.returncode == 0 and os.path.exists(out)

def compile_gen(name):
    src = os.path.join(GEN_DIR, name + ".cpp")
    out = os.path.join(WORK, "gen", f"{name}.bin")
    parent = os.path.dirname(GEN_DIR)
    r = run(f"{CXX} -O2 -std=c++17 -I{parent} -I{COMMON} -o {out} {src} 2>&1 | head -20")
    return out if (r.returncode == 0 and os.path.exists(out)) else None

def gen_input(name, seed):
    out = os.path.join(WORK, "in", f"{name}_{seed}.in")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    run(f"{os.path.join(WORK,'gen',name+'.bin')} {seed} > {out}")
    return out

def dec_to_hex_in(decin):
    # same integers, hex encoding (uppercase, no 0x) -> feed HEX best
    out = decin.replace(".in", ".hex.in")
    with open(decin) as f:
        lines = f.read().split("\n")
    T = int(lines[0])
    hexlines = [str(T)]
    for ln in lines[1:T+1]:
        if not ln.strip(): continue
        a, b = ln.split()
        hexlines.append(f"{int(a):X} {int(b):X}")
    with open(out, "w") as f:
        f.write("\n".join(hexlines) + "\n")
    return out

def scale_of(decin):
    with open(decin) as f:
        T = int(f.readline())
        a, b = f.readline().split()
    da, db = len(a), len(b)
    bits = (da + db) * math.log2(10)
    limbs = (da + 1) // 9
    return da, db, bits, limbs

def perf_run(binpath, infile):
    out = infile + ".out"
    pf = infile + ".perf"
    run(f"perf stat -e instructions:u {binpath} < {infile} > {out} 2>{pf}")
    instr = None
    try:
        for line in open(pf):
            if "instructions:u" in line:
                instr = int(line.split()[0].replace(",", ""))
    except: pass
    ob = os.path.getsize(out) if os.path.exists(out) else 0
    ib = os.path.getsize(infile)
    return instr, ib, ob, out

def verify(outfile, base):
    try:
        toks = open(outfile).read().split()
        T = int(toks[0]); o = 1
        for _ in range(T):
            Q = int(toks[o]); R = int(toks[o+1]); o += 2
            # we cannot re-check A,B here without input; trust structure + equality check
        return True
    except Exception:
        return False

def verify_pair(decin, decout, hexout):
    # decode both, ensure same Q,R and Q*B+R==A
    # NOTE: program OUTPUT has NO T header (just Q R pairs); T comes from INPUT.
    A = []; B = []
    with open(decin) as f:
        toks = f.read().split()
    T = int(toks[0]); k = 1
    for _ in range(T):
        A.append(int(toks[k])); B.append(int(toks[k+1])); k += 2
    dq = open(decout).read().split(); hq = open(hexout).read().split()
    try:
        o = 0
        for i in range(T):
            Qd = int(dq[o]); Rd = int(dq[o+1])
            Qh = int(hq[o], 16); Rh = int(hq[o+1], 16); o += 2
            if (Qd, Rd) != (Qh, Rh): return False
            if Qd * B[i] + Rd != A[i] or not (0 <= Rd < B[i]): return False
        return True
    except Exception:
        return False

def main():
    os.makedirs(WORK, exist_ok=True)
    # compile bests
    dec_bin = os.path.join(WORK, "best", "div_dec.bin")
    hex_bin = os.path.join(WORK, "best", "div_hex.bin")
    if not compile(DEC_BEST, dec_bin):
        print("DEC BEST COMPILE FAIL", file=sys.stderr); sys.exit(1)
    if not compile(HEX_BEST, hex_bin):
        print("HEX BEST COMPILE FAIL", file=sys.stderr); sys.exit(1)
    # compile gens
    gen_bin = {}
    for g in GROUPS:
        gb = compile_gen(g)
        if not gb:
            print(f"GEN COMPILE FAIL {g}", file=sys.stderr); continue
        gen_bin[g] = gb
    results = []
    for g in GROUPS:
        if g not in gen_bin: continue
        for seed in range(GROUPS[g]):
            decin = gen_input(g, seed)
            hexin = dec_to_hex_in(decin)
            idi, ibd, obd, decout = perf_run(dec_bin, decin)
            ih, ibh, obh, hexout = perf_run(hex_bin, hexin)
            da, db, bits, limbs = scale_of(decin)
            pair_ok = verify_pair(decin, decout, hexout) if (idi and ih) else False
            iob_d = ibd + obd; iob_h = ibh + obh
            results.append({
                "group": g, "seed": seed,
                "A_digits_dec": da, "B_digits_dec": db, "bits": round(bits), "approx_limbs": limbs,
                "dec_in_bytes": ibd, "dec_out_bytes": obd, "dec_io_bytes": iob_d, "dec_instr": idi,
                "hex_in_bytes": ibh, "hex_out_bytes": obh, "hex_io_bytes": iob_h, "hex_instr": ih,
                "pair_ok": pair_ok,
                "dec_cost_eff_B_per_I": round(iob_d/idi, 4) if idi else 0,
                "hex_cost_eff_B_per_I": round(iob_h/ih, 4) if ih else 0,
                "dec_log": round(math.log10(iob_d)/math.log10(idi), 4) if idi else 0,
                "hex_log": round(math.log10(iob_h)/math.log10(ih), 4) if ih else 0,
            })
    # I/O-subtracted: k = instr/io_bytes from tiny "small" points (computation negligible)
    for base in ["dec", "hex"]:
        tiny = [r for r in results if r["group"] == "small" and r[f"{base}_instr"]]
        k = sum(r[f"{base}_instr"] for r in tiny) / sum(r[f"{base}_io_bytes"] for r in tiny) if tiny else 0
        for r in results:
            if r[f"{base}_instr"]:
                sub = max(r[f"{base}_instr"] - k * r[f"{base}_io_bytes"], 1)
                r[f"{base}_cost_eff_ioadj"] = round(r[f"{base}_io_bytes"] / sub, 4)
    with open(os.path.join(WORK, "calib.json"), "w") as f:
        json.dump(results, f, indent=1)
    # 29ms yardstick (DEC, ~290M instr)
    cand = [r for r in results if r["dec_instr"]]
    yard = min(cand, key=lambda r: abs(r["dec_instr"] - 290_000_000)) if cand else None
    # table
    print("=== CALIBRATION (div) : ONE integer -> DEC BEST (dec) & HEX BEST (hex) ===")
    print("cost-effectiveness = I/O bytes / instructions   [ioadj = I/O-subtracted]")
    print(f"{'group':22}{'sd':3}{'bits':>7}{'limb':>6}{'dec_ioKB':>9}{'dec_IM':>9}{'dec_ce':>8}{'dec_ceA':>8}"
          f"{'hex_ioKB':>9}{'hex_IM':>9}{'hex_ce':>8}{'hex_ceA':>8}{'ok'}")
    for r in results:
        print(f"{r['group']:22}{r['seed']:<3}{r['bits']:>7}{r['approx_limbs']:>6}"
              f"{r['dec_io_bytes']/1024:>9.1f}{r['dec_instr']/1e6:>9.2f}{r['dec_cost_eff_B_per_I']:>8.4f}{r.get('dec_cost_eff_ioadj',0):>8.4f}"
              f"{r['hex_io_bytes']/1024:>9.1f}{r['hex_instr']/1e6:>9.2f}{r['hex_cost_eff_B_per_I']:>8.4f}{r.get('hex_cost_eff_ioadj',0):>8.4f}{r['pair_ok']}")
    print(f"\n=== ratio HEX/DEC (instr at matched integer) ===")
    for r in results:
        if r['dec_instr'] and r['hex_instr']:
            print(f"  {r['group']:22} sd{r['seed']:<2} hex/dec_instr={r['hex_instr']/r['dec_instr']:.3f}  "
                  f"hex/dec_io={r['hex_io_bytes']/r['dec_io_bytes']:.3f}")
    if yard:
        print(f"\n=== DEC 29ms YARDSTICK (target ~290M instr @ ~10M ins/ms AMD) ===")
        print(json.dumps(yard, indent=1))

if __name__ == "__main__":
    main()
