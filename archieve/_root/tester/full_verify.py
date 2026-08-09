#!/usr/bin/env python3
"""完整正确性验证: 生成随机测试数据, 用 Python 大整数除法作参考."""
import subprocess, random, sys, os
sys.set_int_max_str_digits(0)  # 解除整数转字符串长度限制

VM_CMD = "ssh azzr@10.144.33.157"
REMOTE_DIR = "~/precious-speed"

def gen_cases(n, max_digits_a, max_digits_b):
    """生成 n 个测试用例, 返回 (input_str, ref_output_str)."""
    lines_in = [str(n)]
    lines_out = []
    for _ in range(n):
        da = random.randint(1, max_digits_a)
        db = random.randint(1, max_digits_b)
        a = random.randint(0, 10**da - 1) if da > 1 else random.randint(0, 9)
        b = random.randint(1, 10**db - 1) if db > 1 else random.randint(1, 9)
        lines_in.append(f"{a} {b}")
        q, r = divmod(a, b)
        lines_out.append(f"{q} {r}")
    return "\n".join(lines_in) + "\n", "\n".join(lines_out) + "\n"

def run_remote(input_str):
    """在 VM 上运行 cur_div, 返回 stdout."""
    proc = subprocess.run(
        ["ssh", "azzr@10.144.33.157", f"cd {REMOTE_DIR} && timeout 60 ./cur_div"],
        input=input_str, capture_output=True, text=True, timeout=120
    )
    return proc.stdout

def compare(out, ref):
    """比较输出 (逐行, 忽略尾部空白)."""
    out_lines = out.strip().split("\n")
    ref_lines = ref.strip().split("\n")
    if len(out_lines) != len(ref_lines):
        return False, f"行数不同: out={len(out_lines)} ref={len(ref_lines)}"
    for i, (o, r) in enumerate(zip(out_lines, ref_lines)):
        if o.strip() != r.strip():
            return False, f"第{i+1}行不同: out={o[:50]}... ref={r[:50]}..."
    return True, "OK"

def main():
    random.seed(42)
    test_suites = [
        ("tiny",    50,   5,   3),   # 50 用例, a≤5位, b≤3位
        ("small",   100,  20,  10),  # 100 用例, a≤20位, b≤10位
        ("medium",  50,   200, 100), # 50 用例, a≤200位, b≤100位
        ("large",   20,   2000, 1000), # 20 用例, a≤2000位, b≤1000位
        ("xlarge",  10,   5000, 2000), # 10 用例, a≤5000位, b≤2000位
        ("1to1",    30,   100, 100),   # a≈b 规模
        ("a_lt_b",  30,   50,  200),   # a < b (商=0)
        ("a_eq_b",  10,   100, 100),   # a = b (商=1, 余=0) — 特殊构造
    ]

    all_pass = True
    for name, n, da, db in test_suites:
        print(f"=== {name} (n={n}, da≤{da}, db≤{db}) ===", flush=True)
        inp, ref = gen_cases(n, da, db)
        if name == "a_eq_b":
            # 特殊: a = b
            lines = [str(n)]
            ref_lines = []
            for _ in range(n):
                b = random.randint(10**(da-1), 10**da - 1)
                lines.append(f"{b} {b}")
                ref_lines.append("1 0")
            inp = "\n".join(lines) + "\n"
            ref = "\n".join(ref_lines) + "\n"

        out = run_remote(inp)
        ok, msg = compare(out, ref)
        if ok:
            print(f"  PASS ({msg})")
        else:
            print(f"  FAIL: {msg}")
            all_pass = False
            # 保存失败用例
            with open(f"/tmp/fail_{name}.in", "w") as f: f.write(inp)
            with open(f"/tmp/fail_{name}.out", "w") as f: f.write(out)
            with open(f"/tmp/fail_{name}.ref", "w") as f: f.write(ref)

    print("\n=== 总结 ===")
    if all_pass:
        print("全部通过!")
    else:
        print("有失败用例!")
        sys.exit(1)

if __name__ == "__main__":
    main()
