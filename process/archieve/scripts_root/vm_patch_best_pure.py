#!/usr/bin/env python3
"""Patch best_div.cpp to add BENCH_DIV_PURE mode, matching cur_div.cpp."""
import sys

def patch_best_div(src_path, out_path):
    with open(src_path, 'r') as f:
        code = f.read()

    # Find the HINT_OP_DIV main function
    old_main = '''    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
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
}'''

    new_main = '''    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
#ifdef BENCH_DIV_PURE
    std::vector<hint::Integer> a_arr, b_arr, q_arr, r_arr;
    a_arr.reserve(t); b_arr.reserve(t); q_arr.resize(t); r_arr.resize(t);
    hint::Integer a, b;
    for (size_t i = 0; i < t; i++) {
        parseInteger(a);
        parseInteger(b);
        a_arr.push_back(std::move(a));
        b_arr.push_back(std::move(b));
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < t; i++) {
        if (!b_arr[i].isZero()) {
            a_arr[i].absDivRem(b_arr[i], q_arr[i], r_arr[i]);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double div_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    fprintf(stderr, "[DIV] pure div time: %.3f ms\\n", div_ms);
    for (size_t i = 0; i < t; i++) {
        if (b_arr[i].isZero()) {
            *oCursor++ = '0'; *oCursor++ = ' '; *oCursor++ = '0';
        } else {
            writeHint(q_arr[i]); *oCursor++ = ' '; writeHint(r_arr[i]);
        }
        *oCursor++ = '\\n';
    }
    flushOutput();
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

    if old_main not in code:
        print("ERROR: Could not find main function pattern in best_div.cpp")
        # Try to find the pattern with different whitespace
        import re
        # Search for the key line
        idx = code.find('hint::Integer a, b, q, r;')
        if idx >= 0:
            # Show context
            start = max(0, idx - 100)
            end = min(len(code), idx + 400)
            print(f"Found 'hint::Integer a, b, q, r;' at pos {idx}")
            print(f"Context:")
            print(code[start:end])
        sys.exit(1)

    code = code.replace(old_main, new_main)

    with open(out_path, 'w') as f:
        f.write(code)
    print(f"Patched: {src_path} -> {out_path}")

if __name__ == '__main__':
    patch_best_div('best_div.cpp', 'best_div_patched.cpp')
