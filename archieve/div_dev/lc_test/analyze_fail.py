"""Analyze failed cases: extract the specific failing case, compare mod vs cor output."""
import sys
from pathlib import Path

OUT = Path(r"d:\precious_speed\div_dev\lc_test\out")
CASES = Path(r"d:\precious_speed\div_dev\lc_test\cases")


def read_lines(path):
    with open(path, "r", errors="replace") as f:
        return f.read().split("\n")


def analyze(name, seed, word_index):
    """word_index is 1-based as reported by checker."""
    mod = read_lines(OUT / f"{name}_{seed:02d}.mod.out")
    cor = read_lines(OUT / f"{name}_{seed:02d}.cor.out")
    inp = read_lines(CASES / f"{name}_{seed:02d}.in")

    # checker counts tokens (q and r alternate), so case_idx = (word_index-1)//2
    case_idx = (word_index - 1) // 2
    is_q = (word_index - 1) % 2 == 0

    print(f"\n=== {name} seed={seed} word={word_index} (case {case_idx}, {'Q' if is_q else 'R'}) ===")

    # input: line 0 is T, then pairs "A B"
    if case_idx + 1 < len(inp):
        line = inp[case_idx + 1].split()
        if len(line) >= 2:
            a, b = line[0], line[1]
            print(f"  |A|={len(a)} |B|={len(b)} ratio={len(a)/max(1,len(b)):.3f}")
            print(f"  A[:40]={a[:40]}... A[-10:]={a[-10:]}")
            print(f"  B[:40]={b[:40]}... B[-10:]={b[-10:]}")

    # mod/cor: line = case_idx has "Q R"
    if case_idx < len(mod) and case_idx < len(cor):
        mq, mr = mod[case_idx].split()
        cq, cr = cor[case_idx].split()
        if is_q:
            print(f"  MOD Q len={len(mq)}: {mq[:50]}...{mq[-20:]}")
            print(f"  COR Q len={len(cq)}: {cq[:50]}...{cq[-20:]}")
            # find diff position
            min_len = min(len(mq), len(cq))
            for i in range(min_len):
                if mq[i] != cq[i]:
                    print(f"  DIFF at pos {i} ({i/min_len*100:.1f}%):")
                    print(f"    mod: ...{mq[max(0,i-15):i+15]}...")
                    print(f"    cor: ...{cq[max(0,i-15):i+15]}...")
                    break
            else:
                print(f"  No diff in first {min_len} chars, len mod={len(mq)} cor={len(cq)}")
        else:
            print(f"  MOD R len={len(mr)}: {mr[:50]}...{mr[-20:]}")
            print(f"  COR R len={len(cr)}: {cr[:50]}...{cr[-20:]}")


# Failed cases from checker output
# medium seed=2: 213th words differ
# r_nearly_zero seed=1: 7385th words differ
# length_ratio_integer seed=2: 1st words differ
# length_ratio_integer seed=3: 1st words differ

analyze("medium", 2, 213)
analyze("r_nearly_zero", 1, 7385)
analyze("length_ratio_integer", 2, 1)
analyze("length_ratio_integer", 3, 1)
