import sys
sys.path.insert(0, r"D:\precious_speed\lc_bench\vm")
from vmctl import run, put

put(r"D:\precious_speed\lc_bench\vm\d3_fuzz.py", "/home/azzr/d3_fuzz.py")
run("cd ~ && python3 ~/d3_fuzz.py 2>&1 | tail -40")
