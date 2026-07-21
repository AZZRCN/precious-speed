import subprocess
import sys
import os
import random

def run_div(exe_path, input_data):
    """运行 DIV 测试，返回 (stdout, returncode)"""
    result = subprocess.run([exe_path], input=input_data, capture_output=True, timeout=60)
    return result.stdout.decode('ascii', errors='replace'), result.returncode

def gen_test(a_digits, b_digits, seed=42):
    """生成测试数据，返回 (input_bytes, expected_str)"""
    random.seed(seed)
    sys.set_int_max_str_digits(max(4300, a_digits * 2 + 100))
    a = random.randint(10**(a_digits-1), 10**a_digits - 1)
    b = random.randint(10**(b_digits-1), 10**b_digits - 1)
    q = a // b
    r = a % b
    input_str = f"1\n{a}\n{b}\n"
    expected = f"{q} {r}"
    return input_str.encode('ascii'), expected

if __name__ == '__main__':
    os.chdir(r'd:\precious_speed')

    test_cases = [
        (100, 50, 42),
        (1000, 500, 100),
        (10000, 5000, 200),
        (100000, 50000, 300),
        (200000, 100000, 400),
    ]

    print(f"{'Test':<25} {'base64':<10} {'base128':<10} {'base256':<10}")
    print("-" * 60)

    for a_digits, b_digits, seed in test_cases:
        label = f"{a_digits}/{b_digits}"
        try:
            input_data, expected = gen_test(a_digits, b_digits, seed)
        except Exception as e:
            print(f"{label:<25} GEN FAIL: {e}")
            continue

        results = {}
        for exe, label2 in [
            ('moptm_div_base64.exe', 'base64'),
            ('moptm_div_base128.exe', 'base128'),
            ('moptm_div_base256.exe', 'base256'),
        ]:
            try:
                stdout, rc = run_div(exe, input_data)
                results[label2] = 'PASS' if stdout.strip() == expected else 'FAIL'
                if results[label2] == 'FAIL':
                    # 显示前 60 字符对比
                    exp_short = expected[:60]
                    got_short = stdout.strip()[:60]
                    results[label2] = f"FAIL(exp={exp_short}...,got={got_short}...)"
            except subprocess.TimeoutExpired:
                results[label2] = 'TIMEOUT'
            except Exception as e:
                results[label2] = f'ERR:{e}'

        b64 = results.get('base64', '?')
        b128 = results.get('base128', '?')
        b256 = results.get('base256', '?')
        print(f"{label:<25} {b64:<10} {b128:<10} {b256:<10}")
