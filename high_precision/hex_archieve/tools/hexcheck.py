#!/usr/bin/env python3
"""HEX 三题 验证 + 测速 harness (运行于 VM, python3, 仅标准库).

子命令:
  gen     <prob> <seed> <infile> <expfile>   生成随机+边界用例, 写出 .in 与黄金 .exp
  run     <bin>  <infile> <expfile>          运行 bin 并逐字节比对, 输出 OK/BAD
  genbench <prob> <benchin>                  生成代表性大输入(用于 Ir 对比)
  bench   <bin>  <benchin> [--cg]            运行 bin; --cg 时走 callgrind 取 Ir(权威)

prob in {add, mul, div}. 黄金 oracle = Python int(s,16) 精确算术.
"""
import sys, os, subprocess, random, re, time

LOG16 = 1600000
SUMCHAR = 3200002
HEXCH = '0123456789ABCDEF'

def fmt_hex(v: int) -> str:
    if v == 0:
        return "0"
    return format(v, 'X')

def rand_hex(rng, n, allow_neg):
    if n <= 0:
        return "0"
    body = ''.join(rng.choice(HEXCH) for _ in range(n))
    if n > 1:
        body = rng.choice('123456789ABCDEF') + body[1:]   # no leading zero
    if allow_neg and rng.random() < 0.5:
        return '-' + body
    return body

def rand_pos(rng, n):
    # positive, non-zero, no leading zero
    if n <= 0:
        n = 1
    body = ''.join(rng.choice(HEXCH) for _ in range(n))
    if n > 1:
        body = rng.choice('123456789ABCDEF') + body[1:]
    return body

def edges(prob, maxd):
    E = []
    sz = [1, 2, 3, 4, 8, 15, 16, 17, 31, 32, 33, 63, 64, 100,
          255, 256, 257, 1000, 4095, 4096, 4097, 10000]
    for n in sz:
        if n > maxd:
            continue
        E.append(('F' * n, '1'))
        E.append(('1' + '0' * (n - 1), '1' + '0' * (n - 1)))
        E.append(('F' * n, 'F' * n))
        E.append(('10' * (n // 2) + ('1' if n % 2 else ''), 'F' * n))
        E.append(('0', 'F' * n))
        E.append(('F' * n, '0'))
    E.append(('0', '0'))
    E.append(('1', '1'))
    if prob in ('add', 'mul'):
        E.append(('-1', '1'))
        E.append(('1', '-1'))
        E.append(('-FF', 'FF'))
        E.append(('-' + 'F' * 5, '-' + 'F' * 5))
    else:  # div: A>=0, B>0
        for n in sz:
            if n > maxd:
                continue
            B = rand_pos(random.Random(n), min(n, 200))
            A = rand_pos(random.Random(n + 7), min(n, 200))
            E.append((A, B))
            E.append((B, B))            # q=1 r=0
            E.append((B + '0', B))       # A=16B -> exact
            # A = B-1 (r=B-1), A = B+1 (q=1 r=1)
            try:
                bb = int(B, 16)
                E.append((fmt_hex(bb - 1), B))
                E.append((fmt_hex(bb + 1), B))
                E.append((fmt_hex(2 * bb), B))
            except Exception:
                pass
    return E

def gen_cases(prob, rng, n_random, maxd):
    cases = edges(prob, maxd)
    for _ in range(n_random):
        na = rng.randint(1, maxd)
        nb = rng.randint(1, maxd)
        a = rand_hex(rng, na, prob in ('add', 'mul'))
        b = rand_hex(rng, nb, prob in ('add', 'mul'))
        if prob == 'div':
            a = a.lstrip('-') or '0'
            if b == '0' or b.lstrip('-') == '0':
                b = '1'
            else:
                b = b.lstrip('-')
            if int(b, 16) == 0:
                b = '1'
        cases.append((a, b))
    return cases

def expected(prob, a, b):
    av = int(a, 16)
    bv = int(b, 16)
    if prob == 'add':
        return fmt_hex(av + bv)
    if prob == 'mul':
        return fmt_hex(av * bv)
    # div: A>=0 B>0, floor
    if bv == 0:
        raise ValueError('div by zero case a=%r b=%r' % (a, b))
    q = av // bv
    r = av % bv
    return fmt_hex(q) + ' ' + fmt_hex(r)

def write_io(prob, cases, infile, expfile):
    with open(infile, 'w') as fi, open(expfile, 'w') as fe:
        fi.write(str(len(cases)) + '\n')
        for a, b in cases:
            fi.write(f'{a} {b}\n')
            fe.write(expected(prob, a, b) + '\n')

def cmd_gen(prob, seed, infile, expfile, maxmode=False):
    rng = random.Random(seed)
    if maxmode:
        cases = []
        if prob == 'div':
            cases.append((rand_pos(rng, LOG16), rand_pos(rng, LOG16 // 2)))
        else:
            cases.append((rand_hex(rng, LOG16, True), rand_hex(rng, LOG16, True)))
        write_io(prob, cases, infile, expfile)
        print(f'gen(max) {prob}: {len(cases)} cases -> {infile},{expfile}')
        return
    # normal: edges + budgeted random, total length <= ~3.0M (problem constraint)
    BUDGET = 3_000_000
    cases = edges(prob, 200000)
    # div 要求 B>0: 把任何 B==0 的非法边例换为 B=1 (A>=0 仍允许为 0)
    if prob == 'div':
        cases = [(a, '1' if b.strip('0') == '' else b) for (a, b) in cases]
    used = sum(len(a) + len(b) for a, b in cases)
    n_random = 0
    while used < BUDGET and n_random < 260:
        na = rng.randint(1, 200000)
        nb = rng.randint(1, 200000)
        if used + na + nb > BUDGET:
            room = BUDGET - used
            if room < 4:
                break
            half = max(1, room // 2)
            na = rng.randint(1, half)
            nb = room - na
            if nb < 1:
                break
        a = rand_hex(rng, na, prob in ('add', 'mul'))
        b = rand_hex(rng, nb, prob in ('add', 'mul'))
        if prob == 'div':
            a = a.lstrip('-') or '0'
            b = b.lstrip('-') or '1'
            if int(b, 16) == 0:
                b = '1'
        cases.append((a, b))
        used += na + nb
        n_random += 1
    write_io(prob, cases, infile, expfile)
    print(f'gen {prob} seed={seed}: {len(cases)} cases (used {used} chars) -> {infile},{expfile}')

def cmd_run(binpath, infile, expfile):
    with open(infile, 'rb') as f:
        inp = f.read()
    with open(expfile, 'rb') as f:
        exp = f.read()
    t0 = time.perf_counter()
    p = subprocess.run([binpath], input=inp, capture_output=True, timeout=600)
    dt = time.perf_counter() - t0
    out = p.stdout
    if p.returncode != 0:
        print(f'RUN FAIL rc={p.returncode} stderr={p.stderr[:500]}')
        return 1
    # compare uppercased (checker uppercases both)
    ok = out.upper() == exp.upper()
    if ok:
        print(f'OK  {binpath}  cases verified, wall={dt*1000:.1f}ms')
        return 0
    # find first mismatch line
    ol = out.decode('utf-8', 'replace').split('\n')
    el = exp.decode('utf-8', 'replace').split('\n')
    bad = 0
    for i in range(min(len(ol), len(el))):
        if ol[i].upper() != el[i].upper():
            bad += 1
            if bad <= 5:
                print(f'  MISMATCH line {i+1}: exp={el[i][:60]!r} got={ol[i][:60]!r}')
    print(f'BAD {binpath}  mismatched_lines={bad} (exp_lines={len(el)} got_lines={len(ol)}) wall={dt*1000:.1f}ms')
    return 1

BENCH_SETS = {
    # name: (per-case digit size for A, ratio for B, repeat count)  -- total <= 3.2M chars
    'max':  None,   # special
    'mid':  (200000, 8),
    'many': (32, 50000),
}

def cmd_genbench(prob, which, benchin):
    """which in {max, mid, many}; total chars kept <= 3.2M (problem constraint)."""
    rng = random.Random(20260810)
    cases = []
    neg = (prob in ('add', 'mul'))
    if which == 'max':
        if prob == 'div':
            cases.append((rand_pos(rng, LOG16), rand_pos(rng, LOG16 // 2)))
        else:
            cases.append((rand_hex(rng, LOG16, neg), rand_hex(rng, LOG16, neg)))
    elif which == 'mid':
        n, cnt = BENCH_SETS['mid']
        for _ in range(cnt):
            if prob == 'div':
                cases.append((rand_pos(rng, n), rand_pos(rng, n // 2)))
            else:
                cases.append((rand_hex(rng, n, neg), rand_hex(rng, n, neg)))
    elif which == 'many':
        n, cnt = BENCH_SETS['many']
        for _ in range(cnt):
            if prob == 'div':
                cases.append((rand_pos(rng, n), rand_pos(rng, rng.randint(1, n))))
            else:
                cases.append((rand_hex(rng, n, neg), rand_hex(rng, n, neg)))
    else:
        print('unknown bench set', which)
        return 2
    tot = sum(len(a) + len(b) for a, b in cases)
    # div 要求 B>0: 任何 B==0 换为 B=1
    if prob == 'div':
        cases = [(a, '1' if b.strip('0') == '' else b) for (a, b) in cases]
    write_io(prob, cases, benchin, benchin + '.exp')
    print(f'genbench {prob}/{which}: {len(cases)} cases, {tot} chars -> {benchin}')
    return 0

def cmd_bench(binpath, benchin, use_cg):
    with open(benchin, 'rb') as f:
        inp = f.read()
    if use_cg:
        env = os.environ.copy()
        cmd = ['valgrind', '--tool=callgrind', '--instr-atstart=yes',
               '--collect-jumps=no', binpath]
        t0 = time.perf_counter()
        p = subprocess.run(cmd, input=inp, capture_output=True, timeout=1200)
        dt = time.perf_counter() - t0
        err = p.stderr.decode('utf-8', 'replace')
        m = re.search(r'I\s+refs:\s*([\d,]+)', err)
        ir = m.group(1) if m else '?'
        print(f'BENCH(cg) {binpath}  Ir={ir}  wall={dt*1000:.0f}ms  rc={p.returncode}')
    else:
        t0 = time.perf_counter()
        p = subprocess.run([binpath], input=inp, capture_output=True, timeout=600)
        dt = time.perf_counter() - t0
        print(f'BENCH(wall) {binpath}  wall={dt*1000:.1f}ms  rc={p.returncode}')

def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 2
    if a[0] == 'gen':
        cmd_gen(a[1], int(a[2]), a[3], a[4])
    elif a[0] == 'genmax':
        cmd_gen(a[1], int(a[2]), a[3], a[4], maxmode=True)
    elif a[0] == 'run':
        return cmd_run(a[1], a[2], a[3])
    elif a[0] == 'genbench':
        return cmd_genbench(a[1], a[2], a[3])
    elif a[0] == 'bench':
        use_cg = '--cg' in a
        cmd_bench(a[1], a[2], use_cg)
    else:
        print('unknown cmd', a[0])
        return 2
    return 0

if __name__ == '__main__':
    sys.exit(main())
