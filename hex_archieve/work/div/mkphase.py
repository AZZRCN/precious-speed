import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

src = open(r"D:\hex_precious_speed\submit_ready\div.cpp", encoding="utf-8").read()

# 1) add rdtsc helper before main (file scope)
anchor = "int main() {"
assert anchor in src
src = src.replace(anchor, "static inline unsigned long long rdtsc(){ return __rdtsc(); }\n" + anchor, 1)

# 2) declare timers as main locals, start total timer at entry
src = src.replace("    hugify(inbuf_, sizeof inbuf_);",
                  "    unsigned long long g_total = rdtsc(), g_comp = 0;\n    hugify(inbuf_, sizeof inbuf_);", 1)

# 3) wrap the division dispatch with timing
old = (
    "        if (nb < BZ_MIN || na - nb + 1 <= KD_QMAX) {\n"
    "            knuthD(A, na, B, nb, Qout, Rout);\n"
    "        } else {\n"
    "            wp = WORK;\n"
    "            bz_divide(A, na, B, nb, Qout, Rout);\n"
    "        }"
)
new = (
    "        if (nb < BZ_MIN || na - nb + 1 <= KD_QMAX) {\n"
    "            unsigned long long _a = rdtsc(); knuthD(A, na, B, nb, Qout, Rout); g_comp += rdtsc() - _a;\n"
    "        } else {\n"
    "            wp = WORK;\n"
    "            unsigned long long _a = rdtsc(); bz_divide(A, na, B, nb, Qout, Rout); g_comp += rdtsc() - _a;\n"
    "        }"
)
assert old in src, "dispatch block not found"
src = src.replace(old, new, 1)

# 4) print phase split at end of main (target the LAST return 0;)
idx = src.rfind("    return 0;\n}")
assert idx != -1
src = src[:idx] + "    fprintf(stderr, \"PHASE total=%llu comp=%llu io=%llu\\n\", g_total, g_comp, g_total - g_comp);\n" + src[idx:]

open(r"D:\hex_precious_speed\work\div\div_v11_phase.cpp", "w", encoding="utf-8").write(src)
print("phase-instrumented -> div_v11_phase.cpp")

put(r"D:\hex_precious_speed\work\div\div_v11_phase.cpp", "/tmp/div_v11_phase.cpp")

cmd = (
    "cd /tmp; "
    "g++ -O2 -march=native -std=c++23 -o div_v11_phase div_v11_phase.cpp 2>&1 | tail -5; echo BUILD_RC=$?; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "python3 ~/divbench/hexcheck.py run ./div_v11_phase \"$LARGEST\" \"${LARGEST%.in}.exp\"; echo CK_RC=$?; "
    "echo '=== phase cycles (optimized build) ==='; "
    "./div_v11_phase < \"$LARGEST\" > /dev/null; "
    "echo '=== medium_00 for contrast ==='; "
    "M=$(ls -S ~/hexbench/data/div/*.in | sed -n '2p'); ./div_v11_phase < \"$M\" > /dev/null"
)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
