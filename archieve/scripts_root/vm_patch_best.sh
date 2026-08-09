#!/bin/bash
# Patch best_div.cpp to add BENCH_DIV_PURE mode
cd ~/div_bench

# Create patched version
python3 -c "
import re
with open('best_div.cpp') as f:
    code = f.read()

# Add BENCH_DIV_PURE block before the main while loop
old = '''    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    hint::Integer a, b, q, r;
    while (t--) {
        parseInteger(a);
        parseInteger(b);
        a.absDivRem(b, q, r);
        writeHint(q);
        *oCursor++ = ' ';
        writeHint(r);
        *oCursor++ = '\n';
    }
    flushOutput();
    return 0;
}'''

new = '''    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
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
    fprintf(stderr, \"[DIV] pure div time: %.3f ms\\n\", div_ms);
    for (size_t i = 0; i < t; i++) {
        if (b_arr[i].isZero()) {
            *oCursor++ = '0'; *oCursor++ = ' '; *oCursor++ = '0';
        } else {
            writeHint(q_arr[i]); *oCursor++ = ' '; writeHint(r_arr[i]);
        }
        *oCursor++ = '\n';
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
        *oCursor++ = '\n';
    }
    flushOutput();
    return 0;
#endif
}'''

code = code.replace(old, new)
with open('best_div_patched.cpp', 'w') as f:
    f.write(code)
print('Patched OK')
"

# Compile patched version with BENCH_DIV_PURE
g++ -std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DBENCH_DIV_PURE -I. best_div_patched.cpp -o best_div_pure -lpthread && echo "compiled OK"
