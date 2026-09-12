import paramiko, re
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.55', username='azzr', password="REDACTED", timeout=30)
def run(c):
    _, o, e = ssh.exec_command(c)
    return o.read().decode(errors='replace') + e.read().decode(errors='replace')

tags = ['div_v9','ref_v9p','ref_v8p','ref_v5p','web_div2']
keys = ['I refs','D refs','I1  misses','D1  misses','LLi misses','LLd misses','LL misses','LL refs','Branches','Mispredicts']
print('=== Zen3 LL=32M/16 cachegrind, bench_max.in (A=1.6M hex-digits div) ===')
rows = {}
for tag in tags:
    log = run('cat /home/azzr/divbench/cg_%s.log' % tag)
    d = {}
    for key in keys:
        m = re.search(re.escape(key) + r':\s*([\d,]+)', log)
        if m:
            d[key] = int(m.group(1).replace(',',''))
    rows[tag] = d
    print('--- %s ---' % tag)
    for k in ['I refs','D refs','LLd misses','LL misses','LL refs','Mispredicts']:
        if k in d:
            print('   %-12s %d' % (k, d[k]))

print()
print('=== I refs ranking (lower = better) ===')
base = rows['div_v9'].get('I refs', 1)
for tag in sorted(rows, key=lambda t: rows[t].get('I refs', 1<<60)):
    ir = rows[tag].get('I refs', 0)
    print('   %-10s I_refs=%-13d (%.3fx vs div_v9)  D_refs=%-13d  LLd_misses=%-9d  LL_misses=%d' % (
        tag, ir, ir/base, rows[tag].get('D refs',0), rows[tag].get('LLd misses',0), rows[tag].get('LL misses',0)))
ssh.close()
