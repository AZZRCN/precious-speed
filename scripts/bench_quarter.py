#!/usr/bin/env python3
"""对比 default (2块) vs QUARTER (4块) 的性能"""
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh
import random, time

# 编译两个版本
configs = [
    ("default",  "-DDISABLE_2NXN_CYCLIC"),
    ("quarter",  "-DDISABLE_2NXN_CYCLIC -DDIV_MU_IN_QUARTER"),
]

print("=== Upload + Compile ===")
ssh.upload("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
for name, flags in configs:
    exe = f"/home/azzr/moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} /home/azzr/moptm_fusion.cpp -o {exe} -pthread 2>&1"
    rc, out, err = ssh.run(cmd, timeout=300)
    status = "OK" if rc == 0 else f"FAIL: {out[:200]}"
    print(f"  {name}: {status}")

# 生成测试用例并 SFTP 上传
def gen_and_upload(a_d, b_d, seed=42):
    random.seed(seed)
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
    test_input = f"1\n{a}\n{b}\n"
    # 写到本地临时文件
    local_path = f"d:/precious_speed/_test_input.txt"
    with open(local_path, "w") as f:
        f.write(test_input)
    # SFTP 上传
    ssh.upload(local_path, "/tmp/test_input.txt")
    return local_path

# 测试用例
test_cases = [
    ("10k/5k",    10000,   5000),
    ("50k/25k",   50000,  25000),
    ("100k/50k", 100000,  50000),
    ("200k/100k",200000, 100000),
]

print(f"\n{'Config':<12} {'Test Case':<12} {'Best (s)':<12} {'Speedup':<10}")
print("-" * 50)

for tc_name, a_d, b_d in test_cases:
    gen_and_upload(a_d, b_d)
    baseline_time = None
    for name, flags in configs:
        exe = f"/home/azzr/moptm_{name}"
        # 运行 5 次取最好
        cmd = f"""
best=999999
for i in $(seq 1 5); do
    t=$(/usr/bin/time -f '%e' {exe} < /tmp/test_input.txt 2>&1 >/dev/null)
    python3 -c "exit(0 if float('$t') < $best else 1)" && best=$t
done
echo $best
"""
        rc, out, err = ssh.run(cmd, timeout=300)
        try:
            t = float(out.strip().split('\n')[-1])
        except:
            t = None
        
        if t is not None:
            speedup = ""
            if name == "default":
                baseline_time = t
            elif baseline_time:
                speedup = f"{baseline_time/t:.2f}x"
            print(f"{name:<12} {tc_name:<12} {t:<12.3f} {speedup:<10}")
        else:
            print(f"{name:<12} {tc_name:<12} {'FAIL':<12} {'-':<10}")
    print()

print("Done!")
