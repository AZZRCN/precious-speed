import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

put(r"D:\hex_precious_speed\work\div\r8_validate.cpp", "/tmp/r8_validate.cpp")
cmd = (
    "cd /tmp; g++ -O2 -std=c++23 -o r8_validate r8_validate.cpp 2>&1 | tail -3; echo BUILD_RC=$?; "
    "./r8_validate; echo RUN_RC=$?"
)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
