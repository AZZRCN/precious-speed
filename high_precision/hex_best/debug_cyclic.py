import random
from sim_cyclic_div import (B, limbs_to_int, int_to_limbs, fft_ceil_cycm,
                            unwrap, r_based_cyclic)

def dbg_case(len2, qn, in_sz, a_int, b_int, dbg=True):
    len1 = len2 + qn
    a = int_to_limbs(a_int, len1)
    b = int_to_limbs(b_int, len2)
    dividend = a[:]
    quotient = [0]*qn
    qn_remaining = qn
    inv_extra = 2 if (in_sz + 2 <= len2) else 0
    dh_full = b[len2-(in_sz+inv_extra):] if inv_extra else b[len2-in_sz:]
    inv_full_int = (B**(2*(in_sz+inv_extra)))//limbs_to_int(dh_full)
    inv_span_int = inv_full_int >> (inv_extra*16)
    blk = 0
    while qn_remaining > 0:
        this_in = min(in_sz, qn_remaining)
        window = dividend[qn_remaining-this_in: qn_remaining+len2]
        win_int = limbs_to_int(window)
        d_int = limbs_to_int(b)
        true_qb = win_int // d_int
        divid_high_int = limbs_to_int(window[len2-1: len2+this_in])
        qhat_full_int = divid_high_int * inv_span_int
        qhat_int = (qhat_full_int >> ((in_sz+1)*16)) & ((B**(this_in+1))-1)
        cyclic_m = max(fft_ceil_cycm(len2+1), fft_ceil_cycm((len2+in_sz)//2+1))
        use_cyclic = (cyclic_m < in_sz+len2)
        if use_cyclic:
            tprod = unwrap(len2, this_in, b, qhat_int, cyclic_m, window)
            qhat_arr = int_to_limbs(qhat_int, this_in+1)
            qhat_arr, rem, corr, r = r_based_cyclic(window, b, tprod, qhat_arr, this_in, len2)
            cyc_qb = limbs_to_int(qhat_arr)
            if dbg:
                import sim_cyclic_div as S
                P = qhat_int * d_int
                Plow = int_to_limbs(P, cyclic_m)
                C = int_to_limbs(P % (B**cyclic_m - 1), cyclic_m)
                print(f"  block{blk}: len2={len2} this_in={this_in} in={in_sz} cyclic_m={cyclic_m} wn={len2+this_in-cyclic_m} use_cyc={use_cyclic}")
                print(f"    qhat_pre={qhat_int} true_qb={true_qb} cyc_qb={cyc_qb} r={r} corr={corr}")
                print(f"    qhat_pre - true_qb = {qhat_int - true_qb}")
                print(f"    P[59]={int_to_limbs(P, len2+this_in)[59]}  tp[59]={tprod[59]}")
                print(f"    window[59]={window[59]} window[0:2]={window[0:2]}")
                rem_exact = (win_int - qhat_int*d_int)
                print(f"    rem_exact(qhat_pre)={int_to_limbs(rem_exact, len2)[:3]}  rem_cyc={rem[:3] if rem else None}")
                print(f"    cmp(rem_cyc,div)={S.cmp_at(rem,0,b,0,len2)}")
                # inverse analysis
                dh_full = b[len2-(in_sz+inv_extra):] if inv_extra else b[len2-in_sz:]
                dh_int = S.limbs_to_int(dh_full)
                inv_full_int = (B**(2*(in_sz+inv_extra)))//dh_int
                inv_span_int = inv_full_int >> (inv_extra*16)
                print(f"    inv_extra={inv_extra} d_high limbs={len(dh_full)} inv len={len(S.int_to_limbs(inv_span_int, in_sz+1+inv_extra))}")
                print(f"    inv_span_int low3={S.int_to_limbs(inv_span_int, 3)}")
            if cyc_qb != true_qb:
                if dbg:
                    print(f"    *** MISMATCH (cyc={cyc_qb} true={true_qb})")
                return False, blk
        else:
            cyc_qb = true_qb
        for i in range(this_in):
            quotient[qn_remaining-this_in+i] = int_to_limbs(cyc_qb, this_in)[i]
        rem_int = win_int - true_qb*d_int
        rem = int_to_limbs(rem_int, len2)
        for i in range(len2):
            dividend[qn_remaining-this_in+i] = rem[i] if i < len(rem) else 0
        qn_remaining -= this_in
        blk += 1
    q_true = int_to_limbs(a_int // b_int, qn)
    return (quotient == q_true), blk

def run(seed=0, N=2000):
    random.seed(seed)
    for t in range(N):
        len2 = random.randint(8,300)
        qn = random.randint(1,200)
        len1 = len2+qn
        in_sz = random.randint(max(1,len2//4), len2//2)
        a_int = random.getrandbits(len1*16)
        if a_int < (B**len2): a_int += B**len2
        b_int = random.getrandbits(len2*16)
        if b_int < (B**(len2-1)): b_int += B**(len2-1)
        # quick check
        okq = dbg_case(len2, qn, in_sz, a_int, b_int, dbg=False)
        if not okq[0]:
            print(f"FAIL t={t} dims=(len2={len2},qn={qn},in={in_sz})")
            dbg_case(len2, qn, in_sz, a_int, b_int, dbg=True)
            return

if __name__ == "__main__":
    run(0)
