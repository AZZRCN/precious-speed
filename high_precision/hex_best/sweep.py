#!/usr/bin/env python3
# 扫描多尺寸/多 seed, 找第一个 FAIL 的用例, 落盘 case.txt 供 invchk dump block0
import sys, subprocess, random

def rand_limbs(n, rng):
    limbs = [rng.randrange(65536) for _ in range(n)]
    limbs[-1] |= 0x8000
    val = 0
    for v in reversed(limbs):
        val = val * 65536 + v
    return val

def gen_case(seed, lb, ratio, rng):
    la = max(lb + 1, int(lb * ratio))
    B = rand_limbs(lb, rng)
    A = rand_limbs(la, rng)
    if A < B: A, B = B, A
    while (B >> ((lb-1)*16)) < 32768:
        B |= (1 << ((lb-1)*16))
    return A, B

EXE = "D:/precious_speed/hex_best/div_base16_cyc.exe"

def run(A, B):
    inp = f"1\n{format(A,'X')} {format(B,'X')}\n"
    p = subprocess.run(EXE, input=inp, capture_output=True, text=True, timeout=180)
    if not p.stdout.strip():
        return None, p.stderr
    gq, gr = p.stdout.split()
    return (int(gq,16), int(gr,16)), p.stderr

def main():
    sizes = [(200,2.0),(500,2.0),(1000,2.0),(2000,2.0),(4096,2.0),(8000,2.0),(32000,2.0),(60000,2.0),(120000,2.0)]
    nseed = int(sys.argv[1]) if len(sys.argv)>1 else 8
    for (lb, ratio) in sizes:
        fails = 0
        for s in range(nseed):
            rng = random.Random(s*1000 + lb)
            A, B = gen_case(s, lb, ratio, rng)
            got, err = run(A, B)
            eq, er = A//B, A%B
            ok = got is not None and got[0]==eq and got[1]==er
            if not ok:
                fails += 1
                if fails == 1:
                    with open("D:/precious_speed/hex_best/case.txt","w") as f:
                        f.write(f"1\n{format(A,'X')} {format(B,'X')}\n")
                    print(f"  >> FIRST FAIL lb={lb} ratio={ratio} seed={s}")
                    print(f"     got={got} err={err.strip().splitlines()[:3]}")
        print(f"lb={lb} ratio={ratio}: {nseed-fails}/{nseed} PASS")
    print("done")

if __name__ == "__main__":
    main()
