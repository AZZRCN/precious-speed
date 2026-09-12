import struct, sys
sys.set_int_max_str_digits(800000)

data = open(r'D:\precious_speed\tools\ndiag10.dat', 'rb').read()
off = 0
def rdU32():
    global off
    v = struct.unpack_from('<I', data, off)[0]; off += 4; return v
def rdU64arr(n):
    global off
    a = struct.unpack_from('<%dQ' % n, data, off); off += 8*n; return a

T = rdU32()
B = 1 << 64
for _ in range(T):
    t = rdU32(); na = rdU32(); nb = rdU32(); status = rdU32()
    if status == 0:
        n = rdU32(); na_an = rdU32(); qn = rdU32(); match = rdU32()
        start = off
        def R(tag, cnt, is64=True):
            global off
            a = rdU64arr(cnt) if is64 else (rdU32(),)
            if t == 0: print(f'  {tag}: read {cnt} {"u64" if is64 else "u32"} -> off now {off}')
            return a
        DN = R('DN', n); AN = R('AN', na_an)
        V = R('V', n); V2 = R('V2', n)
        L = R('L', na_an+n); L2 = R('L2', na_an+n)
        QE = R('QE', qn); RT = R('RT', na_an)
        Qn = R('Qn', qn); Qk = R('Qk', qn)
        Rn = R('Rn', nb); Rk = R('Rk', nb)
        Aa = R('Aa', na); Bb = R('Bb', nb)
        D_in = R('D_in', n); A_in = R('A_in', na); Sig = R('Sig', 1, False)[0]
        def to_int(a):
            v = 0
            for x in reversed(a):
                v = (v << 64) | x
            return v
        dn = to_int(DN); an = to_int(AN)
        v = to_int(V); v2 = to_int(V2)
        Lv = to_int(L); L2v = to_int(L2)
        qe = to_int(QE); rt = to_int(RT)
        Qnc = to_int(Qn); Qkc = to_int(Qk)
        Rnc = to_int(Rn); Rkc = to_int(Rk)
        Aint = to_int(Aa); Bint = to_int(Bb)
        Din = to_int(D_in); Ain = to_int(A_in)
        if t == 0:
            print('DEBUG off after case0 arrays =', off, 'next u32s:',
                  struct.unpack_from('<IIII', data, off)[0:4] if off+16<=len(data) else 'OOB')
        print('='*78)
        print(f'CASE {t}: na={na} nb={nb} n={n} na_an={na_an} qn={qn} match={match}')
        # verify normalization from the ACTUAL input divisor d (inside newton_divide)
        d_shifted = (Din << Sig)
        a_shifted = (Ain << Sig)
        # truncate to n / na_an limbs
        BITS_N = 64*n; BITS_NAAN = 64*na_an
        d_sh_n = d_shifted & ((1 << BITS_N) - 1)
        a_sh_naan = a_shifted & ((1 << BITS_NAAN) - 1)
        print(f'  sigma(in)={Sig}  dn == d_in<<sigma (n limbs)? {dn == d_sh_n}')
        print(f'  an == a_in<<sigma (na_an limbs)? {an == a_sh_naan}')
        sig2 = 64 - D_in[n-1].bit_length() if D_in[n-1] else 64
        print(f'  lzcnt(d_in top) = {sig2}  (matches Sig? {sig2==Sig})')
        print(f'  d_in == pristine B0 ? {Din == Bint}')
        print(f'  a_in == pristine A0 ? {Ain == Aint}')
        v_true = (B**(2*n) // dn) - (B**n)
        print(f'  V == v_true ? {v == v_true}   V2 == v_true ? {v2 == v_true}')
        if v != v_true:
            print(f'    |V-v_true| bits = {(v ^ v_true).bit_length()}')
        modL = B**(64*(na_an+n))
        print(f'  L  == an*v ? {Lv == (an*v) % modL}')
        print(f'  L2 == an*v ? {L2v == (an*v) % modL}')
        # recompute q_est
        himid = an >> (64*n)
        prod = an*v
        lo = prod >> (128*n)
        q0 = himid + lo
        amod = an & ((B**(2*n))-1)
        Lmod = (an*v) & ((B**(2*n))-1)
        if amod + Lmod >= (B**(2*n)):
            q0 += 1
        print(f'  qe == recomputed q_est ? {qe == q0}  (qe bits={qe.bit_length()}, q0 bits={q0.bit_length()})')
        if qe != q0:
            print(f'    |qe-q0| bits = {(qe^q0).bit_length()}')
        print(f'  Qn(cand) == Qk(true) ? {Qnc == Qkc}  (Qn bits={Qnc.bit_length()}, Qk bits={Qkc.bit_length()})')
        print(f'  qe*dn + rt == an ? {(qe*dn + rt) == an}')
        # exact reductions
        q_an_dn = an // dn
        print(f'  qe == an//dn (exact) ? {qe == q_an_dn}')
        print(f'  an//dn == Qk(A//B)  ? {q_an_dn == Qkc}')
        # is rt fully reduced (rt < dn) ?
        rt_ok = (rt < dn)
        print(f'  rt < dn (fully reduced) ? {rt_ok}')
        if not rt_ok:
            print(f'    rt bits={rt.bit_length()}  dn bits={dn.bit_length()}  excess={(rt - (rt % dn)).bit_length() if dn else 0}')
        # verify normalization yields true quotient A//B
        din_top = 1 if D_in[n-1] else 0
        an_ok = (Ain << Sig) == an
        dn_ok = (Din << Sig) == dn
        print(f'  A<<sig==an? {an_ok}  B<<sig==dn? {dn_ok}')
        if an_ok and dn_ok:
            print(f'  an//dn == A//B ? {(an//dn) == (Aint//Bint)}   (A//B bits={(Aint//Bint).bit_length()})')
        print(f'  qe == Qn(returned) ? {qe == Qnc}')
    elif status == 2:
        tag = data[off:off+5].decode(); off += 5
        print(f'CASE {t}: na={na} nb={nb} SKIP({tag})')
    elif status == 3:
        tag = data[off:off+5].decode(); off += 5
        print(f'CASE {t}: na={na} nb={nb} NBZ({tag})')
    else:
        print(f'CASE {t}: UNKNOWN status {status} at off {off}')
        break
print('done, off=%d total=%d' % (off, len(data)))
