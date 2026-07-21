import subprocess
import sys
import os
import random
import time

def run_div_timed(exe_path, input_data):
    """运行 DIV 测试，返回 (elapsed_ms, returncode)"""
    t0 = time.perf_counter()
    result = subprocess.run([exe_path], input=input_data, capture_output=True, timeout=120)
    t1 = time.perf_counter()
    return (t1 - t0) * 1000.0, result.returncode, result.stdout

def gen_test(a_digits, b_digits, seed=42):
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

    # 大数据测试 (1M/500k 位)
    test_cases = [
        (100000, 50000, 300, "100k/50k"),
        (200000, 100000, 400, "200k/100k"),
        (500000, 250000, 500, "500k/250k"),
        (1000000, 500000, 600, "1M/500k"),
    ]

    binaries = [
        ('moptm_div_base64.exe', 'base64'),
        ('moptm_div_base128.exe', 'base128'),
        ('moptm_div_base256.exe', 'base256'),
    ]

    print(f"{'Test':<15} {'binary':<10} {'time(ms)':<12} {'status':<10}")
    print("-" * 50)

    for a_digits, b_digits, seed, label in test_cases:
        try:
            input_data, expected = gen_test(a_digits, b_digits, seed)
        except Exception as e:
            print(f"{label:<15} GEN FAIL: {e}")
            continue

        for exe, name in binaries:
            try:
                # warmup
                run_div_timed(exe, input_data)
                # 3 次取最小
                times = []
                for _ in range(3):
                    elapsed, rc, stdout = run_div_timed(exe, input_data)
                    times.append(elapsed)
                min_time = min(times)
                status = 'PASS' if stdout.strip() == expected else 'FAIL'
                print(f"{label:<15} {name:<10} {min_time:<12.2f} {status:<10}")
            except subprocess.TimeoutExpired:
                print(f"{label:<15} {name:<10} {'TIMEOUT':<12}")
            except Exception as e:
                print(f"{label:<15} {name:<10} ERR:{e}")
        print()
