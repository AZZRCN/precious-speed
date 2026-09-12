"""统计 DIV 各测试点的除法形状 (qn x dn) 与 absDivRem 分支代价分布。
分支条件取自 div_D39.cpp @5786:
    len2<=64 || (len1-len2)<=64  -> absDivBasicCore  (schoolbook, O(qn*dn))
    len1 < len2*2                -> absDivNewtonCore1
    else                         -> mu_div_qr Newton
"""
import os
import sys

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases", "div")


def L(d):  # decimal digits -> limbs (base 1e4)
    return (d + 3) // 4


def analyze(fn):
    p = os.path.join(D, fn)
    with open(p) as f:
        n = int(f.readline())
        rows = [f.readline().split() for _ in range(n)]
    sb_cost = 0
    sb_n = nc1_n = mu_n = 0
    sb_shapes = []
    for a, b in rows:
        na, nb = a.lstrip('-'), b.lstrip('-')
        l1, l2 = L(len(na)), L(len(nb))
        if l1 < l2:
            continue
        q = l1 - l2
        if l2 <= 64 or q <= 64:
            sb_n += 1
            c = q * l2
            sb_cost += c
            sb_shapes.append((q, l2, c))
        elif l1 < l2 * 2:
            nc1_n += 1
        else:
            mu_n += 1
    sb_shapes.sort(key=lambda t: -t[2])
    tot = sb_n + nc1_n + mu_n
    print(f"--- {fn}  groups={n} usable={tot}")
    print(f"    schoolbook: {sb_n:6d} ({100*sb_n/max(tot,1):5.1f}%)  cost={sb_cost/1e6:8.2f}M limb-op")
    print(f"    newton1   : {nc1_n:6d} ({100*nc1_n/max(tot,1):5.1f}%)")
    print(f"    mu_div    : {mu_n:6d} ({100*mu_n/max(tot,1):5.1f}%)")
    if sb_shapes:
        print("    top sb (qn x dn = cost):",
              ", ".join(f"{q}x{d}={c}" for q, d, c in sb_shapes[:6]))
        g_sd = sum(c for q, d, c in sb_shapes if d <= 64)
        g_sq = sum(c for q, d, c in sb_shapes if d > 64 and q <= 64)
        print(f"    cost by gate: dn<=64 -> {g_sd/1e6:7.2f}M ({100*g_sd/max(sb_cost,1):3.0f}%)"
              f" | qn<=64&dn>64 -> {g_sq/1e6:7.2f}M ({100*g_sq/max(sb_cost,1):3.0f}%)")
        # ---- absSubMul1 效率剖分 ----
        # 每次 absDivBasicCore 迭代调用 1 次 absSubMul1(acc, in=divisor(dn), x)
        calls = sum(q for q, d, c in sb_shapes)          # 调用次数
        vec_limb = sum(q * (d // 16 * 16) for q, d, c in sb_shapes)  # AVX2 覆盖
        tail_limb = sum(q * (d % 16) for q, d, c in sb_shapes)       # 标量尾部
        scalar_all = sum(q * d for q, d, c in sb_shapes if d < 16)   # 全标量(dn<16)
        # Ir 模型: AVX2 5.25/limb, 标量 20/limb, 每次调用常量建立 15
        ir_vec = vec_limb * 5.25
        ir_tail = tail_limb * 20.0
        ir_call = calls * 15.0
        ir_iter = calls * 30.0   # qhat 估计 + Knuth D3 + 循环控制
        ir_tot = ir_vec + ir_tail + ir_call + ir_iter
        print(f"    absSubMul1 calls={calls}  vec_limb={vec_limb/1e6:.2f}M  tail_limb={tail_limb/1e6:.2f}M"
              f"  (full-scalar dn<16: {scalar_all/1e6:.2f}M)")
        print(f"    Ir model: vec={ir_vec/1e6:6.1f}M({100*ir_vec/ir_tot:4.1f}%)"
              f" tail={ir_tail/1e6:6.1f}M({100*ir_tail/ir_tot:4.1f}%)"
              f" callconst={ir_call/1e6:5.1f}M({100*ir_call/ir_tot:4.1f}%)"
              f" iter={ir_iter/1e6:5.1f}M({100*ir_iter/ir_tot:4.1f}%)"
              f"  TOTAL={ir_tot/1e6:.1f}M")
    return sb_cost


if __name__ == "__main__":
    files = sys.argv[1:] or sorted(f for f in os.listdir(D) if f.endswith(".in"))
    total = 0
    for fn in files:
        try:
            total += analyze(fn)
        except Exception as e:  # noqa: BLE001
            print(fn, "ERR", e)
    print(f"=== TOTAL schoolbook cost = {total/1e6:.2f}M limb-op")
