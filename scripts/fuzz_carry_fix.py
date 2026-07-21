#!/usr/bin/env python3
# 200 组大规模 fuzz: 验证 carry folding 修复后 USE_GMP_NEWTON vs 基线
import paramiko, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

CMD_COMPILE_DEFAULT = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC moptm_fusion.cpp -o moptm_default -pthread"
CMD_COMPILE_DISABLE = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DDISABLE_2NXN_CYCLIC moptm_fusion.cpp -o moptm_disable -pthread"

FUZZ_SCRIPT = """
import random, subprocess, hashlib, os, sys
sys.set_int_max_str_digits(2000000)
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n", a, b

tests = []
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

fail = 0
detail_count = 0  # 限制详细输出数量，避免 stdout 爆炸
stats = {'default_only_wrong': 0, 'disable_only_wrong': 0, 'both_wrong': 0, 'both_right_but_differ': 0, 'default_crash': 0, 'disable_crash': 0}

for i, (a_d, b_d) in enumerate(tests):
    inp, sa, sb = gen(a_d, b_d)
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r1 = subprocess.run(['bash', '-c', './moptm_default < /tmp/fuzz.in'], capture_output=True, timeout=120)
    r2 = subprocess.run(['bash', '-c', './moptm_disable < /tmp/fuzz.in'], capture_output=True, timeout=120)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    if h1 != h2:
        fail += 1
        # 解析两个版本输出（格式 "q r\\n"）
        try:
            line1 = r1.stdout.decode().strip()
            parts1 = line1.split(' ')
            d_q, d_r = parts1[0], parts1[1]
            d_ok = True
        except Exception as e:
            d_q, d_r = '', ''
            d_ok = False
            stats['default_crash'] += 1
        try:
            line2 = r2.stdout.decode().strip()
            parts2 = line2.split(' ')
            x_q, x_r = parts2[0], parts2[1]
            x_ok = True
        except Exception as e:
            x_q, x_r = '', ''
            x_ok = False
            stats['disable_crash'] += 1

        # Python 正确答案
        A = int(sa)
        B = int(sb)
        Q = A // B
        R = A % B
        sq, sr = str(Q), str(R)

        d_match = d_ok and (d_q == sq) and (d_r == sr)
        x_match = x_ok and (x_q == sq) and (x_r == sr)

        if d_match and not x_match:
            stats['disable_only_wrong'] += 1
            verdict = "DEFAULT_RIGHT"
        elif x_match and not d_match:
            stats['default_only_wrong'] += 1
            verdict = "DISABLE_RIGHT"
        elif not d_match and not x_match:
            stats['both_wrong'] += 1
            verdict = "BOTH_WRONG"
        else:
            stats['both_right_but_differ'] += 1
            verdict = "BOTH_RIGHT_DIFFER"  # 不应发生

        # 前 5 个 FAIL 打印详细
        if detail_count < 5:
            detail_count += 1
            print(f"\\n=== FAIL #{i} (a={a_d} b={b_d}) verdict={verdict} ===")
            print(f"  default rc={r1.returncode} len={len(r1.stdout)} match={d_match}")
            print(f"  disable rc={r2.returncode} len={len(r2.stdout)} match={x_match}")
            if d_ok:
                dq_diff = ''
                if not d_match and d_q != sq:
                    try:
                        diff = int(d_q) - Q
                        dq_diff = f" q_diff={diff:+} ({len(str(abs(diff)))} digits)"
                    except: pass
                dr_diff = ''
                if not d_match and d_r != sr:
                    try:
                        diff = int(d_r) - R
                        dr_diff = f" r_diff={diff:+}"
                    except: pass
                print(f"  default:{dq_diff}{dr_diff}")
                print(f"    d_q head={d_q[:40]} tail={d_q[-40:]}")
                print(f"    d_r head={d_r[:40]}")
            if x_ok:
                xq_diff = ''
                if not x_match and x_q != sq:
                    try:
                        diff = int(x_q) - Q
                        xq_diff = f" q_diff={diff:+} ({len(str(abs(diff)))} digits)"
                    except: pass
                xr_diff = ''
                if not x_match and x_r != sr:
                    try:
                        diff = int(x_r) - R
                        xr_diff = f" r_diff={diff:+}"
                    except: pass
                print(f"  disable:{xq_diff}{xr_diff}")
                print(f"    x_q head={x_q[:40]} tail={x_q[-40:]}")
                print(f"    x_r head={x_r[:40]}")
            print(f"  truth: q_len={len(sq)} r_len={len(sr)}")
            print(f"  truth q head={sq[:40]} tail={sq[-40:]}")
            print(f"  truth r head={sr[:40]}")
            if r1.stderr:
                print(f"  default stderr: {r1.stderr.decode()[:200]}")
            if r2.stderr:
                print(f"  disable stderr: {r2.stderr.decode()[:200]}")
        elif detail_count == 5:
            detail_count += 1
            print(f"\\n... (subsequent FAILs summary only) ...")
    elif i % 40 == 0:
        print(f"OK #{i}: a={a_d} b={b_d}")
    sys.stdout.flush()

print(f"\\n=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ===")
print(f"Stats: {stats}")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

print("\n=== Upload ===")
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK]")

print("\n=== Compile default (USE_GMP_NEWTON) ===")
stdin, stdout, stderr = c.exec_command(CMD_COMPILE_DEFAULT, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err}")
if rc != 0: exit(1)

print("\n=== Compile disable (baseline) ===")
stdin, stdout, stderr = c.exec_command(CMD_COMPILE_DISABLE, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err}")
if rc != 0: exit(1)

print("\n=== Fuzz test (200 cases) ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3", timeout=3600)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

c.close()
