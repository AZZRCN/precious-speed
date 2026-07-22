#!/usr/bin/env python3
"""Scan FFT_MUL_THRESHOLD with new BASE=10^8 basicMul on medium/small tests."""
import paramiko, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

SCRIPT = r'''
GXX=/usr/bin/g++-11

echo "=== Compiling best/mul.cpp ==="
$GXX -O2 -std=gnu++20 -static -DONLINE_JUDGE /home/azzr/best_mul.cpp -o /home/azzr/mul_best -pthread 2>&1 | tail -1
echo "[OK] mul_best compiled"

# Test thresholds: 64(default), 96, 128, 160, 192, 256
for thr in 64 96 128 160 192 256; do
    echo ""
    echo "--- Compiling with FFT_MUL_THRESHOLD=$thr ---"
    $GXX -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -DFFT_MUL_THRESHOLD=$thr -DFFT_SQR_THRESHOLD=$thr /home/azzr/mul.cpp -o /home/azzr/mul_thr$thr -pthread 2>&1 | tail -1
    if [ $? -ne 0 ]; then
        echo "[FAIL] compile thr=$thr"
        continue
    fi
    echo "[OK] mul_thr$thr compiled"
done

echo ""
echo "=== Benchmark (3 runs, min) ==="
printf "%-8s  %-10s  %-10s  %-10s  %-10s  %-10s  %-10s  %s\n" "thr" "med_0" "med_1" "med_2" "small_00" "large_0" "max_max_0" "correct"
echo "best     -          -          -          -          -          -          -"

for test in medium_0; do
    inp="/tmp/lc_test/${test}.in"
    bt=""
    for _ in 1 2 3; do
        t0=$(python3 -c "import time;print(time.time())")
        /home/azzr/mul_best < $inp > /tmp/best_${test}.txt 2>/dev/null
        t1=$(python3 -c "import time;print(time.time())")
        dt=$(python3 -c "print(round(($t1-$t0)*1000,1))")
        bt="$bt $dt"
    done
    bmin=$(echo $bt | tr ' ' '\n' | sort -n | head -1)
    echo "best     $bmin"
done

for thr in 64 96 128 160 192 256; do
    bin="/home/azzr/mul_thr$thr"
    if [ ! -x $bin ]; then continue; fi
    line="thr=$thr  "
    for test in medium_0 medium_1 medium_2 small_00 large_0 max_max_0; do
        inp="/tmp/lc_test/${test}.in"
        if [ ! -f $inp ]; then
            line+="N/A        "
            continue
        fi
        # 2 warmup
        $bin < $inp > /dev/null 2>&1
        $bin < $inp > /dev/null 2>&1
        ot=""
        for _ in 1 2 3; do
            t0=$(python3 -c "import time;print(time.time())")
            $bin < $inp > /tmp/ours_thr_${test}.txt 2>/dev/null
            t1=$(python3 -c "import time;print(time.time())")
            dt=$(python3 -c "print(round(($t1-$t0)*1000,1))")
            ot="$ot $dt"
        done
        omin=$(echo $ot | tr ' ' '\n' | sort -n | head -1)
        line+=$(printf "%-10s " "${omin}ms")
        # Correctness check against best (only for first thr)
        if [ "$thr" = "64" ] && [ -f /tmp/best_${test}.txt ]; then
            if ! diff -q /tmp/ours_thr_${test}.txt /tmp/best_${test}.txt > /dev/null 2>&1; then
                line+="[FAIL]"
            fi
        fi
    done
    echo "$line"
done

echo ""
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
    print(f"[STDERR] {err[:1000]}")
c.close()
print("[DONE]")
