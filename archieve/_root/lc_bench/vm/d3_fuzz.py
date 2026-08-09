import random, subprocess

random.seed(20260802)

BIN_ORIG = "~/divbench/bin/div_orig"
BIN_D3 = "~/divbench/bin/div_D3"

def rand_big(digits):
    if digits <= 0:
        return "0"
    s = [random.choice("0123456789") for _ in range(digits)]
    s[0] = random.choice("123456789")  # 首位非 0
    return "".join(s)

def gen_file(path, cases):
    with open(path, "w") as f:
        f.write(str(len(cases)) + "\n")
        for a, b in cases:
            f.write(a + " " + b + "\n")

def run_bin(binpath, infile):
    # 用文件重定向 (binary < file) 触发 mmap 读入路径, 与官方 verify 一致, 避开管道 8MB iBuffer 限制
    out = subprocess.run("%s < %s" % (binpath, infile), shell=True,
                         capture_output=True, text=True)
    return out.stdout, out.returncode, out.stderr

def compare(name, cases):
    gen_file("/tmp/fz.in", cases)
    o_stdout, o_rc, o_err = run_bin(BIN_ORIG, "/tmp/fz.in")
    d_stdout, d_rc, d_err = run_bin(BIN_D3, "/tmp/fz.in")
    if o_rc != 0 or d_rc != 0:
        print("[%s] RC mismatch orig=%d d3=%d" % (name, o_rc, d_rc))
        if o_rc != 0: print("  orig_stderr:", o_err[:300])
        if d_rc != 0: print("  d3_stderr:", d_err[:300])
        return False
    if o_stdout != d_stdout:
        ol = o_stdout.split("\n")
        dl = d_stdout.split("\n")
        for i in range(min(len(ol), len(dl))):
            if ol[i] != dl[i]:
                print("[%s] MISMATCH at line %d" % (name, i))
                print("  orig:", ol[i][:120])
                print("  d3  :", dl[i][:120])
                break
        return False
    print("[%s] PASS (%d cases)" % (name, len(cases)))
    return True

all_ok = True

sanity = [("100","7"), ("7","100"), ("123456789","987654321"),
          ("1000000000000000000000000000000","7"), ("0","5"), ("5","5")]
all_ok &= compare("sanity", sanity)

cases = []
for _ in range(2000):
    d2 = random.randint(1, 200)
    d1 = max(1, int(d2 * random.uniform(0.5, 10.0)))
    cases.append((rand_big(d1), rand_big(d2)))
all_ok &= compare("small", cases)

cases = []
for _ in range(200):
    d2 = random.randint(500, 5000)
    d1 = max(1, int(d2 * random.uniform(0.5, 8.0)))
    cases.append((rand_big(d1), rand_big(d2)))
all_ok &= compare("medium", cases)

cases = []
for _ in range(30):
    d2 = random.randint(10000, 100000)
    d1 = max(1, int(d2 * random.uniform(0.5, 5.0)))
    cases.append((rand_big(d1), rand_big(d2)))
all_ok &= compare("large", cases)

cases = []
for _ in range(10):
    d2 = random.randint(200000, 800000)
    d1 = max(1, int(d2 * random.uniform(1.0, 3.0)))
    cases.append((rand_big(d1), rand_big(d2)))
all_ok &= compare("huge", cases)

cases = []
for _ in range(20):
    d2 = random.randint(50000, 200000)
    d1 = max(1, int(d2 * random.uniform(5.0, 30.0)))
    cases.append((rand_big(d1), rand_big(d2)))
all_ok &= compare("many_blocks", cases)

cases = []
for _ in range(100):
    d = random.randint(5, 3000)
    b = "9" * d
    cases.append((rand_big(random.randint(1, int(d*1.5))), b))
for _ in range(100):
    d = random.randint(5, 3000)
    b = "9" * d
    cases.append(("9"*d, b))
    cases.append(("9"*d + "0", b))
    cases.append(("9"*(d-1) + "8", b))
for _ in range(50):
    d = random.randint(5, 2000)
    limbs = [0]*d
    limbs[random.randint(0,d-1)] = random.randint(1, 9999)
    b = "".join(str(x) for x in limbs).lstrip("0") or "0"
    cases.append((rand_big(random.randint(1, int(d*1.2))), b))
all_ok &= compare("adversarial", cases)

print("=== FUZZ RESULT:", "ALL PASS" if all_ok else "FAILURES FOUND", "===")
