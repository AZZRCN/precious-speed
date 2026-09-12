#!/usr/bin/env python3
# oracle_check.py -- 局部优化守口: 优化版 vs 参考版 逐字节相等 (硬判) + Q*B+R==A 告警 + instructions:u
# 用法 (VM):
#   python3 oracle_check.py --a ./div_base16_opt --b ./div_best --n 240 --perf
#   (--perf 用 /tmp/one_case.txt, REPS 取自环境变量 REPS, 默认 10)
import argparse, random, subprocess, sys

def rand_hex(nbits):
    val = random.getrandbits(nbits) or 1
    return format(val, 'X')

def gen_cases(sizes, n_per):
    cases = []
    for nb in sizes:
        for _ in range(n_per):
            A = rand_hex(nb)
            Bbits = max(1, random.randint(1, nb))
            B = rand_hex(Bbits)
            if B in ('0', ''): B = '1'
            cases.append((A, B))
    return cases

def write_in(cases, path):
    with open(path, 'w') as f:
        f.write(f"{len(cases)}\n")
        for A, B in cases:
            f.write(f"{A} {B}\n")

def run(binpath, inp, perf=False):
    if perf:
        cmd = ['perf', 'stat', '-e', 'instructions:u', '-x', ',', binpath]
        p = subprocess.run(cmd, stdin=open(inp), capture_output=True, text=True)
        instr = None
        for line in p.stderr.splitlines():
            if 'instructions:u' in line:
                try: instr = int(line.split(',')[0])
                except ValueError: pass
        return p.stdout, instr
    p = subprocess.run([binpath], stdin=open(inp), capture_output=True, text=True)
    return p.stdout, None

def qbr_ok(A, B, line):
    parts = line.split()
    if len(parts) != 2: return False
    try:
        q = int(parts[0], 16); r = int(parts[1], 16); a = int(A,16); b = int(B,16)
    except ValueError:
        return False
    if r < 0 or r >= b: return False
    if q * b + r != a: return False
    return True

def main():
    random.seed(0xC0FFEE)  # 固定种子: 各变体测同一批用例, 结果可复现
    ap = argparse.ArgumentParser()
    ap.add_argument('--a', required=True)
    ap.add_argument('--b', required=True)
    ap.add_argument('--n', type=int, default=240)
    ap.add_argument('--sizes', default='256,1024,4096,16384,65536,262144,400000')
    ap.add_argument('--perf', action='store_true')
    ap.add_argument('--perfcase', default='/tmp/one_case.txt')
    ap.add_argument('--reps', default=None)
    args = ap.parse_args()

    sizes = [int(x) for x in args.sizes.split(',')]
    n_per = max(1, args.n // len(sizes))
    cases = gen_cases(sizes, n_per)
    inp = '/tmp/oracle_in.txt'
    write_in(cases, inp)

    out_a, _ = run(args.a, inp)
    out_b, _ = run(args.b, inp)
    la = out_a.strip().splitlines()
    lb = out_b.strip().splitlines()

    if len(la) != len(lb):
        print(f"FAIL: 行数不一致 a={len(la)} b={len(lb)}")
        sys.exit(1)

    neq = 0      # 字节不等 (硬判失败)
    qbr_warn = 0 # 字节相等但 QBR 不成立 (参考本身 envelope 现象, 告警)
    first_diff = None
    for idx, (A, B) in enumerate(cases):
        if la[idx] != lb[idx]:
            neq += 1
            if first_diff is None:
                first_diff = (idx, A[:20], la[idx][:48], lb[idx][:48])
        else:
            if not qbr_ok(A, B, la[idx]):
                qbr_warn += 1

    if neq == 0:
        print(f"BYTE-EQUAL: {len(cases)} 例, 优化版与参考版逐字节一致")
    else:
        print(f"BYTE-DIFF: {neq} 例不一致 (硬判失败)")
        if first_diff:
            print(f"  first: idx={first_diff[0]} A={first_diff[1]}..")
            print(f"    opt ={first_diff[2]}")
            print(f"    ref ={first_diff[3]}")
        sys.exit(1)

    if qbr_warn:
        print(f"QBR-WARN: {qbr_warn} 例字节相等但 Q*B+R!=A (参考 envelope 边界, 非本次引入)")
    else:
        print("QBR: 全部满足 Q*B+R==A")

    if args.perf:
        oa, ia = run(args.a, args.perfcase, perf=True)
        print(f"PERF({args.perfcase}):")
        if ia:
            print(f"  instructions:u (total, REPS={args.reps or 'env'}) = {ia}")
            print(f"  per-call ≈ {ia/max(1,int(args.reps or 10))}")
        else:
            print("  perf 读取失败")

if __name__ == '__main__':
    main()
