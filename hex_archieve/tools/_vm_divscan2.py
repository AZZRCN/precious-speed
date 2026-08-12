import paramiko, re
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

# HEX div variants only (absolute paths). gccg_div is DEC, probe_v AVX-512 crash -> excluded
cands = [
    ('div_v9',   '/home/azzr/divbench/div_v9'),
    ('hb_div',   '/home/azzr/divbench/hb_div'),
    ('hb_ib24',  '/home/azzr/divbench/hb_ib24'),
    ('hb_ib96',  '/home/azzr/divbench/hb_ib96'),
]
print('=== Zen3 LL=32M/16 cachegrind, bench_max.in (1.6M hex-digit div) ===')
rows = {}
for tag, path in cands:
    cmd = (
        'cd /home/azzr/divbench && valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes '
        '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
        '--cachegrind-out-file=cg_%s.out %s < bench_max.in >/dev/null 2> cg_%s.log; '
        'grep -E "I refs|D refs|LLd misses|LL misses" cg_%s.log' % (tag, path, tag, tag)
    )
    print('--- %s ---' % tag)
    print(run(cmd))
ssh.close()
