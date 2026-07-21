import subprocess
import sys
import os
import random

def run_exe(exe_path, input_data):
    result = subprocess.run([exe_path], input=input_data, capture_output=True, timeout=60)
    return result.stdout, result.returncode

def gen_test(a_digits, b_digits, seed=42):
    random.seed(seed)
    sys.set_int_max_str_digits(max(4300, a_digits * 2 + 100))
    a = random.randint(10**(a_digits-1), 10**a_digits - 1)
    b = random.randint(10**(b_digits-1), 10**b_digits - 1)
    return a, b

if __name__ == '__main__':
    os.chdir(r'd:\precious_speed')

    # 测试 O3 版本的 ADD/MUL/DIV 正确性
    print("=== O3 正确性验证 ===\n")

    # ADD 测试
    print("[ADD O3]")
    random.seed(42)
    sys.set_int_max_str_digits(100000)
    a = random.randint(10**99, 10**100 - 1)
    b = random.randint(10**99, 10**100 - 1)
    expected_add = str(a + b)
    input_add = f"1\n{a}\n{b}\n".encode('ascii')
    stdout, rc = run_exe('moptm_add_O3.exe', input_add)
    got = stdout.decode('ascii').strip()
    print(f"  100+100位: {'PASS' if got == expected_add else 'FAIL'}")

    # MUL 测试
    print("[MUL O3]")
    a, b = gen_test(100, 50, 42)
    expected_mul = str(a * b)
    input_mul = f"1\n{a}\n{b}\n".encode('ascii')
    stdout, rc = run_exe('moptm_mul_O3.exe', input_mul)
    got = stdout.decode('ascii').strip()
    print(f"  100*50位: {'PASS' if got == expected_mul else 'FAIL'}")

    # DIV 测试 (大数据)
    print("[DIV O3]")
    test_cases = [(100, 50, 42), (1000, 500, 100), (10000, 5000, 200), (100000, 50000, 300)]
    for a_digits, b_digits, seed in test_cases:
        a, b = gen_test(a_digits, b_digits, seed)
        q = a // b
        r = a % b
        expected = f"{q} {r}"
        input_div = f"1\n{a}\n{b}\n".encode('ascii')
        stdout, rc = run_exe('moptm_div_O3.exe', input_div)
        got = stdout.decode('ascii').strip()
        status = 'PASS' if got == expected else 'FAIL'
        print(f"  {a_digits}/{b_digits}位: {status}")
        if status == 'FAIL':
            print(f"    expected first 40: {expected[:40]}")
            print(f"    got first 40:      {got[:40]}")
