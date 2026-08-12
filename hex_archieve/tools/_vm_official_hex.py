import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("192.168.1.55", username="azzr", password="REDACTED", timeout=30)
def run(c, timeout=600):
    _, o, e = ssh.exec_command(c, timeout=timeout)
    return o.read().decode(errors="replace") + e.read().decode(errors="replace")

DATA = "/home/azzr/hexbench/data/div"
cases = run(f"ls {DATA}/*.in").split()
cases = [c.split('/')[-1][:-3] for c in cases if c.strip()]

bins = ["div_v8", "div_v9", "div_v10", "web_div"]
for b in bins:
    print(f"\n########## {b} ##########")
    allok = True
    for c in sorted(cases):
        out = run(f"cd /home/azzr/divbench && python3 hexcheck.py run ./{b} {DATA}/{c}.in {DATA}/{c}.exp 2>&1", timeout=300)
        line = [l for l in out.strip().splitlines() if l.startswith(("OK", "BAD", "RUN FAIL"))]
        verdict = line[0] if line else out.strip()[:80]
        # 只打印 BAD/FAIL, OK 汇总
        if verdict.startswith("OK"):
            pass
        else:
            allok = False
            print(f"  {c}: {verdict}")
    print(f"  >>> {b}: {'ALL OK (27/27)' if allok else 'HAS FAILURES (see above)'}")
ssh.close()
print("\nDONE")
