"""
LC (library-checker-problems) test pipeline for div_modular.cpp.
Generates test cases using official generators, runs cur_mod.exe vs correct.exe,
and verifies outputs with official checker.

Usage:
    python run_lc_test.py            # full run (skip max.cpp by default)
    python run_lc_test.py --max      # also run max.cpp (2M digits, slow)
    python run_lc_test.py --only large  # only run specified generator
"""
import os
import subprocess
import sys
import time
from pathlib import Path

# === Paths ===
ROOT = Path(r"d:\precious_speed")
DIV_DEV = ROOT / "div_dev"
LC_ROOT = Path(r"E:\library-checker-problems-master")
COMMON = LC_ROOT / "common"
DIV_DIR = LC_ROOT / "big_integer" / "division_of_big_integers"
GEN_DIR = DIV_DIR / "gen"
SOL_DIR = DIV_DIR / "sol"

TEST_DIR = DIV_DEV / "lc_test"
TEST_DIR.mkdir(parents=True, exist_ok=True)
GEN_BIN = TEST_DIR / "gen_bin"
GEN_BIN.mkdir(exist_ok=True)
CASES_DIR = TEST_DIR / "cases"
CASES_DIR.mkdir(exist_ok=True)
OUT_DIR = TEST_DIR / "out"
OUT_DIR.mkdir(exist_ok=True)

CUR_MOD = DIV_DEV / "cur_mod.exe"
CORRECT = TEST_DIR / "correct.exe"
CHECKER = TEST_DIR / "checker.exe"

# Generators and their seed counts (from info.toml)
GENERATORS = {
    "small": 1,
    "medium": 3,
    "large": 2,
    "max": 3,
    "a_max_b_random": 3,
    "r_nearly_zero": 3,
    "length_ratio_integer": 6,
    "burnikel_ziegler_bound": 4,
}

CXX = "g++"
CXXFLAGS = "-O2 -std=c++20 -w"
INCLUDES = f"-I\"{COMMON}\" -I\"{DIV_DIR}\""


def shell_compile(src, out):
    """Compile src -> out using shell string. Returns (rc, err)."""
    cmd = f'{CXX} {CXXFLAGS} {INCLUDES} "{src}" -o "{out}"'
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=120, shell=True)
        return r.returncode, r.stderr
    except subprocess.TimeoutExpired:
        return 124, "TIMEOUT"

# Per-case timeout (seconds). max.cpp generates 2M-digit numbers, needs more time.
CASE_TIMEOUT = {
    "small": 10,
    "medium": 30,
    "large": 60,
    "max": 300,
    "a_max_b_random": 300,
    "r_nearly_zero": 60,
    "length_ratio_integer": 120,
    "burnikel_ziegler_bound": 60,
}


def run(cmd, cwd=None, timeout=30, capture=True):
    """Run command (string or list). Return (rc, stdout, stderr)."""
    try:
        if isinstance(cmd, str):
            r = subprocess.run(cmd, cwd=cwd, capture_output=capture, text=True, timeout=timeout, shell=True)
        else:
            r = subprocess.run(cmd, cwd=cwd, capture_output=capture, text=True, timeout=timeout)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "TIMEOUT"


def log(msg):
    print(msg, flush=True)


def compile_all(skip_max=False):
    """Compile correct.exe, checker.exe, and all generator .exe."""
    log("=== Phase 1: Compilation ===")

    # correct.exe
    rc, err = shell_compile(SOL_DIR / "correct.cpp", CORRECT)
    log(f"  correct.exe: rc={rc}" + (f" err={err[:200]}" if rc else ""))
    if rc:
        return False

    # checker.exe
    rc, err = shell_compile(DIV_DIR / "checker.cpp", CHECKER)
    log(f"  checker.exe: rc={rc}" + (f" err={err[:200]}" if rc else ""))
    if rc:
        return False

    # generators
    ok = True
    for name in GENERATORS:
        src = GEN_DIR / f"{name}.cpp"
        if not src.exists():
            log(f"  {name}.cpp: SKIP (not found)")
            continue
        exe = GEN_BIN / f"{name}.exe"
        rc, err = shell_compile(src, exe)
        status = "OK" if rc == 0 else f"FAIL rc={rc}"
        log(f"  {name}.exe: {status}" + (f" err={err[:200]}" if rc else ""))
        if rc:
            ok = False
    return ok


def gen_case(name, seed):
    """Run generator `name` with `seed`, write to cases/{name}_{seed:02d}.in. Return path or None."""
    exe = GEN_BIN / f"{name}.exe"
    if not exe.exists():
        return None
    out_path = CASES_DIR / f"{name}_{seed:02d}.in"
    try:
        with open(out_path, "wb") as f:
            r = subprocess.run([str(exe), str(seed)], stdout=f, stderr=subprocess.PIPE,
                               timeout=CASE_TIMEOUT.get(name, 60))
        if r.returncode != 0:
            log(f"    gen {name} seed={seed}: FAIL rc={r.returncode}")
            return None
        return out_path
    except subprocess.TimeoutExpired:
        log(f"    gen {name} seed={seed}: TIMEOUT")
        return None


def run_case(in_path, exe, out_path, timeout):
    """Run exe < in_path > out_path. Return (rc, elapsed_ms)."""
    t0 = time.time()
    try:
        with open(in_path, "rb") as fin, open(out_path, "wb") as fout:
            r = subprocess.run([str(exe)], stdin=fin, stdout=fout, stderr=subprocess.PIPE,
                               timeout=timeout)
        dt = int((time.time() - t0) * 1000)
        return r.returncode, dt
    except subprocess.TimeoutExpired:
        dt = int((time.time() - t0) * 1000)
        return 124, dt


def check_case(in_path, mod_out, cor_out):
    """Run checker.exe input mod_out cor_out. Return (rc, msg)."""
    r = subprocess.run([str(CHECKER), str(in_path), str(mod_out), str(cor_out)],
                       capture_output=True, text=True, timeout=30)
    return r.returncode, (r.stdout + r.stderr).strip()


def main():
    skip_max = "--max" not in sys.argv
    only = None
    if "--only" in sys.argv:
        i = sys.argv.index("--only")
        only = sys.argv[i + 1]

    if not compile_all(skip_max=skip_max):
        log("Compilation failed, abort.")
        return 1

    if not CUR_MOD.exists():
        log(f"ERROR: {CUR_MOD} not found. Run ai.bat compile_mod first.")
        return 1

    log("\n=== Phase 2: Generate + Test ===")
    total = 0
    passed = 0
    failed_cases = []
    max_digits_seen = 0

    for name, n_seeds in GENERATORS.items():
        if only and name != only:
            continue
        if skip_max and name == "max":
            log(f"\n[{name}] SKIP (--max not set, 2M digits too slow)")
            continue

        log(f"\n[{name}] {n_seeds} seed(s), timeout={CASE_TIMEOUT.get(name,60)}s/case")
        for seed in range(n_seeds):
            total += 1
            # 1. generate
            t0 = time.time()
            in_path = gen_case(name, seed)
            if in_path is None:
                failed_cases.append((name, seed, "gen_failed"))
                log(f"  seed={seed}: GEN FAILED")
                continue
            in_size = in_path.stat().st_size
            gen_dt = int((time.time() - t0) * 1000)

            # quick peek at digit count (first line is T, then A B)
            try:
                with open(in_path, "r", errors="replace") as f:
                    first_line = f.readline().strip()
                    second = f.readline().strip().split()
                    a_len = len(second[0]) if second else 0
                    b_len = len(second[1]) if len(second) > 1 else 0
                max_digits_seen = max(max_digits_seen, a_len, b_len)
            except Exception:
                a_len = b_len = 0

            # 2. run cur_mod
            mod_out = OUT_DIR / f"{name}_{seed:02d}.mod.out"
            cor_out = OUT_DIR / f"{name}_{seed:02d}.cor.out"
            case_to = CASE_TIMEOUT.get(name, 60)
            rc_mod, dt_mod = run_case(in_path, CUR_MOD, mod_out, timeout=case_to)
            rc_cor, dt_cor = run_case(in_path, CORRECT, cor_out, timeout=case_to)

            if rc_mod != 0:
                failed_cases.append((name, seed, f"mod_rc={rc_mod}"))
                log(f"  seed={seed}: |A|={a_len} |B|={b_len} in={in_size}B gen={gen_dt}ms "
                    f"mod={dt_mod}ms(rc={rc_mod}) cor={dt_cor}ms -> MOD FAIL")
                continue
            if rc_cor != 0:
                failed_cases.append((name, seed, f"cor_rc={rc_cor}"))
                log(f"  seed={seed}: |A|={a_len} |B|={b_len} in={in_size}B gen={gen_dt}ms "
                    f"mod={dt_mod}ms cor={dt_cor}ms(rc={rc_cor}) -> COR FAIL")
                continue

            # 3. check
            ck_rc, ck_msg = check_case(in_path, mod_out, cor_out)
            status = "PASS" if ck_rc == 0 else f"WA(ck={ck_rc})"
            if ck_rc == 0:
                passed += 1
            else:
                failed_cases.append((name, seed, ck_msg[:80]))
            log(f"  seed={seed}: |A|={a_len} |B|={b_len} in={in_size}B gen={gen_dt}ms "
                f"mod={dt_mod}ms cor={dt_cor}ms -> {status}"
                + (f" | {ck_msg[:100]}" if ck_rc else ""))

    log("\n=== Summary ===")
    log(f"Total: {total}, Passed: {passed}, Failed: {len(failed_cases)}")
    log(f"Max digits seen: {max_digits_seen}")
    if failed_cases:
        log("\nFailed cases:")
        for name, seed, why in failed_cases:
            log(f"  {name} seed={seed}: {why}")
    return 0 if not failed_cases else 2


if __name__ == "__main__":
    sys.exit(main())
