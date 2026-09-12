#!/usr/bin/env python3
# gen_verify.py - generate LC-style test inputs, run HEX binaries on the Intel VM
# (PROXY), verify correctness against a local Python bigint reference, and
# collect AMD-Zen3-semantic perf profiles on the largest cases.
#
# CROSS-ARCH RULE: VM is Intel. perf = proxy only. Final tuning MUST be
# confirmed by LC (AMD EPYC 7B13) submission receipts.
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh
import vm_profile

WS = "/home/azzr/precious_speed"
HEX = WS + "/hex_best"
CASES = WS + "/cases"

HEX_ALPHA = '0123456789ABCDEF'
HEX_NONZERO = '123456789ABCDEF'

def rand_hex(rng, n, neg=False):
    if n <= 0:
        return '0'
    first = rng.choice(HEX_NONZERO) if n > 1 else rng.choice(HEX_NONZERO)
    rest = ''.join(rng.choice(HEX_ALPHA) for _ in range(n - 1))
    s = first + rest
    return ('-' + s) if neg else s

def make_input(rng, T, sa, sb, nega=False, negb=False):
    L = [str(T)]
    for _ in range(T):
        a = rand_hex(rng, rng.randint(sa[0], sa[1]), nega)
        b = rand_hex(rng, rng.randint(sb[0], sb[1]), negb)
        L.append(f"{a} {b}")
    return "\n".join(L) + "\n"

def make_div_input(rng, T, gen):
    L = [str(T)]
    for _ in range(T):
        a, b = gen(rng)
        L.append(f"{a} {b}")
    return "\n".join(L) + "\n"

def compute_ref(inp, op):
    toks = inp.split()
    T = int(toks[0]); i = 1; out = []
    for _ in range(T):
        a = int(toks[i], 16); b = int(toks[i + 1], 16); i += 2
        if op == 'add':
            out.append(f"{a + b:x}")
        elif op == 'mul':
            out.append(f"{a * b:x}")
        elif op == 'div':
            q, r = divmod(a, b)
            out.append(f"{q:x} {r:x}")
    return "\n".join(out) + "\n"

def verify_case(op, name, inp, do_profile=False):
    in_file = f"{CASES}/{op}_{name}.in"
    hexout = f"{CASES}/{op}_{name}.hexout"
    local_in = f"/tmp/{op}_{name}.in"
    with open(local_in, 'wb') as f:
        f.write(inp.encode())
    vm_ssh.put(local_in, in_file)
    vm_ssh.run(f"cd {HEX} && ./{op}.bin < {in_file} > {hexout} 2>/dev/null")
    local_out = f"/tmp/{op}_{name}.hexout"
    vm_ssh.get(hexout, local_out)
    with open(local_out) as f:
        got = f.read()
    exp = compute_ref(inp, op)
    got_l = "\n".join(l.strip().lower() for l in got.splitlines())
    exp_l = "\n".join(l.strip().lower() for l in exp.splitlines())
    ok = (got_l == exp_l)
    prof = None
    if do_profile:
        prof = vm_profile.profile(f"hex_best/{op}.bin", in_file)
    return ok, len(inp), prof

# (op, name, generator, profile_this_one)
PLAN = [
    ('add', 'small',  lambda r: make_input(r, 2000, (1, 32), (1, 32), True, True), False),
    ('add', 'medium', lambda r: make_input(r, 50, (500, 1500), (500, 1500), True, True), False),
    ('add', 'large',  lambda r: make_input(r, 8, (40000, 60000), (40000, 60000), True, True), True),
    ('mul', 'small',  lambda r: make_input(r, 2000, (1, 32), (1, 32), True, True), False),
    ('mul', 'medium', lambda r: make_input(r, 50, (500, 1500), (500, 1500), True, True), False),
    ('mul', 'large',  lambda r: make_input(r, 8, (40000, 60000), (40000, 60000), True, True), False),
    ('mul', 'max',    lambda r: make_input(r, 2, (100000, 100000), (100000, 100000), False, False), True),
    ('div', 'small',  lambda r: make_input(r, 2000, (1, 32), (1, 32), False, False), False),
    ('div', 'medium', lambda r: make_input(r, 50, (500, 1500), (500, 1500), False, False), False),
    ('div', 'large',  lambda r: make_input(r, 8, (40000, 60000), (40000, 60000), False, False), False),
    ('div', 'max',    lambda r: make_input(r, 3, (100000, 100000), (100000, 100000), False, False), False),
    ('div', 'a_max_b_random', lambda r: make_div_input(r, 10,
        lambda r: (rand_hex(r, r.randint(60000, 80000), False), rand_hex(r, r.randint(100, 500), False))), False),
    ('div', 'length_ratio', lambda r: make_div_input(r, 10,
        lambda r: (rand_hex(r, r.randint(8000, 12000) * r.randint(3, 8), False),
                   rand_hex(r, r.randint(8000, 12000), False))), True),
]

def main():
    vm_ssh.run(f"mkdir -p {CASES}")
    print(f"{'case':22} {'bytes':>10} {'result':>7}   inst      cache_miss   L1miss%   IPC")
    print("-" * 70)
    for op, name, gen, prof in PLAN:
        rng = random.Random((hash((op, name)) & 0xffffffff) ^ 0x9E3779B9)
        inp = gen(rng)
        ok, size, p = verify_case(op, name, inp, prof)
        if p:
            print(f"{op+'_'+name:22} {size:>10} {'PASS' if ok else 'FAIL':>7}   "
                  f"{p.get('instructions',0):>8} {p.get('cache_miss',0):>10}   "
                  f"{p.get('L1_miss_rate',0):>6}   {p.get('IPC',0)}")
        else:
            print(f"{op+'_'+name:22} {size:>10} {'PASS' if ok else 'FAIL':>7}")
    print("done")

if __name__ == "__main__":
    main()
