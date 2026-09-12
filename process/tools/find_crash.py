#!/usr/bin/env python3
# 二分定位 rnear_0.in 中首个使 div_zn.bin SIGFPE 崩溃的 case.
import subprocess, sys
path = sys.argv[1] if len(sys.argv) > 1 else 'cases_hex/rnear_0.in'
lines = open(path).read().split('\n')
T = int(lines[0]); pairs = lines[1:1+T]
BIN = 'hex_best/div_zn.bin'
def run(sub):
    inp = f"{len(sub)}\n" + "\n".join(sub) + "\n"
    r = subprocess.run([BIN], input=inp, capture_output=True, text=True, timeout=30)
    return r.returncode != 0  # True = crash/nonzero
# 找最小 i 使 run(pairs[0:i+1]) 崩
lo, hi = 0, len(pairs)
if not run(pairs[0:1]):
    print("first case alone OK; searching...")
# 二分: 维护 known_crash = 最大已知崩溃前缀右端
# 用 [0:mid] 崩则答案在 [0:mid), 否则 [mid:]
a, b = 1, len(pairs)
ans = -1
while a <= b:
    mid = (a + b) // 2
    if run(pairs[0:mid]):
        ans = mid; b = mid - 1
    else:
        a = mid + 1
print("first crash idx (0-based, exclusive prefix len):", ans)
if ans > 0:
    print("crashing case A,B =", pairs[ans-1][:80])
    # 打印完整 case
    print("FULL:", pairs[ans-1])
