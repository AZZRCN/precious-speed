#!/usr/bin/env python3
"""Debug GMP cyclic Newton bug: compare absInvNewton vs absInvNewtonGMP on FAIL cases."""
import paramiko
import base64
import random
import sys

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# FAIL cases from verify_fix.log
FAIL_CASES = [
    (50000, 10000),   # #37
    (2000, 664),      # #106
    (500000, 250000), # #164
]

def gen_normalized_digits(num_digits, rng):
    """Generate random digits with top limb >= 5000 (HALF_BASE) for b normalization.
    BASE=10^4, so digits must be multiple of 4, top digit 5-9."""
    # Ensure multiple of 4
    num_digits = (num_digits // 4) * 4
    if num_digits < 4:
        num_digits = 4
    # Top digit 5-9
    top = rng.randint(5, 9)
    rest = [rng.randint(0, 9) for _ in range(num_digits - 1)]
    return str(top) + ''.join(str(d) for d in rest)

def gen_test_data():
    """Generate test data for HINT_OP_TESTNEWTON."""
    rng = random.Random(42)
    lines = [str(len(FAIL_CASES))]
    for a_digits, b_digits in FAIL_CASES:
        a = gen_normalized_digits(a_digits, rng)
        b = gen_normalized_digits(b_digits, rng)
        lines.append(a)
        lines.append(b)
    return '\n'.join(lines) + '\n'

def main():
    test_data = gen_test_data()
    print(f"Generated {len(FAIL_CASES)} test cases")
    print(f"Test data size: {len(test_data)} bytes")

    # Connect to VM
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PWD, timeout=30)
    print(f"[OK] SSH connected to {USER}@{HOST}")

    # Upload moptm_fusion.cpp
    sftp = ssh.open_sftp()
    sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
    print("[OK] Uploaded moptm_fusion.cpp")

    # Upload test data
    with sftp.open("/home/azzr/testnewton_fail.txt", "w") as f:
        f.write(test_data)
    print("[OK] Uploaded test data")

    # Compile with HINT_OP_TESTNEWTON (absInvNewtonGMP uses cyclic by default)
    cmd = "cd /home/azzr && g++ -O2 -std=gnu++20 -DHINT_OP_TESTNEWTON moptm_fusion.cpp -o testnewton_fail.exe 2>&1"
    stdin, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode()
    err = stderr.read().decode()
    print(f"Compile: {out}{err}")
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"[FAIL] Compile error, rc={rc}")
        ssh.close()
        return

    # Run test
    cmd = "cd /home/azzr && ./testnewton_fail.exe < testnewton_fail.txt 2>&1"
    stdin, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode()
    err = stderr.read().decode()
    print(f"\n=== Test output ===\n{out}")
    if err:
        print(f"=== Stderr ===\n{err}")

    ssh.close()
    print("[DONE]")

if __name__ == "__main__":
    main()
