import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

print('=== rebuild div_v9 with -g, recachegrind, cg_annotate funcs ===')
print(run('cd /home/azzr/divbench && g++ -O2 -march=x86-64-v3 -std=c++23 -g -o div_v9g v9.cpp 2>&1 | tail -3; echo BUILT'))
print(run('cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
          '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
          '--cachegrind-out-file=cg_v9g.out ./div_v9g < bench_max.in >/dev/null 2>cg_v9g.log; '
          'grep -E "I refs|LLd misses" cg_v9g.log'))
print('=== top functions by Ir (v9.cpp) ===')
print(run('cd /home/azzr/divbench && cg_annotate --auto=yes --show=Ir cg_v9g.out 2>/dev/null | grep -E "v9.cpp" | sort -rn | head -35'))
ssh.close()
