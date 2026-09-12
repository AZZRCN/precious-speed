"""Run pure div bench multiple times for stability check."""
import subprocess

CUR_DIV = r"d:\precious_speed\cur_div_pure.exe"
INPUT = r"d:\precious_speed\pure_bench.in"

with open(INPUT, "rb") as f:
    data = f.read()

for i in range(5):
    r = subprocess.run([CUR_DIV], input=data, capture_output=True, timeout=120)
    line = r.stderr.decode().strip()
    print(f"Run {i+1}: {line}")
