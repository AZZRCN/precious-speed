import subprocess, random, os, sys, time
sys.set_int_max_str_digits(200000000)

EXE = os.environ.get("EXE", r"D:\precious_speed\hex_best\div_base16_cyc.exe")

def run_case(A, B):
    inp = "1\n" + format(A, 'X') + " " + format(B, 'X') + "\n"
    t0 = time.time()
    r = subprocess.run([EXE], input=inp, capture_output=True, text=True)
    dt = time.time() - t0
    if r.returncode != 0:
        print("RUN FAILED rc=", r.returncode, r.stderr[:800]); return None, None, dt
    p = r.stdout.strip().split()
    if len(p) < 2:
        print("BAD OUTPUT:", repr(r.stdout[:200])); return None, None, dt
    return int(p[0], 16), int(p[1], 16), dt

def gen(lb_digits, la_ratio, top_nonzero=True):
    assert lb_digits % 4 == 0 and lb_digits > 0
    la_digits = max(lb_digits + 4, int(round(lb_digits * la_ratio)))
    assert la_digits % 4 == 0
    B = random.getrandbits(4 * lb_digits)
    if top_nonzero:
        B |= (1 << (4 * lb_digits - 1))
    B |= 1
    A = random.getrandbits(4 * la_digits)
    A |= (1 << (4 * la_digits - 1))
    return A, B

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    lb = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
    ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    n = int(sys.argv[4]) if len(sys.argv) > 4 else 4
    random.seed(seed)
    fails = 0
    print(f"[scale] lb_hex={lb} limbs={lb//4} ratio={ratio} n={n} seed={seed}")
    for i in range(n):
        A, B = gen(lb, ratio)
        qg, rg, dt = run_case(A, B)
        if qg is None:
            fails += 1; continue
        qok = (qg == A // B)
        rok = (rg == A % B)
        rinb = (0 <= rg < B)
        if not (qok and rok and rinb):
            fails += 1
            print(f"  FAIL#{i} lb={lb//4} limbs q_ok={qok} r_ok={rok} r_in_B={rinb} dt={dt:.2f}s")
            if i < 2:
                tq, tr = A // B, A % B
                qs, ts = format(qg, 'X'), format(tq, 'X')
                for k in range(min(len(qs), len(ts))):
                    if qs[~k] != ts[~k]:
                        a = max(0, ~k - 3); b = ~k + 4
                        print(f"    q last-diff from-end {k}: got={qs[a:b]!r} exp={ts[a:b]!r}")
                        break
        else:
            print(f"  OK#{i} lb={lb//4} limbs dt={dt:.2f}s")
    print(f"[result] FAILS={fails}/{n}")
    sys.exit(1 if fails else 0)

if __name__ == "__main__":
    main()
