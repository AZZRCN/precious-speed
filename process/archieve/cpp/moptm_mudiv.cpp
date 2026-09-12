// mu_div implementation - separate TU to avoid affecting Core2 codegen
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>

namespace hint {

template <typename T>
struct SpanTy {
    T *ptr;
    size_t size;
    SpanTy() = default;
    SpanTy(T *p, size_t s) : ptr(p), size(s) {}
    SpanTy operator+(size_t offset) const { return SpanTy(ptr + offset, size - offset); }
    T &operator[](size_t idx) { return ptr[idx]; }
    const T &operator[](size_t idx) const { return ptr[idx]; }
    T *begin() { return ptr; }
    T *end() { return ptr + size; }
};

template <typename T>
struct ViewTy {
    const T *ptr;
    size_t size;
    ViewTy() = default;
    ViewTy(const T *p, size_t s) : ptr(p), size(s) {}
    ViewTy operator+(size_t offset) const { return ViewTy(ptr + offset, size - offset); }
    const T &operator[](size_t idx) const { return ptr[idx]; }
    const T *begin() const { return ptr; }
    const T *end() const { return ptr + size; }
};

class Integer {
public:
    using Limb = uint16_t;
    using Span = SpanTy<Limb>;
    using View = ViewTy<Limb>;
    static constexpr Limb BASE = 10000;

    // Public static helpers that mu_div needs
    static void absMul(View in1, View in2, Span out);
    static bool absSub(View in1, View in2, Span out);
    static bool absAdd1(View in1, Limb in2, Span out);
    static bool absSub1(View in1, Limb in2, Span out);
    static int absCompare(View input1, View input2);
    static void absInvNewton(View m, Span inv,
                             const double *m_dft = nullptr, size_t m_dft_float_len = 0);
};

template <typename T>
constexpr size_t count_true_length(const T array[], size_t length) {
    if (nullptr == array) return 0;
    while (length > 0 && array[length - 1] == 0) length--;
    return length;
}

} // namespace hint

namespace hint {

// Single-block division using a precomputed full inverse (dn+1 limbs).
// dividend.size must be <= 2 * divisor.size.
// After return, the remainder occupies dividend[0..divisor.size-1].
// quotient has (dividend.size - divisor.size) limbs.
static void divWithInv(Integer::Span dividend, Integer::View divisor, Integer::Span quotient, Integer::View inv_span)
{
    size_t k = divisor.size;
    if (dividend.size <= k) return;

    Integer::Span divid_high = dividend + (k - 1);

    thread_local std::vector<Integer::Limb> tqhat, tprod;
    size_t qhat_len = divid_high.size + inv_span.size;
    size_t prod_len = qhat_len - 1;
    if (tqhat.size() < qhat_len) tqhat.resize(qhat_len);
    if (tprod.size() < prod_len) tprod.resize(prod_len);

    Integer::Span qhat_span(tqhat.data(), qhat_len);
    Integer::Span prod_span(tprod.data(), prod_len);
    Integer::absMul(inv_span, Integer::View(divid_high.ptr, divid_high.size), qhat_span);
    qhat_span = qhat_span + (k + 1);
    Integer::absMul(divisor, Integer::View(qhat_span.ptr, qhat_span.size), prod_span);
    prod_span.size = count_true_length(prod_span.ptr, prod_span.size);

    while (Integer::absCompare(Integer::View(prod_span.ptr, prod_span.size), Integer::View(dividend.ptr, dividend.size)) > 0)
    {
        Integer::absSub(Integer::View(prod_span.ptr, prod_span.size), divisor, prod_span);
        Integer::absSub1(Integer::View(qhat_span.ptr, qhat_span.size), 1, qhat_span);
        prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
    }
    Integer::absSub(Integer::View(dividend.ptr, dividend.size), Integer::View(prod_span.ptr, prod_span.size), dividend);
    dividend.size = k;
    while (Integer::absCompare(Integer::View(dividend.ptr, dividend.size), divisor) >= 0)
    {
        Integer::absSub(Integer::View(dividend.ptr, dividend.size), divisor, dividend);
        Integer::absAdd1(Integer::View(qhat_span.ptr, qhat_span.size), 1, qhat_span);
    }
    qhat_span.size = count_true_length(qhat_span.ptr, qhat_span.size);
    size_t copy_n = qhat_span.size < quotient.size ? qhat_span.size : quotient.size;
    std::copy_n(qhat_span.ptr, copy_n, quotient.ptr);
    if (copy_n < quotient.size)
        std::fill_n(quotient.ptr + copy_n, quotient.size - copy_n, Integer::Limb(0));
}

void absDivNewtonMu_hint(Integer::Span dividend, Integer::View divisor, Integer::Span quotient)
{
    size_t qn = quotient.size;
    size_t dn = divisor.size;
    size_t in;
    if (qn > dn)
    {
        size_t mu_b = (qn - 1) / dn + 1;
        in = (qn - 1) / mu_b + 1;
    }
    else if (3 * qn > dn)
    {
        in = (qn - 1) / 2 + 1;
    }
    else
    {
        in = qn;
    }
    if (in > dn) in = dn;
    if (in > qn) in = qn;

    // Compute full inverse of divisor (dn+1 limbs)
    thread_local std::vector<Integer::Limb> inv_buf;
    if (inv_buf.size() < dn + 1) inv_buf.resize(dn + 1);
    Integer::Span inv_full(inv_buf.data(), dn + 1);
    Integer::absInvNewton(divisor, inv_full);

    // Buffer for remainder
    thread_local std::vector<Integer::Limb> rem_buf_vec;
    if (rem_buf_vec.size() < dn) rem_buf_vec.resize(dn);
    Integer::Limb *rem_buf = rem_buf_vec.data();

    // Initial remainder = top dn limbs of dividend
    std::copy(dividend.ptr + qn, dividend.ptr + qn + dn, rem_buf);

    // Buffer for one block's dividend
    thread_local std::vector<Integer::Limb> block_buf_vec;
    size_t max_block_dn = in + dn;
    if (block_buf_vec.size() < max_block_dn) block_buf_vec.resize(max_block_dn);
    Integer::Limb *block_buf = block_buf_vec.data();

    // Buffer for one block's quotient
    thread_local std::vector<Integer::Limb> block_q_vec;
    if (block_q_vec.size() < in) block_q_vec.resize(in);

    auto np = dividend.ptr + qn;
    auto qp = quotient.ptr + qn;
    size_t qn_rem = qn;

    while (qn_rem > 0)
    {
        size_t block = in < qn_rem ? in : qn_rem;
        np -= block;
        qp -= block;
        qn_rem -= block;

        // Form [np[0..block-1], rem_buf[0..dn-1]] -> block_buf
        std::copy(np, np + block, block_buf);
        std::copy(rem_buf, rem_buf + dn, block_buf + block);
        Integer::Span block_span(block_buf, block + dn);

        // Quotient for this block
        Integer::Span block_quot(block_q_vec.data(), block);

        // Use divWithInv (full inverse available)
        divWithInv(block_span, divisor, block_quot, Integer::View(inv_full.ptr, inv_full.size));

        // Copy quotient and remainder
        std::copy(block_quot.ptr, block_quot.ptr + block, qp);
        std::copy(block_span.ptr, block_span.ptr + dn, rem_buf);
    }

    // Zero out dividend and write final remainder
    std::fill_n(dividend.ptr, qn + dn, Integer::Limb(0));
    size_t new_len = count_true_length(rem_buf, dn);
    if (new_len > 0)
        std::copy(rem_buf, rem_buf + new_len, dividend.ptr);
    dividend.size = new_len > 0 ? new_len : 1;
}

} // namespace hint
