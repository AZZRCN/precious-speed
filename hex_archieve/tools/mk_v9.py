import io, sys
p = 'work/mul/v9.cpp'
s = open(p, encoding='utf-8').read()

anchor = "static void ditRec(cpx* d, u32 n, u32 bb) {"
newfn = """// ---- shang ban (index >= n/2) quan ling shi de ding ceng radix-4 (bb == 0, w0=w1=1) ----
// t2 = t3 = 0  =>  a0 = a2 = x0, a1 = a3 = x1
//   p[0]=x0+x1  p[q]=x0-x1  p[2q]=x0+x1*w2  p[3q]=x0-x1*w2
// zhi du 2 ge quarter (sheng yi ban du liu liang), qie split wu xu qing ling shang ban.
static void difZeroHiTop(cpx* d, u32 n) {
    const u32 q = n >> 2, bs = q << 1;
    const cpx w2 = tw[1];
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = d + q;
    cpx* p = d;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d u3 = cmul4(x1, W2, W2s);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x0, x1));
        _mm256_storeu_pd((double*)(p + q), _mm256_sub_pd(x0, x1));
        _mm256_storeu_pd((double*)(p + bs), _mm256_add_pd(x0, u3));
        _mm256_storeu_pd((double*)(p + bs + q), _mm256_sub_pd(x0, u3));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q];
        cpx u3 = cmul(x1, w2);
        p[0] = _mm_add_pd(x0, x1); p[q] = _mm_sub_pd(x0, x1);
        p[bs] = _mm_add_pd(x0, u3); p[bs + q] = _mm_sub_pd(x0, u3);
    }
}
static void difRecZeroHi(cpx* d, u32 n) {
    const u32 q = n >> 2;
    difZeroHiTop(d, n);
    difRec(d, q, 0);
    difRec(d + q, q, 1);
    difRec(d + 2 * q, q, 2);
    difRec(d + 3 * q, q, 3);
}
"""
assert s.count(anchor) == 1
s = s.replace(anchor, newfn + anchor, 1)

old_sig = "static void split_b2(const u64* src, double* g, size_t n, int k, size_t lm) {"
assert s.count(old_sig) == 1, "sig"
s = s.replace(old_sig, "static size_t split_b2(const u64* src, double* g, size_t n, int k, size_t zlim) {", 1)

old_tail = """    if (lm > total) std::memset(g + total, 0, (lm - total) * 8);
}"""
assert s.count(old_tail) == 1, "tail"
s = s.replace(old_tail, """    if (zlim > total) std::memset(g + total, 0, (zlim - total) * 8);
    return total;
}""", 1)

old_mul = """    const bool same = (a == b) && (na == nb);
    split_b2(a, FB, na, k, lm);
    if (!same) split_b2(b, GB, nb, k, lm);
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    if (same) {
        fft::pointwiseSq((fft::cpx*)FB, ts);
    } else {
        fft::difRec((fft::cpx*)GB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    }"""
assert s.count(old_mul) == 1, "mul"
new_mul = """    const bool same = (a == b) && (na == nb);
    const u32 ts = lm >> 1;
    // zero-hi: dan ge cao zuo shu xi shu shu <= lm/2 shi, shi shu huan chong shang ban zheng kuai wei ling,
    // ding ceng radix-4 ke zhi du xia ban (sheng 1/2 du liu liang + split sheng 1/2 qing ling).
    const bool deep = ts > (1u << FFT_LEAF_LOG);
    const size_t half = lm >> 1;
    const size_t ta = split_b2(a, FB, na, k, deep ? half : lm);
    const bool hiA = deep && ta <= half;
    if (!hiA && ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
    size_t tb = ta; bool hiB = hiA;
    if (!same) {
        tb = split_b2(b, GB, nb, k, deep ? half : lm);
        hiB = deep && tb <= half;
        if (!hiB && tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
    }
    fft::resize(ts);
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
    if (same) {
        fft::pointwiseSq((fft::cpx*)FB, ts);
    } else {
        if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    }"""
s = s.replace(old_mul, new_mul, 1)

open(p, 'w', encoding='utf-8').write(s)
print("ok")
