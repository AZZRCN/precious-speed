#!/usr/bin/env python3
# 精确重建 absInvNewton 的 Newton 代数步骤, 定位 bug.
# 已知: absDivBasicCore(REF) 与 REALINV0 均精确正确; 只有 INV (Newton 递推结果) limb0 偏大 1.
# 目标: 重建 C++ 实际算术, 确认 PROD 是否精确, 并定位 INV 偏大的来源.
import sys
B = 65536

def parse_dump(path):
    d = {}
    with open(path) as f:
        for ln in f.read().splitlines():
            if not ln.strip(): continue
            if ln.startswith("K="):
                for tok in ln.split():
                    kk,_,vv = tok.partition("=")
                    if vv: d[kk]=int(vv)
                continue
            key,_,rest = ln.partition("=")
            key=key.strip()
            if key in ("K","S","INV0LEN","PRODSIZE"): d[key]=int(rest.strip())
            else: d[key]=[int(x) for x in rest.strip().split(",") if x!=""]
    return d

def to_big(L):
    v=0
    for i,x in enumerate(L): v+=x*(B**i)
    return v
def to_limbs(v,n):
    return [(v//(B**i))%B for i in range(n)]

def main():
    p=r"D:\precious_speed\hex_best\invnewt_blk3.txt"
    d=parse_dump(p)
    K=d["K"]; S=d["S"]; INV0LEN=d["INV0LEN"]
    M=d["M"]; M_val=to_big(M)
    x0=d["REALINV0"]            # 精确: floor(B^96 / M_high), M_high=M[S..]
    PROD=d["PROD"]; INV=d["INV"]
    M_high=M[S:]                # 长度 K-S = 48
    M_high_val=to_big(M_high)
    x0_val=to_big(x0)          # 精确值
    # 校验 x0 精确
    assert x0_val == (B**(2*(K-S)))//M_high_val, "x0 不精确!"

    # ---- 1) PROD = x0^2 * M 是否精确? ----
    P_exact = x0_val * x0_val * M_val
    P_limbs = to_limbs(P_exact, len(PROD))
    prod_mismatch = [i for i in range(len(PROD)) if PROD[i]!=P_limbs[i]]
    print(f"[1] PROD(=x0^2*M) vs 精确: {'完全一致' if not prod_mismatch else 'MISMATCH@'+str(prod_mismatch[:5])}")
    P_len = P_exact.bit_length() and (P_exact>0)
    nP = (P_exact.bit_length()+15)//16
    print(f"    P_exact 实际有效 limb 数 = {nP}, dump PROD 长度 = {len(PROD)}")

    # ---- 2) 重建 C++ 的 inv 计算 ----
    # INV2: 2*x0 放在 offset s=47, 缓冲 inv2_size=k+1=96 -> 值 = 2*x0 * B^47
    INV2_big = 2 * x0_val * (B**S)
    # highP: prod_span = prod + 2*(k-s) 偏移, size-- -> floor(P/B^(2*(k-s))) 取低 96 limb
    shift = 2*(K-S)
    highP_full = P_exact // (B**shift)
    highP_used = highP_full % (B**(K+1))   # C++ 取低 96 limb (size-- 后 96 limb)
    print(f"[2] shift=2*(K-S)={shift}; highP_full 实际 limb 数 = {(highP_full.bit_length()+15)//16}")
    print(f"    highP_full 顶 limb 是否非零(被 size-- 丢弃)? limb{(highP_full.bit_length()+15)//16 -1} = {to_limbs(highP_full, (highP_full.bit_length()+15)//16)[-1] if highP_full>0 else 0}")
    # C++ 实际: inv = INV2 - highP_used (96 limb absSub)
    x_cpp_big = INV2_big - highP_used
    x_cpp_limbs = to_limbs(x_cpp_big, K+1)
    inv_match = [i for i in range(K+1) if INV[i]!=x_cpp_limbs[i]]
    print(f"    重建 x_cpp limb0..95 vs INV dump: {'完全一致' if not inv_match else 'MISMATCH@'+str(inv_match[:5])}")
    if not inv_match:
        print("    >> 重建算术与 C++ INV 完全一致 -> bug 在 Newton 公式/代数本身, 不在 mul/sub")

    # ---- 3) 与真值比较 ----
    Q_exact = (B**(2*K))//M_val
    print(f"\n[3] 真值 Q_exact[0]={to_limbs(Q_exact,K+1)[0]}  x_cpp[0]={x_cpp_limbs[0]}  INV[0]={INV[0]}")
    print(f"    x_cpp == Q_exact ? {x_cpp_big==Q_exact}")
    print(f"    x_cpp - Q_exact = {x_cpp_big - Q_exact}  (若正好 +1 说明整个 96-limb 数偏大 1)")
    # 残余: x*M vs B^(2k)
    rem_cpp = (B**(2*K)) - x_cpp_big*M_val
    rem_true = (B**(2*K)) - Q_exact*M_val
    print(f"    x_cpp*M  与 B^{2*K} 关系: x_cpp*M {'>=' if x_cpp_big*M_val>=(B**(2*K)) else '<'} B^{2*K}  (余={'负' if rem_cpp<0 else '正'}{abs(rem_cpp)})")
    print(f"    Q_exact*M <= B^{2*K} ? {Q_exact*M_val <= (B**(2*K))}")

    # ---- 4) 修正: 若 x*M >= B^(2k) 则 x-- (GMP 式校正) ----
    x_corr = x_cpp_big
    cnt=0
    while x_corr*M_val >= (B**(2*K)):
        x_corr -= 1; cnt+=1
    print(f"\n[4] GMP 式校正(while x*M>=B^2k: x--): 减了 {cnt} 次, 得到 x_corr[0]={to_limbs(x_corr,K+1)[0]}")
    print(f"    x_corr == Q_exact(真值) ? {x_corr==Q_exact}")

    # ---- 5) 看 size-- 丢弃的 limb: 若 highP_full 有第 96 limb(即>=B^96 位), size-- 是否丢信息 ----
    # highP_used 是 highP_full 的低 96 limb; 若 highP_full >= B^96 则 size-- 正确丢弃高位(那些高位在 INV2 中也超出 96 limb)
    # 检查 INV2_big 是否 <= B^? : INV2 = 2*x0*B^47, x0<2*B^48 -> INV2 < 2*B^95 < B^96 对 96 limb 而言溢出?
    INV2_limbs = to_limbs(INV2_big, K+2)
    print(f"[5] INV2(=2*x0*B^{S}) limb 数 = {sum(1 for _ in range(K+2))} 有效 limb = {max(i for i in range(K+2) if INV2_limbs[i])+1}")
    print(f"    INV2[95]={INV2_limbs[95]} INV2[96]={INV2_limbs[96] if K+1<len(INV2_limbs) else 'n/a'}")

if __name__=="__main__":
    main()
