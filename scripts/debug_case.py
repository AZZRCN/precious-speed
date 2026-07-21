#!/usr/bin/env python3
# 调试单个失败案例: 对比 moptm 输出 vs Python 正确答案
import paramiko, random

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

# 重现 fuzz seed 2026 的 case #17: a=2000 b=400
random.seed(2026)
for i in range(18):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4

a_d, b_d = 2000, 400
print(f"Case #17: a={a_d} b={b_d}")

# 生成输入
a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
inp = f"1\n{a}\n{b}\n"

# Python 正确答案
A = int(a)
B = int(b)
Q = A // B
R = A % B
print(f"Python: Q len={len(str(Q))}, R len={len(str(R))}")
print(f"Python Q first 80: {str(Q)[:80]}")
print(f"Python Q last 80:  {str(Q)[-80:]}")
print(f"Python R: {str(R)[:80]}")

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 写入输入文件
sftp = c.open_sftp()
with sftp.open('/tmp/debug.in', 'w') as f:
    f.write(inp)
sftp.close()

# 运行 moptm_default
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && ./moptm_default < /tmp/debug.in", timeout=60)
out = stdout.read().decode()
err = stderr.read().decode()
rc = stdout.channel.recv_exit_status()
print(f"\n=== moptm_default (rc={rc}) ===")
if err:
    print(f"STDERR: {err[:500]}")
line = out.strip()
parts = line.split(' ')
print(f"Output parts: {len(parts)}")
mq = parts[0] if len(parts) >= 1 else ""
mr = parts[1] if len(parts) >= 2 else ""
print(f"moptm Q len={len(mq)}, first 80: {mq[:80]}")
print(f"moptm Q last 80:  {mq[-80:]}")
print(f"moptm R len={len(mr)}: {mr[:80]}")

# 对比
q_match = (str(Q) == mq)
r_match = (str(R) == mr)
print(f"\n=== 对比 ===")
print(f"Quotient match: {q_match}")
print(f"Remainder match: {r_match}")
if not q_match:
    # 商差异分析
    try:
        mq_int = int(mq)
        diff = mq_int - Q
        print(f"Quotient diff (moptm - Python): {diff}")
        print(f"  diff sign: {'+' if diff > 0 else '-' if diff < 0 else '0'}")
        print(f"  diff magnitude: {abs(diff)}")
        print(f"  diff digits: {len(str(abs(diff)))}")
    except:
        print(f"  (无法解析 moptm Q 为整数)")
if not r_match:
    try:
        mr_int = int(mr)
        diff = mr_int - R
        print(f"Remainder diff (moptm - Python): {diff}")
    except:
        print(f"  (无法解析 moptm R 为整数)")

c.close()
