"""Run cur_mod on single-case .in files and collect DIAG output."""
import subprocess
from pathlib import Path

SINGLE = Path(r"d:\precious_speed\div_dev\lc_test\single")
CUR_MOD = Path(r"d:\precious_speed\div_dev\cur_mod.exe")

cases = [
    "medium_02_c106.in",
    "r_nearly_zero_01_c3692.in",
    "length_ratio_integer_02_c0.in",
    "length_ratio_integer_03_c0.in",
]

for name in cases:
    inp = SINGLE / name
    if not inp.exists():
        print(f"SKIP {name}: not found")
        continue
    r = subprocess.run([str(CUR_MOD)], stdin=open(inp, "rb"),
                       capture_output=True, timeout=120)
    err = r.stderr.decode(errors="replace")
    diag_lines = [l for l in err.split("\n") if "DIAG" in l]
    print(f"\n=== {name} (rc={r.returncode}) ===")
    if diag_lines:
        for l in diag_lines[:30]:
            print(f"  {l}")
    else:
        print("  (no DIAG output)")
