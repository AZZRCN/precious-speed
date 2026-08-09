#!/usr/bin/env python3
"""Patch best_div.cpp to add BENCH_DIV_GROUPS mode, matching cur_div_groups.py."""
import sys

def patch_best_div(src_path, out_path):
    with open(src_path, 'r') as f:
        code = f.read()

    old_loop = '''    hint::Integer a, b, q, r;
    while (t--) {
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
}'''

    new_loop = '''#ifdef BENCH_DIV_GROUPS
    double t_parse = 0, t_div = 0, t_write = 0;
    double t_div_len2_1 = 0, t_div_len2_2 = 0, t_div_len2_3plus = 0;
    size_t cnt_len2_1 = 0, cnt_len2_2 = 0, cnt_len2_3plus = 0;
    hint::Integer a, b, q, r;
    while (t--) {
        auto _s0 = std::chrono::high_resolution_clock::now();
        parseInteger(a);
        parseInteger(b);
        auto _s1 = std::chrono::high_resolution_clock::now();
        size_t blen = b.length();
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
        if (blen == 1) { t_div_len2_1 += std::chrono::duration<double, std::milli>(_s2 - _s1).count(); cnt_len2_1++; }
        else if (blen == 2) { t_div_len2_2 += std::chrono::duration<double, std::milli>(_s2 - _s1).count(); cnt_len2_2++; }
        else { t_div_len2_3plus += std::chrono::duration<double, std::milli>(_s2 - _s1).count(); cnt_len2_3plus++; }
    }
    flushOutput();
    fprintf(stderr, "=== BENCH_DIV_GROUPS ===\\n");
    fprintf(stderr, "parse:  %.3f ms\\n", t_parse);
    fprintf(stderr, "div:    %.3f ms\\n", t_div);
    fprintf(stderr, "  len2==1:      %zu calls, %.3f ms (%.1f us/call)\\n", cnt_len2_1, t_div_len2_1, cnt_len2_1 > 0 ? t_div_len2_1 * 1000 / cnt_len2_1 : 0);
    fprintf(stderr, "  len2==2:      %zu calls, %.3f ms (%.1f us/call)\\n", cnt_len2_2, t_div_len2_2, cnt_len2_2 > 0 ? t_div_len2_2 * 1000 / cnt_len2_2 : 0);
    fprintf(stderr, "  len2>=3:      %zu calls, %.3f ms (%.1f us/call)\\n", cnt_len2_3plus, t_div_len2_3plus, cnt_len2_3plus > 0 ? t_div_len2_3plus * 1000 / cnt_len2_3plus : 0);
    fprintf(stderr, "write:  %.3f ms\\n", t_write);
    fprintf(stderr, "total:  %.3f ms\\n", t_parse + t_div + t_write);
    return 0;
#else
    hint::Integer a, b, q, r;
    while (t--) {
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
#endif
}'''

    if old_loop not in code:
        print("ERROR: Could not find main loop pattern in best_div.cpp")
        idx = code.find('hint::Integer a, b, q, r;')
        if idx >= 0:
            print(f"Found at pos {idx}")
            start = max(0, idx - 50)
            end = min(len(code), idx + 600)
            print(code[start:end])
        sys.exit(1)

    code = code.replace(old_loop, new_loop, 1)

    with open(out_path, 'w') as f:
        f.write(code)
    print(f"Patched: {src_path} -> {out_path}")

if __name__ == '__main__':
    patch_best_div('best_div.cpp', 'best_div_groups.cpp')
