import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

put(r"D:\hex_precious_speed\work\div\r8_validate2.cpp", "/tmp/r8_validate2.cpp")
cmd = "cd /tmp; g++ -O2 -std=c++23 -o r8_validate2 r8_validate2.cpp 2>&1 | tail -3; echo B=$?; ./r8_validate2; echo R=$?"
rc, out, err = run(cmd, timeout=90)
print("RC", rc); print(out)
if err.strip(): print("ERR", err[:1200])
