#!/usr/bin/env python3
# test_base16.py -- 验证 div_base16 (HEX base-2^16) 的正确性与指令数
#
# 设计 (遵循用户方法论: 一份输入交给两个程序对比输出正确性与指令数):
#   --a  = div_base16  (本文件移植版, base-2^16, HEX I/O)
#   --b  = div_best    (当前 HEX best, base-2^64, HEX I/O)  -> 正确性交叉校验 (同整数输出必须一致)
#   --dec = div_dec    (DEC best, base-10000, DEC I/O)       -> 仅 perf: 同整数 base 对比 (HEX/DEC 指令比)
#
# 正确性: 随机生成各档位 hex 用例, 跑 --a 与 --b, 逐行比对 stdout; 并对每行验证 Q*B+R==A.
# perf:    取最大档位单用例, 各自在 `perf stat -e instructions:u` 下运行, 报
#           指令数 与  base16/best (HEX 内部对比) 及 base16/dec (HEX/DEC 收敛度) 比值.
#
# 用法 (VM Ubuntu, 已用 g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 编译):
#   python3 test_base16.py --a ./div_base16 --b ./div_best --dec ./div_dec --n 300 --perf
# 或用 DEC GEN 十进制输入复用规模:
#   python3 test_base16.py --a ./div_base16 --b ./div_best --dec ./div_dec --dec-input dec_gen.txt --perf
import argparse, random, subprocess, sys

def rand_hex(nbits):
    val = random.getrandbits(nbits)
    if val == 0:
        val = 1
    return format(val, 'X')  # 大写, 无前导零 (对齐 LC 格式)

def gen_cases(sizes, n_per):
    cases = []
    for nb in sizes:
        for _ in range(n_per):
            A = rand_hex(nb)
            Bbits = max(1, random.randint(1, nb))
            B = rand_hex(Bbits)
            if B in ('0', ''):
                B = '1'
            cases.append((A, B))
    return cases

def write_hex_input(cases, path):
    with open(path, 'w') as f:
        f.write(f"{len(cases)}\n")
        for A, B in cases:
            f.write(f"{A} {B}\n")

def write_dec_input(cases, path):
    with open(path, 'w') as f:
        f.write(f"{len(cases)}\n")
        for A, B in cases:
            f.write(f"{int(A,16)} {int(B,16)}\n")

def run(binpath, inp, perf=False):
    if perf:
        cmd = ['perf', 'stat', '-e', 'instructions:u', '-x', ',', binpath]
        p = subprocess.run(cmd, stdin=open(inp), capture_output=True, text=True)
        instr = None
        for line in p.stderr.splitlines():
            if 'instructions:u' in line:
                try:
                    instr = int(line.split(',')[0])
                except ValueError:
                    pass
        return p.stdout, instr
    p = subprocess.run([binpath], stdin=open(inp), capture_output=True, text=True)
    return p.stdout, None

def verify(A, B, line):
    parts = line.split()
    if len(parts) != 2:
        return False, f"bad line {line!r}"
    q = int(parts[0], 16); r = int(parts[1], 16)
    a = int(A, 16); b = int(B, 16)
    if r < 0 or r >= b:
        return False, f"remainder out of range R={parts[1]}"
    if q * b + r != a:
        return False, "Q*B+R!=A"
    return True, ""

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--a', required=True, help='div_base16 二进制')
    ap.add_argument('--b', required=True, help='当前 HEX best 二进制')
    ap.add_argument('--dec', default='', help='DEC best 二进制 (仅 perf)')
    ap.add_argument('--n', type=int, default=300)
    ap.add_argument('--sizes', default='64,256,1024,4096,16384,65536,262144,1048576')
    ap.add_argument('--perf', action='store_true')
    ap.add_argument('--dec-input', default='', help='可选 DEC GEN 十进制输入文件, 转 hex 复用')
    args = ap.parse_args()

    if args.dec_input:
        cases = []
        with open(args.dec_input) as f:
            T = int(f.readline())
            for _ in range(T):
                la = f.readline().split()
                if len(la) != 2:
                    continue
                cases.append((format(int(la[0]), 'X'), format(int(la[1]), 'X')))
    else:
        sizes = [int(x) for x in args.sizes.split(',')]
        n_per = max(1, args.n // len(sizes))
        cases = gen_cases(sizes, n_per)

    hex_in = '/tmp/base16_in.txt'
    write_hex_input(cases, hex_in)

    out_a, _ = run(args.a, hex_in)
    out_b, _ = run(args.b, hex_in)
    la = out_a.strip().splitlines()
    lb = out_b.strip().splitlines()
    if len(la) != len(lb):
        print(f"FAIL: 输出行数不一致 a={len(la)} b={len(lb)}")
        sys.exit(1)

    fails = 0
    for idx, (A, B) in enumerate(cases):
        if la[idx] == lb[idx]:
            ok, msg = verify(A, B, la[idx])
            if not ok:
                print(f"[{idx}] A={A[:24]}.. 两程序输出一致但校验失败: {msg}")
                fails += 1
        else:
            oka, mka = verify(A, B, la[idx])
            okb, mkb = verify(A, B, lb[idx])
            if not oka:
                print(f"[{idx}] A={A[:24]}.. div_base16 校验失败: {mka}")
            if not okb:
                print(f"[{idx}] A={A[:24]}.. div_best 校验失败: {mkb}")
            if la[idx] != lb[idx]:
                print(f"[{idx}] 两程序输出不一致:\n  base16={la[idx][:64]}\n  best  ={lb[idx][:64]}")
            fails += 1
        if fails > 30:
            print("... 失败过多, 中止")
            break

    if fails == 0:
        print(f"PASS: {len(cases)} 例, div_base16 与 div_best 输出逐字节一致且全部满足 Q*B+R==A")
    else:
        print(f"FAIL: {fails} 例不一致/校验失败")
        sys.exit(1)

    if args.perf:
        big = max(cases, key=lambda c: len(c[0]) + len(c[1]))
        big_hex = '/tmp/base16_big.txt'
        with open(big_hex, 'w') as f:
            f.write("1\n")
            f.write(f"{big[0]} {big[1]}\n")
        oa, ia = run(args.a, big_hex, perf=True)
        ob, ib = run(args.b, big_hex, perf=True)
        print(f"PERF (max case lenA_hex={len(big[0])}):")
        print(f"  div_base16 instructions:u = {ia}")
        print(f"  div_best   instructions:u = {ib}")
        if ia and ib:
            print(f"  base16/best 比值 = {ia/ib:.3f}")
        if args.dec:
            big_dec = '/tmp/base16_big_dec.txt'
            with open(big_dec, 'w') as f:
                f.write("1\n")
                f.write(f"{int(big[0],16)} {int(big[1],16)}\n")
            _, idc = run(args.dec, big_dec, perf=True)
            print(f"  div_dec    instructions:u = {idc}")
            if ia and idc:
                print(f"  HEX/DEC 收敛比 (base16/dec) = {ia/idc:.3f}  (目标 ~1.0)")

if __name__ == '__main__':
    main()
