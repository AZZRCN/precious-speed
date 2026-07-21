#!/usr/bin/env python3
"""测试 #106 (a=2000, b=664) 是否也因归一化触发 FAIL"""
import paramiko, random

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

def gen_exact(seed=2026, case_idx=106):
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
    return stdout.channel.recv_exit_status(), out, stderr.read().decode()

def main():
    a, b, a_d, b_d = gen_exact()
    print(f"Case #106: a_digits={a_d} b_digits={b_d}")
    print(f"b[:5]={b[:5]} (b_top={b[0]})")

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PWD, timeout=30)
    sftp = ssh.open_sftp()

    tests = [
        ("orig", b),
        ("b_top5", "5" + b[1:]),
    ]

    for name, b_mod in tests:
        inp = f"1\n{a}\n{b_mod}\n"
        with sftp.open(f"/tmp/c106_{name}.in", "w") as f:
            f.write(inp)
        run_cmd(ssh, f"cd /home/azzr && ./moptm_default < /tmp/c106_{name}.in > /tmp/c106_{name}_def.txt 2>/dev/null")
        run_cmd(ssh, f"cd /home/azzr && ./moptm_disable < /tmp/c106_{name}.in > /tmp/c106_{name}_dis.txt 2>/dev/null")
        _, cmp_out, _ = run_cmd(ssh, f"diff -q /tmp/c106_{name}_def.txt /tmp/c106_{name}_dis.txt 2>&1; echo $?")
        _, len_out, _ = run_cmd(ssh, f"wc -c < /tmp/c106_{name}_def.txt; wc -c < /tmp/c106_{name}_dis.txt")
        lens = len_out.strip().split('\n')
        same = (cmp_out.strip() == "0")
        print(f"  {name} (b_top={b_mod[0]}): {'PASS' if same else 'FAIL'}  def={lens[0].strip()} dis={lens[1].strip()}")

    ssh.close()
    print("[DONE]")

if __name__ == "__main__":
    main()
