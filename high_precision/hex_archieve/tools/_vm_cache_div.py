import paramiko, re
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("192.168.1.55", username="azzr", password="REDACTED", timeout=30)
sftp = ssh.open_sftp()
sftp.put("D:/hex_precious_speed/tools/hexcheck.py", "/home/azzr/divbench/hexcheck.py")
sftp.close()
def run(c, timeout=600):
    _, o, e = ssh.exec_command(c, timeout=timeout)
    return o.read().decode(errors="replace") + e.read().decode(errors="replace")

# 1) 重新生成 many
print(run("cd /home/azzr/divbench && python3 hexcheck.py genbench div many bench_many.in 2>&1").strip())

bins = ["div_v8", "div_v9", "div_v10", "web_div"]
REPS = 10

def meas_ir(b, benchin):
    ins = []
    for _ in range(REPS):
        r = run(f"cd /home/azzr/divbench && perf stat -e instructions:u -r 1 ./{b} < {benchin} > /dev/null 2> /tmp/pf.txt; cat /tmp/pf.txt", timeout=300)
        m = re.search(r'([\d,]+)\s+instructions:u', r)
        if m: ins.append(int(m.group(1).replace(',', '')))
    return min(ins) if ins else -1

print("\n=== many 重测 (Ir min-of-10) ===")
ir = {b: meas_ir(b, "bench_many.in") for b in bins}
v8m = ir["div_v8"]
for b in bins:
    print(f"  {b:9s} many Ir(min)={ir[b]:>14,}  ({ir[b]/v8m:.3f}x of v8)")

# 2) Zen3 几何 cachegrind (max 用例)
print("\n=== cachegrind @ Zen3 geom (I1=32K/8 D1=32K/8 LL=32M/16) on bench_max.in ===")
for b in bins:
    cmd = ("cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes "
           "--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 "
           f"./{b} < bench_max.in > /dev/null 2> /tmp/cg_{b}.txt; "
           "grep -E 'refs:|misses:' /tmp/cg_%s.txt" % b)
    out = run(cmd, timeout=600)
    lines = [l.strip() for l in out.splitlines() if re.search(r'(I refs|D refs|I1 misses|LLi misses|D1 misses|LLd misses):', l)]
    print(f"--- {b} ---")
    for l in lines[:12]:
        print("  " + l)
ssh.close()
print("\nDONE")
