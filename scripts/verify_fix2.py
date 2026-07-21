#!/usr/bin/env python3
"""Quick verify: test 3 known FAIL cases + a few PASS cases with carry folding fix."""
import paramiko
import random

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# Known FAIL cases + some PASS cases
TEST_CASES = [
    (50000, 10000, "FAIL #37"),
    (2000, 664, "FAIL #106"),
    (500000, 250000, "FAIL #164"),
    (200, 48, "PASS #0"),
    (10000, 2000, "PASS #40"),
    (10000, 5000, "PASS #80"),
    (100, 24, "PASS #120"),
    (2000, 400, "PASS #160"),
]

def gen_case(a_digits, b_digits, rng):
    a_top = str(rng.randint(1, 9))
    a = a_top + ''.join(str(rng.randint(0, 9)) for _ in range(a_digits - 1))
    b_digits = (b_digits // 4) * 4
    if b_digits < 4: b_digits = 4
    b_top = str(rng.randint(5, 9))
    b = b_top + ''.join(str(rng.randint(0, 9)) for _ in range(b_digits - 1))
    return a, b

def run_cmd(ssh, cmd, timeout=120):
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode()
    err = stderr.read().decode()
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def main():
    rng = random.Random(42)
    cases = []
    for a_digits, b_digits, label in TEST_CASES:
        a, b = gen_case(a_digits, b_digits, rng)
        cases.append((a, b, label))

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PWD, timeout=30)
    print(f"[OK] SSH connected")

    sftp = ssh.open_sftp()
    sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
    print("[OK] Uploaded moptm_fusion.cpp")

    # Compile both versions
    rc, out, err = run_cmd(ssh,
        "cd /home/azzr && g++ -O2 -std=gnu++20 -DHINT_OP_DIV -DUSE_GMP_NEWTON moptm_fusion.cpp -o moptm_default 2>&1")
    print(f"Compile default (USE_GMP_NEWTON): rc={rc}")
    if rc != 0: print("STDOUT:", out); print("STDERR:", err); ssh.close(); return

    rc, out, err = run_cmd(ssh,
        "cd /home/azzr && g++ -O2 -std=gnu++20 -DHINT_OP_DIV moptm_fusion.cpp -o moptm_disable 2>&1")
    print(f"Compile disable: rc={rc}")
    if rc != 0: print("STDOUT:", out); print("STDERR:", err); ssh.close(); return

    # Test each case
    print("\n=== Test results ===")
    pass_cnt = 0
    fail_cnt = 0
    for i, (a, b, label) in enumerate(cases):
        # Write single-case input
        inp = f"1\n{a}\n{b}\n"
        with sftp.open(f"/tmp/case_{i}.in", "w") as f:
            f.write(inp)

        rc1, o1, e1 = run_cmd(ssh, f"cd /home/azzr && ./moptm_default < /tmp/case_{i}.in > /tmp/o_def_{i}.txt 2>/dev/null; echo $?")
        rc2, o2, e2 = run_cmd(ssh, f"cd /home/azzr && ./moptm_disable < /tmp/case_{i}.in > /tmp/o_dis_{i}.txt 2>/dev/null; echo $?")

        rc_cmp, out_cmp, _ = run_cmd(ssh, f"diff -q /tmp/o_def_{i}.txt /tmp/o_dis_{i}.txt 2>&1; echo $?")
        # diff returns 0 if same, 1 if different
        same = (rc_cmp == 0)

        rc_len, len_out, _ = run_cmd(ssh, f"wc -c < /tmp/o_def_{i}.txt; wc -c < /tmp/o_dis_{i}.txt")
        lens = len_out.strip().split('\n')

        if same:
            print(f"PASS {label}: a={len(a)} b={len(b)} len={lens[0].strip()}")
            pass_cnt += 1
        else:
            print(f"FAIL {label}: a={len(a)} b={len(b)} def={lens[0].strip()} dis={lens[1].strip()}")
            fail_cnt += 1

    print(f"\n=== {pass_cnt} PASS, {fail_cnt} FAIL ===")
    ssh.close()
    print("[DONE]")

if __name__ == "__main__":
    main()
