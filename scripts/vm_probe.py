#!/usr/bin/env python3
import paramiko
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect("192.168.1.55", username="azzr", password="1234", timeout=15)

def run(cmd):
    _, o, e = c.exec_command(cmd, timeout=60)
    return o.read().decode(errors="replace"), e.read().decode(errors="replace")

print("--- lscpu ---")
print(run("lscpu")[0])
print("--- /proc/cpuinfo model name ---")
print(run('grep -m1 "model name" /proc/cpuinfo')[0])
print("--- /proc/cpuinfo flags (first line) ---")
print(run("grep -m1 flags /proc/cpuinfo")[0])
print("--- free -h ---")
print(run("free -h")[0])
print("--- uname ---")
print(run("uname -a")[0])
print("--- getconf CLK_TCK ---")
print(run("getconf CLK_TCK")[0])
print("--- /etc/os-release ---")
print(run("cat /etc/os-release")[0])

# Verify timing with the loop approach
inner = "for i in $(seq 1 50); do ./moptm_add < add_1M.in > /dev/null; done"
cmd = f"cd /tmp/bench && /usr/bin/time -v bash -c '{inner}' 2>&1 | grep -E 'User time|Elapsed|Maximum|Exit'"
print("--- verify: 50x moptm_add on add_1M.in ---")
print(run(cmd)[0])

# Also do a single direct run for comparison
cmd2 = "cd /tmp/bench && /usr/bin/time -v ./moptm_add < add_1M.in 2>&1 | grep -E 'User time|Elapsed|Maximum|Exit'"
print("--- direct single run moptm_add on add_1M.in ---")
print(run(cmd2)[0])

# And DIV which should be measurable directly
cmd3 = "cd /tmp/bench && /usr/bin/time -v ./moptm_div < div_1M_500k.in 2>&1 | grep -E 'User time|Elapsed|Maximum|Exit'"
print("--- direct single run moptm_div on div_1M_500k.in ---")
print(run(cmd3)[0])

c.close()
print("DONE")
