#!/usr/bin/env python3
"""拆解 mul 的 Ir 构成: 生成阶梯式探针变体, 在 VM 上 perf 对比.

用法: python probe_mul.py
输出: 各阶段增量指令数 (IO / split+merge / 正变换 / pointwise / 逆变换)
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

WS = '/home/azzr/hexbench'
ROOT = 'D:/hex_precious_speed'
LCFLAGS = '-O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native'
SRC = sys.argv[1] if len(sys.argv) > 1 else f'{ROOT}/work/mul/v1.cpp'
BASE = os.path.splitext(os.path.basename(SRC))[0]
CASES = 'max_max_00,fft_killer_00,large_01'

BODY_HEAD = """    const size_t u = (size_t)na + nb;
    const int k = pick_k(u);
    const u32 coeffs = (u32)(u * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
    const bool same = (a == b) && (na == nb);
"""

# 阶梯: 每级在前一级基础上加一块
STAGES = {
 'p0_io': BODY_HEAD + """    (void)lm; (void)same;
    for (size_t i = 0; i < u; ++i) c[i] = a[i % (size_t)na] ^ 0x9e3779b97f4a7c15ULL;
""",
 'p1_splitmerge': BODY_HEAD + """    std::memset(FB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    if (!same) { std::memset(GB, 0, (size_t)lm * 8); split_b2(b, GB, nb, k); }
    merge_b2(c, FB, u, k);
""",
 'p2_1fwd': BODY_HEAD + """    std::memset(FB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    if (!same) { std::memset(GB, 0, (size_t)lm * 8); split_b2(b, GB, nb, k); }
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);
""",
 'p3_2fwd': BODY_HEAD + """    std::memset(FB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    if (!same) { std::memset(GB, 0, (size_t)lm * 8); split_b2(b, GB, nb, k); }
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    if (!same) fft::difRec((fft::cpx*)GB, ts, 0);
    merge_b2(c, FB, u, k);
""",
 'p4_pw': BODY_HEAD + """    std::memset(FB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    if (!same) { std::memset(GB, 0, (size_t)lm * 8); split_b2(b, GB, nb, k); }
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    if (same) fft::pointwiseSq((fft::cpx*)FB, ts);
    else { fft::difRec((fft::cpx*)GB, ts, 0); fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts); }
    merge_b2(c, FB, u, k);
""",
}

def main():
    src = open(SRC, 'r', encoding='utf-8').read()
    key = 'static void mul_fft(const u64* a, int na, const u64* b, int nb, u64* c) {'
    i = src.index(key)
    j = src.index('\n}\n', i)
    head, tail = src[:i + len(key)], src[j:]

    names = []
    for name, body in STAGES.items():
        out = head + '\n' + body + tail
        p = f'{ROOT}/work/_scratch/{BASE}_{name}.cpp'
        open(p, 'w', encoding='utf-8').write(out)
        vmctl.put(p, f'{WS}/mul/{BASE}_{name}.cpp')
        names.append(f'{BASE}_{name}')
    vmctl.put(SRC, f'{WS}/mul/{BASE}.cpp')
    vmctl.put(f'{ROOT}/tools/perfone.py', f'{WS}/perfone.py')
    names.append(BASE)

    print('=== compile ===', flush=True)
    cmds = [f'g++ {LCFLAGS} -o {WS}/mul/{n} {WS}/mul/{n}.cpp 2>&1|tail -4' for n in names]
    vmctl.run(' ; '.join(cmds), timeout=1200)

    print('=== perf ladder ===', flush=True)
    blist = ' '.join(f'{WS}/mul/{n}' for n in names)
    vmctl.run(f'python3 {WS}/perfone.py {WS}/data/mul {CASES} {blist}', timeout=1800)

if __name__ == '__main__':
    main()
