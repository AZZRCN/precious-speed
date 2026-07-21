import subprocess
import sys
import os

def run_test(exe_path, input_file):
    """运行 DIV 测试，返回 (stdout, stderr, returncode)"""
    with open(input_file, 'rb') as f:
        input_data = f.read()
    result = subprocess.run([exe_path], input=input_data, capture_output=True, timeout=30)
    return result.stdout.decode('ascii', errors='replace'), result.stderr.decode('ascii', errors='replace'), result.returncode

if __name__ == '__main__':
    os.chdir(r'd:\precious_speed')
    input_file = 'test_div_data.txt'
    expected_file = 'test_div_expected.txt'

    with open(expected_file, 'r') as f:
        expected = f.read().strip()

    for exe, label in [
        ('moptm_div_base64.exe', 'base64 (k<=64)'),
        ('moptm_div_base128.exe', 'base128 (k<=128)'),
    ]:
        stdout, stderr, rc = run_test(exe, input_file)
        status = 'PASS' if stdout.strip() == expected else 'FAIL'
        print(f"[{label}] rc={rc} {status}")
        if status == 'FAIL':
            print(f"  expected: {expected[:80]}...")
            print(f"  got:      {stdout.strip()[:80]}...")
            if stderr:
                print(f"  stderr:   {stderr[:200]}")
        else:
            print(f"  output: {stdout.strip()[:80]}...")
