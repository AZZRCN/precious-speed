#!/usr/bin/env python3
"""VM 端验证脚本: 编译三模式 + ADD/MUL/DIV fuzz 对拍"""
import subprocess, os, sys, random
sys.set_int_max_str_digits(2000000)

def compile_mode(mode_name, exe_name):
    """编译指定模式: 用 Python 修改 #define 行, 创建临时副本编译"""
    src = "/home/azzr/moptm_fusion.cpp"
    tmp = f"/home/azzr/{exe_name}.cpp"
    with open(src) as f:
        lines = f.readlines()
    out_lines = []
    for line in lines:
        stripped = line.lstrip()
        # 匹配所有 HINT_OP_ 开头的 define 行 (含已注释的)
        if '#define HINT_OP_' in stripped:
            # 先取消注释 (去掉前导 //)
            content = stripped
            if content.startswith('//'):
                content = content.lstrip('/').lstrip()
            # 再决定是否注释
            if 'HINT_OP_' + mode_name in content:
                out_lines.append(content)  # 启用此模式
            else:
                out_lines.append('//' + content)  # 注释掉其他模式
        else:
            out_lines.append(line)
    with open(tmp, 'w') as f:
        f.writelines(out_lines)
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE {tmp} -o /home/azzr/{exe_name} -pthread"
    r = subprocess.run(['bash', '-c', cmd], capture_output=True, timeout=300)
    return r.returncode == 0, r.stderr.decode()[:500] if r.stderr else ""

def run_exe(exe, inp):
    r = subprocess.run(['bash', '-c', f'/home/azzr/{exe}'], input=inp,
                      capture_output=True, timeout=120, text=True)
    return r.stdout.strip(), r.returncode

def gen_pair(a_digits, b_digits, seed):
    rng = random.Random(seed)
    a = ''.join([str(rng.randint(0,9)) for _ in range(a_digits)])
    if a_digits > 1 and a[0] == '0':
        a = str(rng.randint(1,9)) + a[1:]
    b = ''.join([str(rng.randint(1,9))] + [str(rng.randint(0,9)) for _ in range(b_digits-1)])
    return a, b

# ============ ADD ============
def verify_add(exe):
    print("=== ADD Verification ===")
    random.seed(42)
    fail = 0; total = 0
    for i in range(500):
        la = random.randint(1, 18); lb = random.randint(1, 18)
        a, b = gen_pair(la, lb, i)
        a_int = int(a) if a else 0; b_int = int(b)
        expected = str(a_int + b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail += 1
            if fail <= 3:
                print("  FAIL small #" + str(i) + ": a=" + a + " b=" + b + " expected=" + expected + " got=" + out)
    print("  small (1-18 digit): " + str(500-fail) + "/500 pass")

    fail2 = 0
    for i in range(200):
        la = random.choice([100, 500, 1000, 5000, 10000])
        lb = random.choice([100, 500, 1000, 5000, 10000])
        a, b = gen_pair(la, lb, i+500)
        a_int = int(a); b_int = int(b)
        expected = str(a_int + b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail2 += 1
            if fail2 <= 3:
                print("  FAIL medium #" + str(i) + ": la=" + str(la) + " lb=" + str(lb))
    print("  medium (100-10k): " + str(200-fail2) + "/200 pass")

    fail3 = 0
    for i in range(30):
        la = random.choice([100000, 200000, 500000, 1000000])
        lb = random.choice([100000, 200000, 500000, 1000000])
        a, b = gen_pair(la, lb, i+700)
        a_int = int(a); b_int = int(b)
        expected = str(a_int + b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail3 += 1
            if fail3 <= 3:
                print("  FAIL large #" + str(i) + ": la=" + str(la) + " lb=" + str(lb))
    print("  large (100k-1M): " + str(30-fail3) + "/30 pass")

    tf = fail + fail2 + fail3
    print("  TOTAL ADD: " + str(total-tf) + "/" + str(total) + " PASS, " + str(tf) + " FAIL")
    return tf == 0

# ============ MUL ============
def verify_mul(exe):
    print("\n=== MUL Verification ===")
    random.seed(123)
    fail = 0; total = 0
    for i in range(500):
        la = random.randint(1, 18); lb = random.randint(1, 18)
        a, b = gen_pair(la, lb, i)
        a_int = int(a) if a else 0; b_int = int(b)
        expected = str(a_int * b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail += 1
            if fail <= 3:
                print("  FAIL small #" + str(i) + ": a=" + a + " b=" + b + " expected=" + expected + " got=" + out)

    fail2 = 0
    for i in range(200):
        la = random.choice([100, 500, 1000, 5000, 10000])
        lb = random.choice([100, 500, 1000, 5000, 10000])
        a, b = gen_pair(la, lb, i+500)
        a_int = int(a); b_int = int(b)
        expected = str(a_int * b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail2 += 1
            if fail2 <= 3:
                print("  FAIL medium #" + str(i) + ": la=" + str(la) + " lb=" + str(lb))

    fail3 = 0
    for i in range(30):
        la = random.choice([100000, 200000, 500000])
        lb = random.choice([100000, 200000, 500000])
        a, b = gen_pair(la, lb, i+700)
        a_int = int(a); b_int = int(b)
        expected = str(a_int * b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail3 += 1
            if fail3 <= 3:
                print("  FAIL large #" + str(i) + ": la=" + str(la) + " lb=" + str(lb))

    fail4 = 0
    for i in range(20):
        la = random.choice([1000, 5000, 10000, 50000])
        a = '9' * la; b = '9' * la
        a_int = int(a); b_int = int(b)
        expected = str(a_int * b_int)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail4 += 1
            if fail4 <= 3:
                print("  FAIL fft_killer #" + str(i) + ": la=" + str(la))

    tf = fail + fail2 + fail3 + fail4
    print("  small: " + str(500-fail) + "/500, medium: " + str(200-fail2) + "/200, large: " + str(30-fail3) + "/30, fft_killer: " + str(20-fail4) + "/20")
    print("  TOTAL MUL: " + str(total-tf) + "/" + str(total) + " PASS, " + str(tf) + " FAIL")
    return tf == 0

# ============ DIV ============
def verify_div(exe):
    print("\n=== DIV Verification ===")
    random.seed(2026)
    fail = 0; total = 0
    for i in range(500):
        a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d = a_d // random.choice([2, 3, 4, 5])
        b_d = max(b_d, 10); b_d = (b_d // 4) * 4
        if b_d < 4: b_d = 4
        a, b = gen_pair(a_d, b_d, i)
        a_int = int(a); b_int = int(b)
        q = a_int // b_int; r = a_int % b_int
        expected = str(q) + " " + str(r)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail += 1
            if fail <= 3:
                print("  FAIL fuzz #" + str(i) + ": a_d=" + str(a_d) + " b_d=" + str(b_d))

    fail2 = 0
    for i in range(50):
        b_d = random.choice([4, 8, 16, 100, 1000, 10000])
        a, b = gen_pair(b_d * 3, b_d, i+500)
        a_int = int(a); b_int = int(b)
        a_int = a_int - (a_int % b_int); a = str(a_int)
        q = a_int // b_int; r = a_int % b_int
        expected = str(q) + " " + str(r)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail2 += 1
            if fail2 <= 3:
                print("  FAIL r_zero #" + str(i) + ": b_d=" + str(b_d))

    fail3 = 0
    for i in range(50):
        b_d = random.choice([4, 8, 16, 100])
        a_d = b_d * random.choice([100, 500, 1000])
        a, b = gen_pair(a_d, b_d, i+550)
        a_int = int(a); b_int = int(b)
        q = a_int // b_int; r = a_int % b_int
        expected = str(q) + " " + str(r)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail3 += 1
            if fail3 <= 3:
                print("  FAIL a_max #" + str(i) + ": a_d=" + str(a_d) + " b_d=" + str(b_d))

    fail4 = 0
    for i in range(50):
        k = random.choice([4, 8, 16, 32, 64, 128, 256])
        a_d = k * random.choice([3, 5, 7, 10]); b_d = k
        a, b = gen_pair(a_d, b_d, i+600)
        a_int = int(a); b_int = int(b)
        q = a_int // b_int; r = a_int % b_int
        expected = str(q) + " " + str(r)
        inp = "1\n" + a + "\n" + b + "\n"
        out, rc = run_exe(exe, inp)
        total += 1
        if out != expected:
            fail4 += 1
            if fail4 <= 3:
                print("  FAIL bz #" + str(i) + ": k=" + str(k))

    tf = fail + fail2 + fail3 + fail4
    print("  fuzz: " + str(500-fail) + "/500, r_zero: " + str(50-fail2) + "/50, a_max: " + str(50-fail3) + "/50, bz: " + str(50-fail4) + "/50")
    print("  TOTAL DIV: " + str(total-tf) + "/" + str(total) + " PASS, " + str(tf) + " FAIL")
    return tf == 0

# ============ Main ============
os.chdir('/home/azzr')
modes = [("ADD", "moptm_verify_ADD"), ("MUL", "moptm_verify_MUL"), ("DIV", "moptm_verify_DIV")]
all_ok = True
for name, exe in modes:
    print("\n=== Compiling " + name + " ===")
    ok, err = compile_mode(name, exe)
    if not ok:
        print("  COMPILE FAILED: " + err)
        all_ok = False
        continue
    print("  [OK]")

if all_ok:
    ok_add = verify_add("moptm_verify_ADD")
    ok_mul = verify_mul("moptm_verify_MUL")
    ok_div = verify_div("moptm_verify_DIV")
    sep = "=" * 50
    print("\n" + sep)
    print("FINAL: ADD=" + ("PASS" if ok_add else "FAIL") + " MUL=" + ("PASS" if ok_mul else "FAIL") + " DIV=" + ("PASS" if ok_div else "FAIL"))
    print("OVERALL: " + ("ALL PASS" if (ok_add and ok_mul and ok_div) else "HAS FAILURES"))
else:
    print("\nCompilation failed, skipping verification")
