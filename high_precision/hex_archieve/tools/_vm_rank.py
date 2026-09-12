import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
sftp = ssh.open_sftp()
sftp.put('work/div/v9.cpp', '/home/azzr/divbench/v9.cpp')
sftp.close()

def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

CXX = 'g++ -O2 -march=x86-64-v3 -std=c++23'
cands = [
    ('div_v9', 'v9.cpp'),
    ('ref_v9p', '/home/azzr/hexbench/ref/v9p.cpp'),
    ('ref_v8p', '/home/azzr/hexbench/ref/v8p.cpp'),
    ('ref_v5p', '/home/azzr/hexbench/ref/v5p.cpp'),
    ('web_div2', '/home/azzr/hexbench/div/div.cpp'),
]
print('=== compile candidates (unified flags) ===')
for tag, src in cands:
    out = run('cd /home/azzr/divbench && %s -o %s %s 2>&1 | tail -2; ls -la %s 2>/dev/null | awk "{print \\$5, \\$9}"' % (CXX, tag, src, tag))
    print(tag, '::', out.strip().replace('\n', ' | '))

print()
print('=== Zen3 LL=32M/16 cachegrind on bench_max.in (1.6M-bit div) ===')
for tag in [c[0] for c in cands]:
    cmd = (
        'cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
        '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
        '--cachegrind-out-file=cg_%s.out ./%s < bench_max.in >/dev/null 2> cg_%s.log; '
        'grep -E "I   refs|D   refs|LLd misses|LL  misses" cg_%s.log' % (tag, tag, tag, tag)
    )
    print('--- %s ---' % tag)
    print(run(cmd))
ssh.close()
