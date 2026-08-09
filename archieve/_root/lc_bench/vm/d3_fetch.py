import sys, os
sys.path.insert(0, r'D:\precious_speed\lc_bench\vm')
from vmctl import client, run

remote = '/home/azzr/divbench/src/div_D3.cpp'
local = r'D:\precious_speed\best\div_D3.cpp'
c = client()
sftp = c.open_sftp()
sftp.get(remote, local)
sftp.close(); c.close()
print('fetched div_D3.cpp ->', local, 'size=', os.path.getsize(local))

# clean VM debug artifacts + temp inputs
run('rm -f /tmp/case.in /tmp/gen_* /tmp/cc* /tmp/stress_*.in 2>/dev/null')
run('rm -f ~/divbench/bin/div_D3_dbg ~/divbench/src/div_D3_dbg.cpp ~/divbench/src/div_D3_dbg_src.cpp 2>/dev/null')
print('vm cleaned')
