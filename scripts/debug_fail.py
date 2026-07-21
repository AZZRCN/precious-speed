import subprocess
import sys
import os
import random

os.chdir(r'd:\precious_speed')
sys.set_int_max_str_digits(100000)

random.seed(300)
a = random.randint(10**99999, 10**100000 - 1)
b = random.randint(10**49999, 10**50000 - 1)
q = a // b
r = a % b
expected = f"{q} {r}"
input_str = f"1\n{a}\n{b}\n".encode('ascii')

result = subprocess.run(['moptm_div_base64.exe'], input=input_str, capture_output=True, timeout=60)
got = result.stdout.decode('ascii', errors='replace').strip()

print(f"expected len: {len(expected)}")
print(f"got len:      {len(got)}")
print(f"expected first 60: {expected[:60]}")
print(f"got first 60:      {got[:60]}")
print(f"expected last 60:  {expected[-60:]}")
print(f"got last 60:       {got[-60:]}")
print(f"match: {expected == got}")

# 找第一个差异位置
for i in range(min(len(expected), len(got))):
    if expected[i] != got[i]:
        print(f"first diff at pos {i}: expected='{expected[i]}', got='{got[i]}'")
        print(f"  context expected: ...{expected[max(0,i-10):i+10]}...")
        print(f"  context got:      ...{got[max(0,i-10):i+10]}...")
        break
