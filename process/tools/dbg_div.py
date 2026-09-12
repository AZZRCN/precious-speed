import os, sys, random
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh

HEX = '0123456789abcdef'
def rand_hex(rng, n):
    s = ''.join(rng.choice(HEX) for _ in range(n))
    if n >= 2:
        s = rng.choice('123456789abcdef') + s[1:]
    return s

def py_div(a, b):
    q, r = divmod(int(a, 16), int(b, 16))
    return format(q, 'x'), format(r, 'x')

def main():
    rng = random.Random(20260813)
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    # match gen_verify div_large shape: na,nb in [2500,3750] limbs, na in [nb,2*nb]
    cases = []
    for _ in range(N):
        nb = rng.randint(2500, 3750)
        na = rng.randint(nb, 2 * nb)
        a = rand_hex(rng, na * 16)
        b = rand_hex(rng, nb * 16)
        if int(a, 16) < int(b, 16):
            a, b = b, a
        cases.append((a, b))
    local_in = os.path.join(os.path.dirname(__file__), "div_in.txt")
    with open(local_in, "w") as f:
        f.write(f"{len(cases)}\n" + "".join(f"{a} {b}\n" for a, b in cases))

    vm_ssh.put(local_in, "/home/azzr/precious_speed/dbg/div_in.txt")
    rc, out, err = vm_ssh.run(
        "cd /home/azzr/precious_speed/hex_best && ./div.bin < /home/azzr/precious_speed/dbg/div_in.txt")
    if rc != 0:
        print("RUN ERROR rc=", rc, err[:300]); return
    got = out.strip().split("\n")
    if len(got) != len(cases):
        print(f"LINE COUNT MISMATCH got={len(got)} cases={len(cases)}"); return
    for i, (a, b) in enumerate(cases):
        eq, er = py_div(a, b)
        g = got[i].strip().lower()
        if g != f"{eq} {er}":
            print(f"MISMATCH idx={i} na={len(a)//16} nb={len(b)//16}")
            print(f"A={a[:80]}...")
            print(f"B={b[:80]}...")
            print(f"GOT={g[:80]}...")
            print(f"EXP={eq[:80]} {er[:80]}...")
            with open(os.path.join(os.path.dirname(__file__), "fail.txt"), "w") as f:
                f.write(f"{a}\n{b}\n")
            return
    print(f"ALL {N} CASES PASS (div_large shape, na<=2nb)")

if __name__ == "__main__":
    main()
