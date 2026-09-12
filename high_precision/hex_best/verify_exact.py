#!/usr/bin/env python3
# 精确真值判定: 用 Python 大整数直接算 Newton 逆的精确数学值,
# 与 dump 的 REF(=absDivBasicCore 直接算) / INV(=Newton 递推) / REALINV0 逐 limb 比对.
# BASE = 65536, Limb = uint32.
import sys

B = 65536

def parse_dump(path):
    d = {}
    with open(path) as f:
        lines = f.read().splitlines()
    for ln in lines:
        if not ln.strip():
            continue
        if ln.startswith("K="):
            # 头部多键值行: K=95 S=47 INV0LEN=49 PRODSIZE=193
            for tok in ln.split():
                kk, _, vv = tok.partition("=")
                if vv:
                    d[kk] = int(vv)
            continue
        key, _, rest = ln.partition("=")
        key = key.strip()
        if key in ("K","S","INV0LEN","PRODSIZE"):
            d[key] = int(rest.strip())
        else:
            vals = [int(x) for x in rest.strip().split(",") if x != ""]
            d[key] = vals
    return d

def to_big(limbs):
    v = 0
    for i, x in enumerate(limbs):
        v += x * (B ** i)
    return v

def to_limbs(v, n):
    L = []
    for i in range(n):
        L.append(v % B)
        v //= B
    return L

def main():
    p = r"D:\precious_speed\hex_best\invnewt_blk3.txt"
    d = parse_dump(p)
    K = d["K"]; S = d["S"]; INV0LEN = d["INV0LEN"]
    M = d["M"]
    assert len(M) == K, f"len(M)={len(M)} != K={K}"
    # M 已归一化? 顶端 limb
    print(f"K={K} S={S} INV0LEN={INV0LEN} len(M)={len(M)}")
    print(f"M top limb = {M[K-1]} (归一化需 >= HALF_BASE=32768): {'OK' if M[K-1]>=32768 else 'NOT NORMALIZED'}")
    print(f"M[0]={M[0]} (偶数? {M[0]%2==0})")

    M_val = to_big(M)
    # 精确真值: floor(B^(2K) / M)
    Q_exact = (B ** (2*K)) // M_val
    Q_limbs = to_limbs(Q_exact, 2*K + 1)
    # 截断到 k+1 = 96 limb 报告 (与 REF/INV dump 对齐, 它们打印 0..k=95)
    print(f"\n=== 精确 floor(B^{2*K}/M) limb[0..{K}] ===")
    print("Q_exact[0..95] =", Q_limbs[:K+1])

    REF = d["REF"]
    INV = d["INV"]
    # 比对 limb 0..K
    diff_ref = [i for i in range(K+1) if REF[i] != Q_limbs[i]]
    diff_inv = [i for i in range(K+1) if INV[i] != Q_limbs[i]]
    print(f"\nREF vs Q_exact: 不同 limb = {diff_ref if diff_ref else 'NONE (完全一致)'}")
    print(f"INV vs Q_exact: 不同 limb = {diff_inv if diff_inv else 'NONE (完全一致)'}")
    print(f"REF[0]={REF[0]}  INV[0]={INV[0]}  Q_exact[0]={Q_limbs[0]}")
    if not diff_ref:
        print(">> REF (absDivBasicCore) 完全等于精确真值 -> absDivBasicCore 正确, Newton 结果 INV 错")
    elif not diff_inv:
        print(">> INV (Newton 递推) 完全等于精确真值 -> absDivBasicCore(REF) 有 off-by-one 类 bug")
    else:
        print(">> REF 与 INV 都偏离真值, 需进一步定位")

    # REALINV0 = floor(B^(2*(K-S)) / M_high) 其中 M_high = M[S..K-1] (INV0LEN = K-S+1 limb)
    M_high = M[S:]          # 长度 K-S = 48
    M_high_val = to_big(M_high)
    Q0_exact = (B ** (2*(K-S))) // M_high_val
    Q0_limbs = to_limbs(Q0_exact, 2*(K-S)+1)
    REALINV0 = d["REALINV0"]
    # REALINV0 长度 INV0LEN=49, 报告 0..INV0LEN-1
    diff_r = [i for i in range(INV0LEN) if REALINV0[i] != Q0_limbs[i]]
    print(f"\n=== REALINV0 = floor(B^{2*(K-S)}/M_high) 比对 ===")
    print(f"REALINV0[0..{INV0LEN-1}] = {REALINV0}")
    print(f"Q0_exact [0..{INV0LEN-1}] = {Q0_limbs[:INV0LEN]}")
    print(f"REALINV0 vs Q0_exact: 不同 limb = {diff_r if diff_r else 'NONE (inv0 正确)'}")
    if not diff_r:
        print(">> inv0 (REALINV0) 完全等于精确值 -> Newton 基例输入正确, 故 INV 应精确")

    # 余数校验: B^(2K) == Q*M + R, 0<=R<M ?
    R = (B ** (2*K)) - Q_exact * M_val
    print(f"\n余数校验: R < M ? {R < M_val} ; R==0 ? {R==0}")
    # 看 REF 对应的余数
    Qref = to_big(REF[:K+1])
    Rref = (B ** (2*K)) - Qref * M_val
    print(f"REF 余数: Rref < M ? {Rref < M_val} ; Rref==0 ? {Rref==0} ; Rref[0..3]={to_limbs(Rref,8)[:4]}")
    Qin = to_big(INV[:K+1])
    Rin = (B ** (2*K)) - Qin * M_val
    print(f"INV 余数: Rin < M ? {Rin < M_val} ; Rin==0 ? {Rin==0} ; Rin[0..3]={to_limbs(Rin,8)[:4]}")

if __name__ == "__main__":
    main()
