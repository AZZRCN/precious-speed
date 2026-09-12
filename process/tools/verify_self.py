#!/usr/bin/env python3
# verify_self.py - 自包含正确性校验: 跑二进制对 cases_hex/<g>.in, 逐组校验 Q*B+R==A 且 0<=R<B.
# 用法: verify_self.py <bin> <case_dir> [groups...]
import sys, subprocess, os, glob

def parse_cases(path):
    with open(path) as f:
        data = f.read().split('\n')
    T = int(data[0].strip())
    cases = []
    for i in range(1, 1 + T):
        a, b = data[i].split()
        cases.append((a, b))
    return cases

def verify_group(binpath, path):
    cases = parse_cases(path)
    inp = open(path).read()
    r = subprocess.run([binpath], input=inp, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        return (path, len(cases), -1, f"CRASH rc={r.returncode} err={r.stderr[:200]}")
    out = r.stdout.strip().split('\n')
    if len(out) != len(cases):
        return (path, len(cases), -1, f"LINECOUNT got={len(out)} exp={len(cases)}")
    fails = 0
    fail_examples = []
    for idx, (ah, bh) in enumerate(cases):
        qh, rh = out[idx].split()
        A = int(ah, 16); B = int(bh, 16); Q = int(qh, 16); R = int(rh, 16)
        ok = (Q * B + R == A) and (0 <= R < B)
        if not ok:
            fails += 1
            if len(fail_examples) < 3:
                fail_examples.append(f"#{idx} A_limbs={len(ah)//16} B_limbs={len(bh)//16} "
                                      f"QB+R==A={Q*B+R==A} 0<=R<B={0<=R<B}")
    return (path, len(cases), fails, "; ".join(fail_examples))

def main():
    binp = sys.argv[1]
    d = sys.argv[2]
    groups = sys.argv[3:]
    if not groups:
        groups = sorted(g for g in (os.path.basename(p)[:-3] for p in glob.glob(os.path.join(d, '*.in'))))
    total_fail = 0
    for g in groups:
        p = os.path.join(d, g + '.in')
        if not os.path.exists(p):
            print(f"{g:18s} MISSING"); continue
        name, n, fails, msg = verify_group(binp, p)
        tag = "OK " if fails == 0 else "FAIL"
        print(f"{g:18s} {tag} cases={n} fails={fails} {msg}")
        total_fail += (fails if fails > 0 else 0)
    print(f"=== TOTAL_FAILS={total_fail} ===")
    sys.exit(1 if total_fail else 0)

if __name__ == '__main__':
    main()
