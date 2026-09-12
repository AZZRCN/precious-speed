#!/usr/bin/env python3
# 生成单个随机 hex 除法用例, 运行 EXE, 与 Python 大整数对拍; 失败则 dump 已写入 /tmp/muinv_blk0.txt
import sys, subprocess, random

def rand_hex_int(n_limbs, rng):
    # 生成 n_limbs 个 base-65536 limb (little-endian 内存序), 返回 hex 串(大写, 高位在前)
    limbs = [rng.randrange(65536) for _ in range(n_limbs)]
    limbs[-1] |= 0x8000  # 保证最高 limb 非零 (归一化近似)
    val = 0
    for v in reversed(limbs):
        val = val * 65536 + v
    return format(val, 'X')

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    lb = int(sys.argv[2]) if len(sys.argv) > 2 else 200   # divisor limbs
    ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    # 强制 divisor 比 dividend 短, 保证商非零
    la = max(lb + 1, int(lb * ratio))
    rng = random.Random(seed)
    B = int(rand_hex_int(lb, rng), 16)
    A = int(rand_hex_int(la, rng), 16)
    # 确保 A >= B 且 B 最高 limb >= 32768 (HALF_BASE 归一化)
    if A < B:
        A, B = B, A
    while (B >> ((lb - 1) * 16)) < 32768:
        B |= (1 << ((lb - 1) * 16))
    hexA = format(A, 'X'); hexB = format(B, 'X')
    inp = f"1\n{hexA} {hexB}\n"
    exe = sys.argv[4] if len(sys.argv) > 4 else "D:/precious_speed/hex_best/div_base16_invchk.exe"
    p = subprocess.run(exe, input=inp, capture_output=True, text=True, timeout=120)
    out = p.stdout.strip()
    print("STDERR:", p.stderr.strip().splitlines()[:5])
    if not out:
        print("NO OUTPUT (rc=%d)" % p.returncode); print(p.stderr[:500]); return
    got_q, got_r = out.split()
    gq = int(got_q, 16); gr = int(got_r, 16)
    eq = A // B; er = A % B
    ok = (gq == eq and gr == er)
    print(f"seed={seed} lb={lb} la={la} ratio={ratio} OK={ok}")
    print(f"  got_q={got_q[:40]}... exp_q={format(eq,'X')[:40]}...")
    print(f"  got_r={got_r[:40]}... exp_r={format(er,'X')[:40]}...")
    if not ok:
        print(f"  A={hexA[:60]}...")
        print(f"  B={hexB[:60]}...")
        print("  FAIL -> 请读 /tmp/muinv_blk0.txt")

if __name__ == "__main__":
    main()
