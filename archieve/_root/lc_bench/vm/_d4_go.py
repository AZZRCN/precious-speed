import sys
sys.path.insert(0, r'D:\precious_speed\lc_bench\vm')
from vmctl import put, run

put(r'D:\precious_speed\best\div_D4.cpp', '/home/azzr/divbench/src/div_D4.cpp')
put(r'D:\precious_speed\lc_bench\vm\d4_build_stress.py', '/home/azzr/d4_build_stress.py')
print('uploaded both files', flush=True)

rc, out, err = run('python3 ~/d4_build_stress.py', timeout=1800, verbose=True)
print('REMOTE_RC=', rc)
