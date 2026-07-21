#!/usr/bin/env python3
"""精确复现 fuzz seed=2026 的 case #37 (a=50000, b=10000), 对比 default vs disable"""
import paramiko, random

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

def gen_exact(a_digits, b_digits, seed=2026):
    """完全复现 fuzz_carry_fix.py 的逻辑: 先生成 200 组 (a_d, b_d), 再依次 gen()"""
    rng = random.Random(seed)
    # 阶段 1: 生成 200 组 (a_d, b_d)
    tests = []
    for _ in range(200):
        a_d = rng.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d = a_d // rng.choice([2, 3, 4, 5])
        b_d = max(b_d, 10)
        b_d = (b_d // 4) * 4
        if b_d < 4: b_d = 4
        tests.append((a_d, b_d))
    # 阶段 2: 依次 gen(), 取第 37 组
    for i, (a_d, b_d) in enumerate(tests):
        a = ''.join([str(rng.randint(0,9)) for _ in range(a_d)])
        b = ''.join([str(rng.randint(1,9))] + [str(rng.randint(0,9)) for _ in range(b_d-1)])
        if i == 37:
            return a, b, a_d, b_d
    return None

def run_cmd(ssh, cmd, timeout=120):
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode()
    err = stderr.read().decode()
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def main():
    a, b, a_d, b_d = gen_exact(50000, 10000)
    print(f"Case #37: a_digits={a_d} b_digits={b_d}")
    print(f"a[:20]={a[:20]}... a[-20:]={a[-20:]}")
    print(f"b[:20]={b[:20]}... b[-20:]={b[-20:]}")
    print(f"a len={len(a)} b len={len(b)}")

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PWD, timeout=30)
    print(f"[OK] SSH connected")

    # 写入测试数据
    sftp = ssh.open_sftp()
    inp = f"1\n{a}\n{b}\n"
    with sftp.open("/tmp/case37.in", "w") as f:
        f.write(inp)
    print(f"[OK] Wrote /tmp/case37.in ({len(inp)} bytes)")

    # 确认两个 binary 存在 (fuzz_carry_fix.py 已编译)
    rc, o, _ = run_cmd(ssh, "ls -la /home/azzr/moptm_default /home/azzr/moptm_disable 2>&1")
    print(f"binaries: {o.strip()}")

    # 运行 default
    rc1, o1, e1 = run_cmd(ssh, "cd /home/azzr && ./moptm_default < /tmp/case37.in > /tmp/o_def.txt 2>/tmp/e_def.txt; echo $?")
    rc2, o2, e2 = run_cmd(ssh, "cd /home/azzr && ./moptm_disable < /tmp/case37.in > /tmp/o_dis.txt 2>/tmp/e_dis.txt; echo $?")

    rc_len, len_out, _ = run_cmd(ssh, "wc -c < /tmp/o_def.txt; wc -c < /tmp/o_dis.txt; wc -c < /tmp/e_def.txt; wc -c < /tmp/e_dis.txt")
    print(f"\nrc1={rc1} rc2={rc2}")
    print(f"sizes: {len_out.strip()}")

    # 看前 200 字节差异
    rc_diff, diff_out, _ = run_cmd(ssh, "diff <(head -c 200 /tmp/o_def.txt) <(head -c 200 /tmp/o_dis.txt); echo '---'; tail -c 200 /tmp/o_def.txt; echo '==='; tail -c 200 /tmp/o_dis.txt")
    print(f"\nDiff (head 200):\n{diff_out}")

    # 看 stderr
    rc_e, e_out, _ = run_cmd(ssh, "cat /tmp/e_def.txt; echo '---'; cat /tmp/e_dis.txt")
    print(f"\nstderr:\n{e_out}")

    ssh.close()
    print("[DONE]")

if __name__ == "__main__":
    main()
