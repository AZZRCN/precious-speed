#!/usr/bin/env python3
# 生成一个 FAIL case 的详细数据，用于调试
import paramiko, base64, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

TEST_PROG = r'''
#include <bits/stdc++.h>
using namespace std;

// 我们需要: 输入 a, b, 输出 a/b 和 a%b
// 同时输出 q 和 r 的长度
int main() {
    int n;
    cin >> n;
    while (n--) {
        string a, b;
        cin >> a >> b;
        // 用 Python 风格输出: 十进制字符串
        // 但我们的 C++ 程序用 moptm_fusion.cpp 的 Integer 类
        // 这里我们只输出 a 和 b 的长度，用于定位
        cout << a.size() << " " << b.size() << endl;
    }
    return 0;
}
'''

# 实际上，让我写一个调试脚本，捕获一个 FAIL case 的 a 和 b，然后分析
FUZZ_SCRIPT = """
import random, subprocess, os, sys
sys.set_int_max_str_digits(2000000)
random.seed(2026)

def gen(a_digits, b_digits, idx):
    random.seed(2026 + idx)  # 可复现
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return a, b

# 第一个 FAIL 是 #6: a=100000 b=50000
a, b = gen(100000, 50000, 6)
print(f"a = {a[:50]}... ({len(a)} digits)")
print(f"b = {b[:50]}... ({len(b)} digits)")
A = int(a)
B = int(b)
Q = A // B
R = A % B
print(f"correct q len = {len(str(Q))}")
print(f"correct r len = {len(str(R))}")
print(f"correct q head = {str(Q)[:40]}")
print(f"correct r head = {str(R)[:40]}")

# 保存到文件
with open('/tmp/debug_a.txt', 'w') as f:
    f.write(a)
with open('/tmp/debug_b.txt', 'w') as f:
    f.write(b)
print("Saved a, b to /tmp/debug_a.txt, /tmp/debug_b.txt")
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

print("\n=== Generate FAIL case data ===")
stdin, stdout, stderr = c.exec_command(f"echo {FUZZ_B64} | base64 -d | python3", timeout=120)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err[:300]}")

c.close()
