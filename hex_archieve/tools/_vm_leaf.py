import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

print('=== FFT_LEAF_LOG sweep 10/11/12 on max & mid (Zen3 LL=32M I refs) ===')
for leaf in [10, 11, 12]:
    sedcmd = ('cd /home/azzr/divbench && sed "s/#define FFT_LEAF_LOG 11/#define FFT_LEAF_LOG '
              + str(leaf) + '/" v9.cpp > v9_leaf' + str(leaf) + '.cpp && g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_leaf'
              + str(leaf) + ' v9_leaf' + str(leaf) + '.cpp 2>&1 | tail -1')
    print('build leaf' + str(leaf) + ':', run(sedcmd).strip())
    for sz in ['max', 'mid']:
        vg = ('cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
              '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
              '--cachegrind-out-file=cg_l' + str(leaf) + '_' + sz + '.out ./v9_leaf' + str(leaf)
              + ' < bench_' + sz + '.in >/dev/null 2>cg_l' + str(leaf) + '_' + sz + '.log; '
              'grep -E "I refs|LLd misses" cg_l' + str(leaf) + '_' + sz + '.log')
        print('LEAF=' + str(leaf) + ' ' + sz + ' :: ' + run(vg).replace('\n', ' '))
ssh.close()
