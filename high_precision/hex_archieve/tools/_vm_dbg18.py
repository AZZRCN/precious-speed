import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = "/home/azzr/hexbench"; B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
vmctl.put(os.path.join(R, "work", "mul", "v18dbg.cpp"), f"{S}/v18dbg.cpp")
rc, out, _ = vmctl.run(f"cd {S} && {CXX} v18dbg.cpp -o {B}/v18dbg 2>&1 | head -20; echo RC=${{PIPESTATUS[0]}}")
print(out)
if "RC=0" not in (out or ""):
    sys.exit(1)
for c in ["pow2_big", "mix3_big", "mix5_big"]:
    vmctl.run(f"cd {H} && build/v18dbg < data/_big/{c}.in 2>&1 >/dev/null | sort | uniq -c | sort -rn | head -12; echo '--- {c} above ---'")
print("\n===== headline 点 =====")
vmctl.run(f"cd {H} && for f in data/mul/max_max_*.in; do echo \"## $(basename $f)\"; "
          f"build/v18dbg < $f 2>&1 >/dev/null | sort | uniq -c | sort -rn | head -8; done")
