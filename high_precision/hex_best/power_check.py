#!/usr/bin/env python3
# 定位 power gen 线性路径首个失败 case, 打印 A/B 与 bin/oracle 的 q,r 偏差.
import subprocess, sys, os

GENBIN = "./genbin"
BIN = "./div_base16_zn"

def gen_cases(gen, seed):
    p = subprocess.run([f"{GENBIN}/{gen}", str(seed)], capture_output=True, text=True)
    lines = p.stdout.strip().split("\n")
    T = int(lines[0])
    cases = []
    for i in range(1, min(T, 4000) + 1):
        a, b = lines[i].split()
        cases.append((a, b))
    return cases

def run_bin(a, b):
    inp = f"1\n{a} {b}\n"
    p = subprocess.run([BIN], input=inp, capture_output=True, text=True)
    out = p.stdout.strip().split("\n")
    if len(out) < 1 or " " not in out[0]:
        return None, None
    q, r = out[0].split()
    return q, r

def to_hex(n):
    h = format(n, "X")
    return h

def main():
    gen = sys.argv[1] if len(sys.argv) > 1 else "power"
    seed = sys.argv[2] if len(sys.argv) > 2 else "1"
    cases = gen_cases(gen, seed)
    print(f"GEN={gen} SEED={seed} T={len(cases)}")
    bad = 0
    dumped = 0
    for idx, (a, b) in enumerate(cases):
        A = int(a, 16); B = int(b, 16)
        qp = abs(A) // abs(B)
        if (A < 0) != (B < 0):
            qp = -qp
        rp = A - qp * B
        qb, rb = run_bin(a, b)
        if qb is None:
            print(f"  [{idx}] BIN_PARSE_FAIL A={a[:20]}.. B={b[:20]}.."); bad += 1; dumped += 1
            if dumped >= 5: break
            continue
        qb_i = int(qb, 16); rb_i = int(rb, 16)
        qok = (qb_i == qp); rok = (rb_i == rp)
        if not qok or not rok:
            bad += 1
            if dumped < 6:
                print(f"  [{idx}] lenA={len(a)} lenB={len(b)} len2_limbs={(len(b)+3)//4}")
                print(f"        q_bin={qb[:24]}{'..' if len(qb)>24 else ''}  q_py={to_hex(qp)[:24]}{'..' if len(to_hex(qp))>24 else ''}  qok={qok}")
                print(f"        r_bin={rb}  r_py={to_hex(rp)}  rok={rok}")
                dumped += 1
    print(f"SUMMARY bad={bad}/{len(cases)}")

if __name__ == "__main__":
    main()
