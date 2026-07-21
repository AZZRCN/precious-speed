#!/usr/bin/env python3
"""测试 b_top=5 (不归一化) vs b_top=4 (归一化) 是否触发 FAIL"""
import paramiko, random

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

def gen_exact(seed=2026, case_idx=37):
    """完全复现 fuzz_carry_fix.py 的逻辑"""
    rng = random.Random(seed)
    tests = []
    for _ in range(200):
        a_d = rng.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d = a_d // rng.choice([2, 3, 4, 5])
        b_d = max(b_d, 10)
        b_d = (b_d // 4) * 4
        if b_d < 4: b_d = 4
        tests.append((a_d, b_d))
    for i, (a_d, b_d) in enumerate(tests):
        a = ''.join([str(rng.randint(0,9)) for _ in range(a_d)])
        b = ''.join([str(rng.randint(1,9))] + [str(rng.randint(0,9)) for _ in range(b_d-1)])
        if i == case_idx:
            return a, b, a_d, b_d
    return None

def run_cmd(ssh, cmd, timeout=120):
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode()
    err = stderr.read().decode()
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def main():
    a, b, a_d, b_d = gen_exact()
    print(f"Original: b[:5]={b[:5]} (b_top={b[0]})")

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PWD, timeout=30)
    sftp = ssh.open_sftp()

    # 测试 1: 原始 b (b_top=4, 归一化)
    # 测试 2: 修改 b_top=5 (不归一化)
    # 测试 3: 修改 b_top=9 (不归一化)
    tests = [
        ("orig_b4", b),
        ("b_top5", "5" + b[1:]),
        ("b_top9", "9" + b[1:]),
    ]

    for name, b_mod in tests:
        inp = f"1\n{a}\n{b_mod}\n"
        with sftp.open(f"/tmp/{name}.in", "w") as f:
            f.write(inp)

        rc1, _, _ = run_cmd(ssh, f"cd /home/azzr && ./moptm_default < /tmp/{name}.in > /tmp/{name}_def.txt 2>/dev/null")
        rc2, _, _ = run_cmd(ssh, f"cd /home/azzr && ./moptm_disable < /tmp/{name}.in > /tmp/{name}_dis.txt 2>/dev/null")

        rc_cmp, cmp_out, _ = run_cmd(ssh, f"diff -q /tmp/{name}_def.txt /tmp/{name}_dis.txt 2>&1; echo $?")
        rc_len, len_out, _ = run_cmd(ssh, f"wc -c < /tmp/{name}_def.txt; wc -c < /tmp/{name}_dis.txt")
        lens = len_out.strip().split('\n')
        same = (cmp_out.strip() == "0")
        status = "PASS" if same else "FAIL"
        print(f"{name} (b_top={b_mod[0]}): {status}  def={lens[0].strip()} dis={lens[1].strip()}")

    ssh.close()
    print("[DONE]")

if __name__ == "__main__":
    main()
