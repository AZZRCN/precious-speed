#!/usr/bin/env python3
"""Large-scale fuzz + full benchmark on all LC test points."""
import paramiko

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

SCRIPT = r'''
GXX=/usr/bin/g++-11

echo "=== Step 1: Large-scale fuzz (10000 cases) ==="
$GXX -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -DFFT_MUL_THRESHOLD=256 -DFFT_SQR_THRESHOLD=256 /home/azzr/mul.cpp -o /home/azzr/mul_fuzz -pthread 2>&1 | tail -1
echo "[OK] fuzz binary compiled"

python3 << 'PYEOF'
import random
random.seed(12345)
cases = []
# Various sizes including edge cases
for _ in range(3000):
    na = random.randint(1, 200)
    nb = random.randint(1, 200)
    a = ''.join([str(random.randint(0,9)) for _ in range(na)])
    b = ''.join([str(random.randint(0,9)) for _ in range(nb)])
    if na >= 2: a = str(random.randint(1,9)) + a[1:]
    if nb >= 2: b = str(random.randint(1,9)) + b[1:]
    if random.randint(0,1): a = '-' + a
    if random.randint(0,1): b = '-' + b
    cases.append((a, b))
# Medium sizes (will go basicMul with thr=256)
for _ in range(3000):
    na = random.randint(1, 50)
    nb = random.randint(1, 50)
    a = ''.join([str(random.randint(0,9)) for _ in range(na)])
    b = ''.join([str(random.randint(0,9)) for _ in range(nb)])
    if na >= 2: a = str(random.randint(1,9)) + a[1:]
    if nb >= 2: b = str(random.randint(1,9)) + b[1:]
    cases.append((a, b))
# Very small
for _ in range(4000):
    na = random.randint(1, 10)
    nb = random.randint(1, 10)
    a = ''.join([str(random.randint(0,9)) for _ in range(na)])
    b = ''.join([str(random.randint(0,9)) for _ in range(nb)])
    if na >= 2: a = str(random.randint(1,9)) + a[1:]
    if nb >= 2: b = str(random.randint(1,9)) + b[1:]
    cases.append((a, b))

with open('/tmp/big_fuzz.in', 'w') as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")
print(f"[OK] Generated {len(cases)} fuzz cases")
PYEOF

/home/azzr/mul_fuzz < /tmp/big_fuzz.in > /tmp/big_fuzz_ours.txt 2>&1
echo "[OK] fuzz binary ran"

python3 << 'PYEOF'
with open('/tmp/big_fuzz.in') as f:
    n = int(f.readline())
    results = []
    for line in f:
        parts = line.split()
        if len(parts) == 2:
            a, b = int(parts[0]), int(parts[1])
            results.append(str(a * b))
with open('/tmp/big_fuzz_expected.txt', 'w') as f:
    f.write('\n'.join(results) + '\n')
print(f"[OK] Computed {len(results)} expected results")
PYEOF

if diff -q /tmp/big_fuzz_ours.txt /tmp/big_fuzz_expected.txt > /dev/null 2>&1; then
    echo "[PASS] All 10000 fuzz cases correct!"
else
    echo "[FAIL] Fuzz mismatch!"
    diff /tmp/big_fuzz_ours.txt /tmp/big_fuzz_expected.txt | head -10
fi

echo ""
echo "=== Step 2: Full benchmark (all LC test points, thr=64) ==="
$GXX -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL /home/azzr/mul.cpp -o /home/azzr/mul_ours -pthread 2>&1 | tail -1
echo "[OK] mul_ours compiled (thr=64 default)"

printf "%-16s  %-10s  %-10s  %-8s  %s\n" "test" "ours(ms)" "best(ms)" "diff" "correct"
total_ours=0
total_best=0
for test in medium_0 medium_1 medium_2 small_00 large_0 large_1 large_2 max_max_0 max_max_1 max_max_2 max_max_3 max_max_4 max_max_5 max_max_6 max_max_7 large_small_00; do
    inp="/tmp/lc_test/${test}.in"
    if [ ! -f $inp ]; then
        echo "  $test: [SKIP] no input"
        continue
    fi
    # 2 warmup + 3 timed
    /home/azzr/mul_ours < $inp > /dev/null 2>&1
    /home/azzr/mul_ours < $inp > /dev/null 2>&1
    ot=""
    for _ in 1 2 3; do
        t0=$(python3 -c "import time;print(time.time())")
        /home/azzr/mul_ours < $inp > /tmp/ours_${test}.txt 2>/dev/null
        t1=$(python3 -c "import time;print(time.time())")
        dt=$(python3 -c "print(round(($t1-$t0)*1000,1))")
        ot="$ot $dt"
    done
    omin=$(echo $ot | tr ' ' '\n' | sort -n | head -1)

    /home/azzr/mul_best < $inp > /dev/null 2>&1
    /home/azzr/mul_best < $inp > /dev/null 2>&1
    bt=""
    for _ in 1 2 3; do
        t0=$(python3 -c "import time;print(time.time())")
        /home/azzr/mul_best < $inp > /tmp/best_${test}.txt 2>/dev/null
        t1=$(python3 -c "import time;print(time.time())")
        dt=$(python3 -c "print(round(($t1-$t0)*1000,1))")
        bt="$bt $dt"
    done
    bmin=$(echo $bt | tr ' ' '\n' | sort -n | head -1)

    if diff -q /tmp/ours_${test}.txt /tmp/best_${test}.txt > /dev/null 2>&1; then
        correct="YES"
    else
        correct="NO"
    fi
    diff_pct=$(python3 -c "print(f'{(($omin-$bmin)/$bmin*100):+.1f}%') if $bmin>0 else print('N/A')")
    printf "%-16s  %-10s  %-10s  %-8s  %s\n" "$test" "${omin}ms" "${bmin}ms" "$diff_pct" "$correct"
done

echo "=== [DONE] ==="
'''

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] Connected to VM")

stdin, stdout, stderr = c.exec_command(SCRIPT, timeout=600)
print(stdout.read().decode())
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:1500]}")
c.close()
print("[DONE]")
