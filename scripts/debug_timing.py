#!/usr/bin/env python3
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh

# 测试程序是否正确运行
rc, out, err = ssh.run('echo "1\n123456789\n12345" | /home/azzr/moptm_default 2>&1', timeout=10)
print(f'Test 1: rc={rc}, out={out.strip()[:100]}')

# 检查 /tmp/test_input.txt 是否存在且非空
rc, out, err = ssh.run('wc -c /tmp/test_input.txt && head -c 50 /tmp/test_input.txt', timeout=10)
print(f'Test input: {out.strip()[:100]}')

# 用 time -p 计时
rc, out, err = ssh.run('/usr/bin/time -p /home/azzr/moptm_default < /tmp/test_input.txt > /dev/null 2>&1', timeout=120)
print(f'Time output: {out.strip()}')

# 直接运行看输出
rc, out, err = ssh.run('/home/azzr/moptm_default < /tmp/test_input.txt 2>&1 | head -c 100', timeout=120)
print(f'Program output (first 100 chars): {out.strip()}')

# 用 Python 计时
rc, out, err = ssh.run("""
python3 -c "
import subprocess, time
times = []
for i in range(5):
    with open('/tmp/test_input.txt') as f:
        t0 = time.perf_counter()
        subprocess.run(['/home/azzr/moptm_default'], stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        t1 = time.perf_counter()
        times.append(t1-t0)
print(f'Best: {min(times)*1000:.1f} ms, Avg: {sum(times)/len(times)*1000:.1f} ms')
print(f'All: {[f\"{t*1000:.1f}\" for t in times]}')
"
""", timeout=300)
print(f'Python timing: {out.strip()}')
