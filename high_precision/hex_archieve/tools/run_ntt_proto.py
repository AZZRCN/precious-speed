import vm, sys, time

LOCAL = r"D:\hex_precious_speed\work\mul\ntt_mul.cpp"
VERIFY = r"""import subprocess, glob, os
binp="/tmp/ntt_mul"
indir=os.path.expanduser("~/hexbench/data/mul")
cases=sorted(glob.glob(indir+"/*.in"))
ok=bad=0
for f in cases:
    base=os.path.basename(f)[:-3]
    expf=os.path.join(indir, base+".exp")
    explines=[l.strip() for l in open(expf).read().split("\n") if l.strip()]
    data=open(f).read()   # raw: whitespace-separated tokens, "T" then T pairs
    got=subprocess.run([binp],input=data,capture_output=True,text=True).stdout
    gotlines=[l.strip() for l in got.split("\n") if l.strip()]
    if gotlines==explines:
        ok+=1
    else:
        bad+=1
        print("BAD", base, "explines", len(explines), "gotlines", len(gotlines))
        for i in range(min(len(explines),len(gotlines))):
            if explines[i]!=gotlines[i]:
                print("   diff@%d explen=%d gotlen=%d"%(i,len(explines[i]),len(gotlines[i])))
                print("   exp:",explines[i][:50])
                print("   got:",gotlines[i][:50])
                break
print(f"RESULT ok={ok} bad={bad} total={len(cases)}")
"""

with open(r"D:\hex_precious_speed\tools\_verify_ntt.py","w") as fp:
    fp.write(VERIFY)

vm.put(LOCAL, "/tmp/ntt_mul.cpp")
vm.put(r"D:\hex_precious_speed\tools\_verify_ntt.py", "/tmp/verify_ntt.py")
print("=== build ===")
print(vm.run("g++ -O2 -march=x86-64-v3 -std=c++17 -o /tmp/ntt_mul /tmp/ntt_mul.cpp 2>&1; echo BUILD=$?"))
print("=== verify ===")
print(vm.run("cd /tmp && python3 verify_ntt.py 2>&1; echo VERIFY=$?"))
