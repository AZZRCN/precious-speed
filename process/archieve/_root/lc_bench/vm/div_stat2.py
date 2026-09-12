#!/usr/bin/env python3
"""DIV 结构画像 v2: FFT 工作量按阶段归因。

阶段:
  1 = absInvNewton / absInvNewtonGMP (倒数计算, 递归内全部归此)
  2 = absDivMu 除倒数外 (prepareDFT + 块循环)
  4 = absDivNewtonCore1 除倒数外
  5 = absDivBasicCore
  0 = 其它 (顶层)
计数在 FFT 顶层入口 dif<true>/idit<true>, 单位 = n*log2(n)。
"""
import re
import subprocess
import sys
from pathlib import Path

SRC_IN = Path('/home/azzr/divbench/src/div_D4.cpp')
SRC_OUT = Path('/home/azzr/divbench/src/div_stat2.cpp')
BIN = Path('/home/azzr/divbench/bin/div_stat2')
INDIR = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
FLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIVSTAT'

HEADER = r'''
// ==================== DIVSTAT v2 (结构计数器, 非计时) ====================
#ifdef DIVSTAT
#include <cmath>
#include <vector>
#include <cstdio>
#include <algorithm>
static int g_phase = 0;
struct DsRec { size_t len1, len2, in, blocks; int cyc; double work; };
struct DivStat {
    double ph[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    size_t nxf = 0;
    size_t cb = 0, cc1 = 0, cmu = 0;
    double wblk = 0;                 // 块循环两次 fftMul 的工作量 (阶段2 的子集)
    size_t nblk = 0, ncyc = 0, nlin = 0, nfb = 0, ncorr = 0;
    std::vector<DsRec> recs;
    ~DivStat() { dump(); }
    void dump() {
        std::fprintf(stderr, "@@DS cb=%zu cc1=%zu cmu=%zu nxf=%zu\n", cb, cc1, cmu, nxf);
        std::fprintf(stderr, "@@DS pInv=%.6g pMu=%.6g pC1=%.6g pBa=%.6g pOth=%.6g wblk=%.6g\n",
                     ph[1], ph[2], ph[4], ph[5], ph[0], wblk);
        std::fprintf(stderr, "@@DS nblk=%zu ncyc=%zu nlin=%zu nfb=%zu ncorr=%zu\n",
                     nblk, ncyc, nlin, nfb, ncorr);
        std::vector<DsRec> v = recs;
        std::sort(v.begin(), v.end(), [](const DsRec& a, const DsRec& b) { return a.work > b.work; });
        for (size_t i = 0; i < v.size() && i < 3; i++)
            std::fprintf(stderr, "@@MU len1=%zu len2=%zu in=%zu blocks=%zu cyc=%d work=%.6g\n",
                         v[i].len1, v[i].len2, v[i].in, v[i].blocks, v[i].cyc, v[i].work);
    }
};
static DivStat g_ds;
struct PhaseGuard {
    int old;
    explicit PhaseGuard(int p) : old(g_phase) { g_phase = p; }
    ~PhaseGuard() { g_phase = old; }
};
#define DS(...) do { __VA_ARGS__; } while (0)
#define DSD(...) __VA_ARGS__
#define DSPH(p) PhaseGuard _dspg_((p))
#else
#define DS(...) ((void)0)
#define DSD(...)
#define DSPH(p)
#endif
// =========================================================================
'''

PATCHES = [
    # 0) 头
    ('#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;',
     HEADER + '\n#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;'),

    # 1) FFT 顶层入口计数: dif
    ('''                void dif(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));''',
     '''                void dif(Float inout[], size_t float_len)
                {
                    DS(if constexpr (RIRI_IN) { g_ds.ph[g_phase] += double(float_len) * std::log2(double(float_len)); g_ds.nxf++; });
                    HINT_ASSUME(is_2pow(float_len));'''),

    # 2) FFT 顶层入口计数: idit
    ('''                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));''',
     '''                void idit(Float inout[], size_t float_len)
                {
                    DS(if constexpr (RIRI_OUT) { g_ds.ph[g_phase] += double(float_len) * std::log2(double(float_len)); g_ds.nxf++; });
                    HINT_ASSUME(is_2pow(float_len));'''),

    # 3) 阶段: basicCore
    ('''        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)''',
     '''        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            DSPH(5);
            if (dividend.size <= divisor.size)'''),

    # 4) 阶段: absInvNewton
    ('''        {
            size_t k = m.size;
            assert(k > 0);''',
     '''        {
            DSPH(1);
            size_t k = m.size;
            assert(k > 0);'''),

    # 5) 阶段: absInvNewtonGMP
    ('''        {
            size_t k = m.size;  // GMP n''',
     '''        {
            DSPH(1);
            size_t k = m.size;  // GMP n'''),

    # 6) 阶段: Core1
    ('''         static void absDivNewtonCore1(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size || dividend.size >= divisor.size * 2)''',
     '''         static void absDivNewtonCore1(Span dividend, View divisor, Span quotient)
        {
            DSPH(4);
            if (dividend.size <= divisor.size || dividend.size >= divisor.size * 2)'''),

    # 7) 分发计数
    ('''                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);''',
     '''                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    DS(g_ds.cb++);
                    absDivBasicCore(dividend_span, divisor_span, quot_span);'''),

    ('''                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);''',
     '''                else if (len1 < len2 * 2)
                {
                    DS(g_ds.cc1++);
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);'''),

    ('''                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);''',
     '''                    DS(g_ds.cmu++);
                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);'''),

    # 8) 阶段: absDivMu
    ('''            size_t len1 = dividend.size, len2 = divisor.size;
            assert(in <= len2);''',
     '''            size_t len1 = dividend.size, len2 = divisor.size;
            DSPH(2);
            DSD(double _ds_w0 = g_ds.wblk; size_t _ds_b0 = g_ds.nblk;)
            assert(in <= len2);'''),

    # 9) 块循环 FFT#1
    ('''                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);''',
     '''                DS(g_ds.nblk++;
                   g_ds.wblk += double(inv_float_len) * std::log2(double(inv_float_len)));
                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);'''),

    # 10) basicMul 回退
    ('''                    basicMul(divid_high, inv_span, qhat_full);''',
     '''                    DS(g_ds.nfb++);
                    basicMul(divid_high, inv_span, qhat_full);'''),

    # 11) 块循环 FFT#2 cyclic
    ('''                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);''',
     '''                    DS(g_ds.ncyc++;
                       g_ds.wblk += double(cyclic_m) * std::log2(double(cyclic_m)));
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);'''),

    # 12) 块循环 FFT#2 linear
    ('''                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);''',
     '''                    DS(g_ds.nlin++;
                       g_ds.wblk += double(divisor_float_len) * std::log2(double(divisor_float_len)));
                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);'''),

    # 13) 修正循环
    ('''                    corr_cnt++;
                }''',
     '''                    corr_cnt++; DS(g_ds.ncorr++);
                }'''),
    ('''                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++;''',
     '''                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++; DS(g_ds.ncorr++);'''),
    ('''                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++;''',
     '''                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++; DS(g_ds.ncorr++);'''),

    # 14) absDivMu 出口记录
    ('''            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {''',
     '''            DS(g_ds.recs.push_back({len1, len2, in, g_ds.nblk - _ds_b0,
                                    int(use_cyclic), g_ds.wblk - _ds_w0}));
            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {'''),
]


def main():
    src = SRC_IN.read_text(encoding='utf-8', errors='replace')
    for i, (a, b) in enumerate(PATCHES):
        cnt = src.count(a)
        if cnt != 1:
            print(f'!! patch {i}: anchor count = {cnt} (需 1) -> abort')
            print('   head:', a.strip().splitlines()[0][:110])
            return 1
        src = src.replace(a, b, 1)
    SRC_OUT.write_text(src, encoding='utf-8')
    print(f'patched -> {SRC_OUT} ({len(src)} B)', flush=True)

    r = subprocess.run(f'g++ {FLAGS} -o {BIN} {SRC_OUT} 2>&1', shell=True,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL:\n' + r.stdout[-4000:])
        return 1
    print('build OK\n', flush=True)

    LC = {  # #390120 逐点耗时 (LC 权威数据)
        'example_00': 1, 'small_00': 12, 'medium_00': 17, 'medium_01': 37, 'medium_02': 44,
        'large_00': 40, 'large_01': 45, 'max_00': 12, 'max_01': 5, 'max_02': 4,
        'a_max_b_random_00': 44, 'a_max_b_random_01': 61, 'a_max_b_random_02': 71,
        'r_nearly_zero_00': 16, 'r_nearly_zero_01': 69, 'r_nearly_zero_02': 41,
        'length_ratio_integer_00': 68, 'length_ratio_integer_01': 66, 'length_ratio_integer_02': 75,
        'length_ratio_integer_03': 63, 'length_ratio_integer_04': 56, 'length_ratio_integer_05': 60,
        'burnikel_ziegler_bound_00': 48, 'burnikel_ziegler_bound_01': 42,
        'burnikel_ziegler_bound_02': 53, 'burnikel_ziegler_bound_03': 42,
    }
    hdr = (f'{"case":26s} {"LC":>3s} {"TOTfft":>9s} {"INV":>9s} {"MU-blk":>9s} {"MU-pre":>9s} '
           f'{"C1":>9s} {"BA":>9s} {"INV%":>5s} {"ms/1e7":>7s}')
    print(hdr)
    print('-' * len(hdr))
    tops = {}
    for p in sorted(INDIR.glob('*.in')):
        with open(p, 'rb') as f:
            r = subprocess.run([str(BIN)], stdin=f, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, timeout=300)
        err = r.stderr.decode('utf-8', 'replace')
        d = dict(re.findall(r'\b(pInv|pMu|pC1|pBa|pOth|wblk|nblk|ncyc|nlin|nfb|ncorr|cb|cc1|cmu|nxf)=([\d.eE+-]+)', err))
        f_ = lambda k: float(d.get(k, 0) or 0)
        inv, mu, c1, ba, oth, wblk = f_('pInv'), f_('pMu'), f_('pC1'), f_('pBa'), f_('pOth'), f_('wblk')
        tot = inv + mu + c1 + ba + oth
        ms = LC.get(p.stem, 0)
        tops[p.stem] = re.findall(
            r'@@MU len1=(\d+) len2=(\d+) in=(\d+) blocks=(\d+) cyc=(\d+) work=([\d.eE+-]+)', err)
        print(f'{p.stem:26s} {ms:>3d} {tot:>9.4g} {inv:>9.4g} {wblk:>9.4g} {mu - wblk:>9.4g} '
              f'{c1:>9.4g} {ba:>9.4g} {(100 * inv / tot if tot else 0):>4.0f}% '
              f'{(ms / (tot / 1e7) if tot else 0):>7.2f}', flush=True)

    print('\n=== 最大 mu 调用 ===')
    for k, v in tops.items():
        for l1, l2, i, bl, cy, w in v[:1]:
            print(f'{k:26s} len1={int(l1):>7d} len2={int(l2):>7d} ratio={int(l1)/max(1,int(l2)):6.2f} '
                  f'in={int(i):>7d} blocks={int(bl):>3d} cyc={cy}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
