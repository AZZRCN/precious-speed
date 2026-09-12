#!/usr/bin/env python3
"""Patch cur_div.cpp to add BENCH_DIV_STAGES mode for fine-grained stage timing."""
import sys
import re

def patch_cur_div(src_path, out_path):
    with open(src_path, 'r') as f:
        code = f.read()

    # 1. Add global stage counters after #includes (find first namespace hint or class Integer)
    # We'll add them right before the HINT_OP_DIV main function
    
    # 2. Patch absDivRem to add path counters
    old_absDivRem = '''        void absDivRem(const Integer &divisor, Integer &quotient, Integer &remainder) const
        {
            size_t len1 = this->length(), len2 = divisor.length();
            int cmp = absCompare(this->getView(), divisor.getView());
            if (cmp == 0)
            {
                quotient = Limb(1);
                remainder = Limb(0);
            }
            else if (cmp < 0)
            {
                quotient = Limb(0);
                remainder = *this;
            }
            else if (len2 == 1)
            {
                quotient = *this;
                remainder = quotient.selfDivRem1(divisor.data[0]);
            }
            else
            {'''

    new_absDivRem = '''        void absDivRem(const Integer &divisor, Integer &quotient, Integer &remainder) const
        {
#ifdef BENCH_DIV_STAGES
            extern ::size_t g_div_calls, g_div_path_eq, g_div_path_lt, g_div_path_1, g_div_path_newton;
            extern double g_div_path_eq_t, g_div_path_lt_t, g_div_path_1_t, g_div_path_newton_t;
            g_div_calls++;
            auto _absDivRem_t0 = std::chrono::high_resolution_clock::now();
#endif
            size_t len1 = this->length(), len2 = divisor.length();
            int cmp = absCompare(this->getView(), divisor.getView());
            if (cmp == 0)
            {
#ifdef BENCH_DIV_STAGES
                g_div_path_eq++; auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif
                quotient = Limb(1);
                remainder = Limb(0);
#ifdef BENCH_DIV_STAGES
                auto _p_t1 = std::chrono::high_resolution_clock::now();
                g_div_path_eq_t += std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count();
#endif
            }
            else if (cmp < 0)
            {
#ifdef BENCH_DIV_STAGES
                g_div_path_lt++; auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif
                quotient = Limb(0);
                remainder = *this;
#ifdef BENCH_DIV_STAGES
                auto _p_t1 = std::chrono::high_resolution_clock::now();
                g_div_path_lt_t += std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count();
#endif
            }
            else if (len2 == 1)
            {
#ifdef BENCH_DIV_STAGES
                g_div_path_1++; auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif
                quotient = *this;
                remainder = quotient.selfDivRem1(divisor.data[0]);
#ifdef BENCH_DIV_STAGES
                auto _p_t1 = std::chrono::high_resolution_clock::now();
                g_div_path_1_t += std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count();
#endif
            }
            else
            {
#ifdef BENCH_DIV_STAGES
                g_div_path_newton++; auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif'''

    if old_absDivRem not in code:
        print("ERROR: Could not find absDivRem pattern")
        # Find the pattern
        idx = code.find('void absDivRem(const Integer &divisor')
        if idx >= 0:
            print(f"Found at pos {idx}")
            print(code[idx:idx+500])
        sys.exit(1)

    code = code.replace(old_absDivRem, new_absDivRem, 1)

    # 3. Find the closing brace of the else block and add timing end
    # The else block ends with the Newton division logic, need to find the right place
    # Let's find "quotient.data.resize(quot_len);" which is early in the else block
    # Actually, we need to close the timing at the end of the else block
    # The else block contains the Newton division, which ends before the function's closing brace
    
    # Find the pattern that marks end of Newton path
    # Looking for "remainder = std::move(dividend_norm);" or similar
    old_newton_end = '''                remainder = std::move(dividend_norm);
                quotient.removeLeadingZero();
            }
        }'''
    
    new_newton_end = '''                remainder = std::move(dividend_norm);
                quotient.removeLeadingZero();
#ifdef BENCH_DIV_STAGES
                auto _p_t1 = std::chrono::high_resolution_clock::now();
                g_div_path_newton_t += std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count();
#endif
            }
        }'''

    if old_newton_end in code:
        code = code.replace(old_newton_end, new_newton_end, 1)
    else:
        print("WARNING: Could not find Newton end pattern, trying alternative...")
        # Try to find just the end of absDivRem function
        # The function ends with the else block's closing brace

    # 4. Patch main function to add stage timing and global variable definitions
    old_main = '''#elif defined(HINT_OP_DIV)
int main() {
    setvbuf(stderr, NULL, _IONBF, 0);
    initInput();
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;'''

    new_main = '''#elif defined(HINT_OP_DIV)
#ifdef BENCH_DIV_STAGES
size_t g_div_calls = 0, g_div_path_eq = 0, g_div_path_lt = 0, g_div_path_1 = 0, g_div_path_newton = 0;
double g_div_path_eq_t = 0, g_div_path_lt_t = 0, g_div_path_1_t = 0, g_div_path_newton_t = 0;
#endif
int main() {
    setvbuf(stderr, NULL, _IONBF, 0);
    initInput();
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;'''

    if old_main not in code:
        print("ERROR: Could not find main function pattern")
        sys.exit(1)

    code = code.replace(old_main, new_main, 1)

    # 5. Add stage timing in the main loop and output results at the end
    old_loop = '''#else
    hint::Integer a, b, q, r;
    while (t--)
    {
        parseInteger(a);
        parseInteger(b);
        a.absDivRem(b, q, r);
        writeHint(q);
        *oCursor++ = ' ';
        writeHint(r);
        *oCursor++ = '\\n';
    }
    flushOutput();
    return 0;'''

    new_loop = '''#else
#ifdef BENCH_DIV_STAGES
    double t_parse = 0, t_div = 0, t_write = 0;
    hint::Integer a, b, q, r;
    while (t--)
    {
        auto _s0 = std::chrono::high_resolution_clock::now();
        parseInteger(a);
        parseInteger(b);
        auto _s1 = std::chrono::high_resolution_clock::now();
        a.absDivRem(b, q, r);
        auto _s2 = std::chrono::high_resolution_clock::now();
        writeHint(q);
        *oCursor++ = ' ';
        writeHint(r);
        *oCursor++ = '\\n';
        auto _s3 = std::chrono::high_resolution_clock::now();
        t_parse += std::chrono::duration<double, std::milli>(_s1 - _s0).count();
        t_div += std::chrono::duration<double, std::milli>(_s2 - _s1).count();
        t_write += std::chrono::duration<double, std::milli>(_s3 - _s2).count();
    }
    flushOutput();
    fprintf(stderr, "=== BENCH_DIV_STAGES ===\\n");
    fprintf(stderr, "parse:  %.3f ms\\n", t_parse);
    fprintf(stderr, "div:    %.3f ms (calls=%zu)\\n", t_div, (size_t)g_div_calls);
    fprintf(stderr, "  path_eq:     %zu calls, %.3f ms\\n", g_div_path_eq, g_div_path_eq_t);
    fprintf(stderr, "  path_lt:     %zu calls, %.3f ms\\n", g_div_path_lt, g_div_path_lt_t);
    fprintf(stderr, "  path_1:      %zu calls, %.3f ms\\n", g_div_path_1, g_div_path_1_t);
    fprintf(stderr, "  path_newton: %zu calls, %.3f ms\\n", g_div_path_newton, g_div_path_newton_t);
    fprintf(stderr, "write:  %.3f ms\\n", t_write);
    fprintf(stderr, "total:  %.3f ms\\n", t_parse + t_div + t_write);
    return 0;
#else
    hint::Integer a, b, q, r;
    while (t--)
    {
        parseInteger(a);
        parseInteger(b);
        a.absDivRem(b, q, r);
        writeHint(q);
        *oCursor++ = ' ';
        writeHint(r);
        *oCursor++ = '\\n';
    }
    flushOutput();
    return 0;
#endif'''

    if old_loop not in code:
        print("ERROR: Could not find main loop pattern")
        # Try to find it
        idx = code.find('hint::Integer a, b, q, r;')
        if idx >= 0:
            print(f"Found at pos {idx}")
            print(code[idx:idx+500])
        sys.exit(1)

    code = code.replace(old_loop, new_loop, 1)

    with open(out_path, 'w') as f:
        f.write(code)
    print(f"Patched: {src_path} -> {out_path}")

if __name__ == '__main__':
    patch_cur_div('cur_div.cpp', 'cur_div_stages.cpp')
