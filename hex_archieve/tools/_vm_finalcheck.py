import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

# ensure v9_leaf10 (LEAF=10) and history baseline hb_div (web_best/div.cpp) built
print(run('cd /home/azzr/divbench && '
          'sed "s/#define FFT_LEAF_LOG 11/#define FFT_LEAF_LOG 10/" v9.cpp > v9_leaf10.cpp && '
          'g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_leaf10 v9_leaf10.cpp 2>&1 | tail -1; echo RC_leaf=$?; '
          'g++ -O2 -march=x86-64-v3 -std=c++23 -o hb_div /home/azzr/hexbench/div/div.cpp 2>&1 | tail -1; echo RC_hb=$?'))

print()
print('=== 27 official HEX div cases: v9_leaf10 vs hb_div ===')
for tag in ['v9_leaf10', 'hb_div']:
    cmd = ('cd /home/azzr/divbench && ok=0; bad=0; '
           'for f in /home/azzr/hexbench/data/div/*.in; do '
           'e=${f%.in}.exp; python3 hexcheck.py run ./{t} "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1)); '
           'done; echo "OFFICIAL {t}: OK=$ok BAD=$bad"').format(t=tag)
    print(tag, '::', run(cmd).strip())

print()
print('=== Zen3 LL=32M/16 cachegrind I refs (LEAF=10 candidate vs history 438M) ===')
specs = [('v9_leaf10','max'), ('v9_leaf10','mid'), ('v9_leaf10','large'),
         ('hb_div','max'), ('hb_div','mid'), ('hb_div','large')]
for bin, sz in specs:
    inf = '/home/azzr/hexbench/data/div/large_00.in' if sz == 'large' else f'bench_{sz}.in'
    vg = (f'cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
          f'--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
          f'--cachegrind-out-file=cg_{bin}_{sz}.out ./{bin} < {inf} >/dev/null 2>cg_{bin}_{sz}.log; '
          f'grep -E "I refs|LLd misses" cg_{bin}_{sz}.log')
    print(f'{bin:12s} {sz:5s} :: {run(vg).replace(chr(10), " ")}')
ssh.close()
