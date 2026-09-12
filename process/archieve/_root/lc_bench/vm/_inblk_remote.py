#!/usr/bin/env python3
"""块数(in)选择 A/B: 当前 D16 nb 模型 vs 强制 2 块(-DDIV_MU_IN_HALF)。

同时给出: FFT 总工作量(NlogN) / 正确性对拍 / 配对 wall-clock 比值。
"""
import os, re, subprocess, sys, statistics

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
OUTD = "/home/azzr/lcp/big_integer/division_of_big_integers/out"
CASES = (sys.argv[1] if len(sys.argv) > 1 else
         "length_ratio_integer_00,length_ratio_integer_01,length_ratio_integer_02,"
         "length_ratio_integer_03,length_ratio_integer_04,length_ratio_integer_05,"
         "a_max_b_random_02,r_nearly_zero_01,burnikel_ziegler_bound_02").split(",")
REP = int(sys.argv[2]) if len(sys.argv) > 2 else 9

BASE_FH, HALF_FH = ROOT + "/bin/fh_div_D25", ROOT + "/bin/d25_half_fh"
BASE, HALF = ROOT + "/bin/d25", ROOT + "/bin/d25_half"


def sh(cmd, timeout=900):
    p = subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True, text=True,
                       timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def nlogn(exe, case):
    rc, o, e = sh("%s < %s/%s.in > /dev/null" % (exe, IN, case))
    m = re.search(r"sum\(N\*logN\)=([0-9.e+]+)", e)
    t = re.search(r"TOTAL transforms=(\d+)", e)
    return (float(m.group(1)) if m else None, int(t.group(1)) if t else None)


def timeit(exe, case):
    rc, o, e = sh("/usr/bin/time -f %%e %s < %s/%s.in > /tmp/o.txt" % (exe, IN, case))
    try:
        return float(e.strip().splitlines()[-1]) * 1000.0
    except Exception:
        return None


def main():
    print("%-32s %11s %11s %7s | %8s %8s %7s | ok" %
          ("case", "NlogN_3blk", "NlogN_2blk", "ratio", "t_3blk", "t_2blk", "B/A"))
    for c in CASES:
        if not os.path.exists("%s/%s.in" % (IN, c)):
            continue
        n1, _ = nlogn(BASE_FH, c)
        n2, _ = nlogn(HALF_FH, c)
        # 正确性: 两版输出逐字节比
        sh("%s < %s/%s.in > /tmp/a.txt" % (BASE, IN, c))
        sh("%s < %s/%s.in > /tmp/b.txt" % (HALF, IN, c))
        rc, o, e = sh("cmp -s /tmp/a.txt /tmp/b.txt && echo SAME || echo DIFF")
        same = o.strip()
        # 官方答案核对(若有)
        exp = "%s/%s.out" % (OUTD, c)
        if os.path.exists(exp):
            rc2, o2, _ = sh("cmp -s /tmp/b.txt %s && echo AC || echo WA" % exp)
            same += "/" + o2.strip()
        # 配对计时: 交替, 丢首轮
        ra, rb = [], []
        for i in range(REP):
            if i % 2 == 0:
                a = timeit(BASE, c); b = timeit(HALF, c)
            else:
                b = timeit(HALF, c); a = timeit(BASE, c)
            if i == 0:
                continue
            if a and b:
                ra.append(a); rb.append(b)
        ta = statistics.median(ra) if ra else 0
        tb = statistics.median(rb) if rb else 0
        print("%-32s %11.4g %11.4g %7.4f | %8.2f %8.2f %7.4f | %s" %
              (c, n1 or 0, n2 or 0, (n2 / n1) if (n1 and n2) else 0,
               ta, tb, (tb / ta) if ta else 0, same))


main()
