import sys
sys.path.insert(0, 'D:/hex_precious_speed/tools')
import vmctl

WS = '/home/azzr/hexbench'

vmctl.put('D:/hex_precious_speed/tools/hexcheck.py', f'{WS}/oracle/hexcheck.py')
vmctl.put('D:/hex_precious_speed/work/add/v1.cpp', f'{WS}/add/v1.cpp')
vmctl.put('D:/hex_precious_speed/web_best/add.cpp', f'{WS}/add/web.cpp')

vmctl.run(f"sed 's/#pragma GCC optimize.*//' {WS}/add/web.cpp > {WS}/add/web_o2.cpp && echo STRIP_OK")

print('=== compile ===')
vmctl.run(f'g++ -O2 -std=c++23 -march=x86-64-v3 -o {WS}/add/v1 {WS}/add/v1.cpp 2>&1 | tail -8')
vmctl.run(f'g++ -O2 -std=c++23 -march=x86-64-v3 -o {WS}/add/web {WS}/add/web.cpp 2>&1 | tail -8')
vmctl.run(f'g++ -O2 -std=c++23 -march=x86-64-v3 -o {WS}/add/web_o2 {WS}/add/web_o2.cpp 2>&1 | tail -8')

print('=== gen + verify ===')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py gen add 1 t.in t.exp')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py run ./v1 t.in t.exp')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py run ./web t.in t.exp')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py run ./web_o2 t.in t.exp')

print('=== bench (callgrind Ir) ===')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py genbench add bench.in')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py bench ./v1 bench.in --cg')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py bench ./web bench.in --cg')
vmctl.run(f'cd {WS}/add && python3 {WS}/oracle/hexcheck.py bench ./web_o2 bench.in --cg')
print('=== DONE ===')
