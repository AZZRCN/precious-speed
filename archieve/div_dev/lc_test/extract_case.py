"""Extract a specific case from a multi-case .in file to a single-case .in file."""
import sys
from pathlib import Path

CASES = Path(r"d:\precious_speed\div_dev\lc_test\cases")
OUT = Path(r"d:\precious_speed\div_dev\lc_test\single")


def extract(name, seed, case_idx):
    """Extract case_idx (0-based) from cases/{name}_{seed:02d}.in to single/{name}_{seed:02d}_c{case_idx}.in"""
    inp = (CASES / f"{name}_{seed:02d}.in").read_text(errors="replace").split("\n")
    t = int(inp[0])
    if case_idx >= t:
        print(f"ERROR: case_idx {case_idx} >= T {t}")
        return None
    line = inp[case_idx + 1].split()
    if len(line) < 2:
        print(f"ERROR: bad line: {inp[case_idx+1][:80]}")
        return None
    a, b = line[0], line[1]
    OUT.mkdir(parents=True, exist_ok=True)
    out_path = OUT / f"{name}_{seed:02d}_c{case_idx}.in"
    out_path.write_text(f"1\n{a} {b}\n")
    print(f"Extracted case {case_idx}: |A|={len(a)} |B|={len(b)} -> {out_path}")
    return out_path


# Failed cases:
# medium seed=2: word 213 -> case 106
# r_nearly_zero seed=1: word 7385 -> case 3692
# length_ratio_integer seed=2: word 1 -> case 0
# length_ratio_integer seed=3: word 1 -> case 0

extract("medium", 2, 106)
extract("r_nearly_zero", 1, 3692)
extract("length_ratio_integer", 2, 0)
extract("length_ratio_integer", 3, 0)
