import random

def mu_div_ref(A, B, debug=False):
    na = (A.bit_length() + 63)//64
    nb = (B.bit_length() + 63)//64
    top = B >> (64*(nb-1))
    sigma = 64 - top.bit_length()   # _lzcnt_u64 of top limb
    BS = B << sigma
    Z = A << sigma
    in_ = nb//2
    if in_ < 1: in_ = 1
    D_high = BS >> (64*(nb - in_))   # top in_ limbs
    # Newton inverse V = floor(2^(128*in_)/D_high)  (近似, 含 2^(64*in_) 项)
    V = (1 << (128*in_)) // D_high
    q = 0
    qn = na - nb
    qn_rem = qn
    blk = 0
    while qn_rem > 0:
        thi = in_ if in_ <= qn_rem else qn_rem
        wstart = qn_rem - thi
        wtop = qn_rem + nb - 1
        wlen = nb + thi
        window = (Z >> (64*wstart)) & ((1 << (64*wlen)) - 1)
        divid_high = (window >> (64*(nb-1))) & ((1 << (64*(thi+1))) - 1)
        qf = divid_high * V
        qhat = qf >> (64*(in_+1))     # high thi+1 limbs
        product = qhat * BS
        true_qhat = window // BS
        # corr_down
        guard = 0
        while guard < 16:
            prod_gt = False
            if product >> (64*wlen) != 0: prod_gt = True
            if not prod_gt:
                if product > window: prod_gt = True
            if not prod_gt: break
            product -= BS; qhat -= 1; guard += 1
        new_rp = window - product
        # corr_up
        guard = 0
        while guard < 16:
            rp_ge = False
            if new_rp >> (64*nb) != 0: rp_ge = True
            if not rp_ge:
                if (new_rp & ((1<<(64*nb))-1)) >= (BS & ((1<<(64*nb))-1)): rp_ge = True
            if not rp_ge: break
            new_rp -= BS; qhat += 1; guard += 1
        if debug and blk < 6:
            print(f"  blk{blk} qn_rem={qn_rem} wstart={wstart} thi={thi}: qhat={qhat.bit_length()}b true_qhat={true_qhat.bit_length()}b qhat==true?{qhat==true_qhat} new_rp<BS?{new_rp < BS} new_rp_bits={new_rp.bit_length()}")
        # write ALL thi+1 limbs of qhat to q at positions [qn_rem-thi, qn_rem]
        q |= (qhat & ((1<<(64*(thi+1)))-1)) << (64*(qn_rem - thi))
        # update Z: replace window with new_rp
        Z = Z - (window << (64*wstart)) + (new_rp << (64*wstart))
        qn_rem -= thi
        blk += 1
    r = (Z & ((1 << (64*nb)) - 1)) >> sigma
    return q, r, blk

def gen_case(nb, na_ratio, rng):
    B = rng.getrandbits(64 * nb)
    if B < (1 << (64 * (nb - 1))):
        B |= (1 << (64 * (nb - 1)))
    na = max(3 * nb, int(nb * na_ratio))
    A = rng.getrandbits(64 * na)
    if A < (1 << (64 * (na - 1))):
        A |= (1 << (64 * (na - 1)))
    if A < B:
        A += B
    return A, B

rng = random.Random(20260813)
for (nb, ratio) in [(200,3.0),(200,8.0),(256,3.0)]:
    A, B = gen_case(nb, ratio, rng)
    qref, rref, blk = mu_div_ref(A, B, debug=(ratio==3.0 and nb==200))
    qexp = A//B; rexp = A%B
    print(f"nb={nb} ratio={ratio}: qref==qexp?{qref==qexp} rref==rexp?{rref==rexp} blocks={blk}")
    print(f"  A==qref*B+rref?{A==qref*B+rref}")
    if qref != qexp:
        print(f"  qref bits={qref.bit_length()} qexp bits={qexp.bit_length()} diffbits={abs(qref.bit_length()-qexp.bit_length())}")
