import paramiko, re
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("192.168.1.55", username="azzr", password="REDACTED", timeout=30)
def run(c, timeout=600):
    _, o, e = ssh.exec_command(c, timeout=timeout)
    return o.read().decode(errors="replace") + e.read().decode(errors="replace")

# 0) 确认 perf 可用
print("perf:", run("perf --version 2>&1; echo paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)").strip())

# 1) 生成 HEX 基准输入 (max / mid / many)
for which in ["max", "mid", "many"]:
    print(run(f"cd /home/azzr/divbench && python3 hexcheck.py genbench div {which} bench_{which}.in 2>&1").strip())

bins = ["div_v8", "div_v9", "div_v10", "web_div"]
benches = ["max", "mid", "many"]
REPS = 10

def perf_instr(benchin, binpath):
    instr = []
    wall = []
    for _ in range(REPS):
        r = run(f"cd /home/azzr/divbench && perf stat -e instructions:u -r 1 ./{binpath} < {benchin} > /dev/null 2> /tmp/pf.txt; cat /tmp/pf.txt", timeout=300)
        m = re.search(r'([\d,]+)\s+instructions:u', r)
        wm = re.search(r'([\d.]+)\s+seconds time elapsed', r)
        if m:
            instr.append(int(m.group(1).replace(',', '')))
        if wm:
            wall.append(float(wm.group(1)))
    return (min(instr) if instr else -1, (min(wall) if wall else -1.0))

results = {}
for b in bins:
    results[b] = {}
    for wb in benches:
        ins, wl = perf_instr(f"bench_{wb}.in", b)
        results[b][wb] = (ins, wl)
        print(f"  {b:9s} {wb:5s}  Ir(min)={ins:>14,}  wall(min)={wl:.3f}s", flush=True)

# 2) 打印 ratio-of-mins 表 (相对 v8)
print("\n=== ratio-of-mins (相对 div_v8, 数值越小越好) ===")
print(f"{'binary':10s} {'max_Ir':>10s} {'mid_Ir':>10s} {'many_Ir':>10s} {'max_wall':>10s}")
base = results["div_v8"]
for b in bins:
    row = []
    for wb in benches:
        ins, wl = results[b][wb]
        bins_, wl_ = results["div_v8"][wb]
        ri = (ins/bins_) if bins_ else 0
        rw = (wl/wl_) if wl_ else 0
        row.append(f"{ri:8.3f}")
    # wall row for max
    _, wmax = results[b]["max"]
    _, wmaxb = base["max"]
    print(f"{b:10s} " + " ".join(row) + f"   {wmax/wmaxb:8.3f}")
ssh.close()
print("\nDONE")
