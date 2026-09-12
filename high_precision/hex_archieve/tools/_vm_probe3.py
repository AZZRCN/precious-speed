import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("192.168.1.55", username="azzr", password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors="replace") + e.read().decode(errors="replace")

print("=== hexbench/data/div ===")
print(run("ls /home/azzr/hexbench/data/div/ 2>/dev/null | head -40; echo count:; ls /home/azzr/hexbench/data/div/ 2>/dev/null | wc -l"))
print("=== hexbench/oracle ===")
print(run("ls -la /home/azzr/hexbench/oracle/ 2>/dev/null | head; echo '--- .py head ---'; for f in /home/azzr/hexbench/oracle/*.py; do echo \"## $f\"; head -45 \"$f\"; done 2>/dev/null | head -70"))
print("=== hexbench/ref ===")
print(run("ls -la /home/azzr/hexbench/ref/ 2>/dev/null | head -20"))
print("=== hexbench/div ===")
print(run("ls -la /home/azzr/hexbench/div/ 2>/dev/null | head -20"))
print("=== sample .in (div) ===")
print(run("F=$(ls /home/azzr/hexbench/data/div/*.in 2>/dev/null | head -1); echo \"$F\"; head -c 300 \"$F\""))
ssh.close()
