import paramiko, re
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

# compile all plausible DIV variants (unified flags)
print('=== compile DIV candidate variants ===')
builds = [
    ('hb_div',   '/home/azzr/hexbench/div/div.cpp'),
    ('hb_ib24',  '/home/azzr/hexbench/div/ib24.cpp'),
    ('hb_ib96',  '/home/azzr/hexbench/div/ib96.cpp'),
    ('gccg_div', '/home/azzr/gccg/div.cpp'),
]
for tag, src in builds:
    out = run('cd /home/azzr/divbench && g++ -O2 -march=x86-64-v3 -std=c++23 -o %s %s 2>&1 | tail -2; ls -la %s 2>/dev/null | awk "{print \\$5, \\$9}"' % (tag, src, tag))
    print(tag, '::', out.strip().replace('\n', ' | '))

# candidates to profile: compiled above + existing div_v9 + probe_v binary
cands = ['div_v9', 'hb_div', 'hb_ib24', 'hb_ib96', 'gccg_div', '/home/azzr/hexbench/div/probe_v']
print()
print('=== Zen3 LL=32M/16 cachegrind, bench_max.in (1.6M hex-digit div) ===')
rows = {}
for tag in cands:
    bname = tag.split('/')[-1]
    cmd = (
        'cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
        '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
        '--cachegrind-out-file=cg_%s.out %s < bench_max.in >/dev/null 2> cg_%s.log; '
        'grep -E "I refs|D refs|LLd misses|LL misses" cg_%s.log' % (bname, tag, bname, bname)
    )
    print('--- %s ---' % tag)
    print(run(cmd))
ssh.close()
