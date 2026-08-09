import sys
sys.path.insert(0, r"D:\precious_speed\lc_bench\vm")
from vmctl import run, put

put(r"D:\precious_speed\lc_bench\vm\patch_div3.py", "/home/azzr/patch_div3.py")

run("cp ~/divbench/src/div_orig.cpp ~/divbench/src/div_D3.cpp && cd ~/divbench && python3 ~/patch_div3.py")
run("cd ~/divbench && g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/div_D3 src/div_D3.cpp 2>&1 | head -30; echo BUILD_RC=${PIPESTATUS[0]}")
run(r"echo === input format (medium_00 head) ===; head -c 200 ~/lcp/big_integer/division_of_big_integers/in/medium_00.in; echo")
run(r"echo === output format (3 tiny cases) ===; printf '100 7\n1000000000000000000000000000000 7\n123456789 987654321\n' > /tmp/fz_fmt.in && ~/divbench/bin/div_orig < /tmp/fz_fmt.in")
run("cd ~/divbench && python3 ~/verify_official.py div bin/div_D3 2>&1 | tail -6")
