import subprocess
import sys

exe = r"d:\precious_speed\tester\cur_div.exe"
in_file = r"d:\precious_speed\tester\failures\div_div_medium_98_0.in"

with open(in_file, 'rb') as f:
    in_data = f.read()

proc = subprocess.Popen(exe, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
out_data, err_data = proc.communicate(in_data, timeout=60)

print(f"stdout: {len(out_data)} bytes")
print(f"stderr: {len(err_data)} bytes")
if err_data:
    err_text = err_data.decode("utf-8", errors="replace")
    print(f"stderr (full):")
    print(err_text)
print(f"return code: {proc.returncode}")