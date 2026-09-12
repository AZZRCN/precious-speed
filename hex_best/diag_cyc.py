import subprocess, random, sys, os
sys.set_int_max_str_digits(200000000)
EXE = os.environ.get("EXE", r"D:\precious_speed\hex_best\div_base16_cyc.exe")
BASE = 1 << 16

def run(A, B):
    inp = "1\n" + format(A, 'X') + " " + format(B, 'X') + "\n"
    r = subprocess.run([EXE], input=inp, capture_output=True, text=True)
    if r.returncode != 0:
        return None, None, r.stderr[:400]
    p = r.stdout.strip().split()
    return int(p[0], 16), int(p[1], 16), ""

def limbs(x):
    if x == 0: return [0]
    out = []
    while x: out.append(x % BASE); x //= BASE
    return out

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    lb = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    random.seed(seed)
    B = random.getrandbits(4 * lb) | (1 << (4 * lb - 1)) | 1
    la = int(round(lb * ratio)); la += (4 - la % 4) % 4
    A = random.getrandbits(4 * la) | (1 << (4 * la - 1))
    qg, rg, err = run(A, B)
    if qg is None:
        print("RUN ERR:", err); return
    qt = A // B; rt = A % B
    qg_l = limbs(qg); qt_l = limbs(qt)
    n = max(len(qg_l), len(qt_l))
    print(f"lb_hex={lb} la_hex={la} nq_limbs={n}")
    print(f"r_in_B={0 <= rg < B} rg_ok={rg == rt}")
    # first differing limb from LOW end
    diffs = []
    for i in range(n):
        a = qg_l[i] if i < len(qg_l) else 0
        b = qt_l[i] if i < len(qt_l) else 0
        if a != b: diffs.append((i, a, b, b - a))
    print(f"# differing limbs = {len(diffs)} (of {n})")
    if diffs:
        # show first 8 from low end
        for i, a, b, d in diffs[:8]:
            print(f"  limb[{i}] got={a} exp={b} delta={d}")
        print(f"  lowest diff limb idx = {diffs[0][0]}, highest = {diffs[-1][0]}")
    else:
        print("  q identical!")

if __name__ == "__main__":
    main()
