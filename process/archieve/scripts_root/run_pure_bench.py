"""Run pure div bench and capture timing."""
import subprocess
import sys

CUR_DIV = r"d:\precious_speed\cur_div_pure.exe"
INPUT = r"d:\precious_speed\pure_bench.in"

with open(INPUT, "rb") as f:
    data = f.read()

r = subprocess.run([CUR_DIV], input=data, capture_output=True, timeout=120)
print("rc:", r.returncode)
print("stderr:", r.stderr.decode())
print("stdout lines:", len(r.stdout.decode().strip().split("\n")) if r.stdout else 0)
