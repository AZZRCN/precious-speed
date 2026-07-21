#!/bin/bash
cd /home/azzr
echo "=== Compile baseline (DISABLE_2NXN_CYCLIC) and default (cyclic + dir2+7) ==="
g++ -O2 -std=gnu++20 -static -DHINT_OP_DIV -DDISABLE_2NXN_CYCLIC -o moptm_baseline moptm_fusion.cpp 2>&1 | tail -5
echo "baseline compiled"
g++ -O2 -std=gnu++20 -static -DHINT_OP_DIV -o moptm_default moptm_fusion.cpp 2>&1 | tail -5
echo "default compiled"
echo ""

echo "=== Generate 100 fuzz cases ==="
python3 /tmp/gen_fuzz.py
echo ""

echo "=== Fuzz correctness (baseline vs default, 100 cases) ==="
md5_base=$(./moptm_baseline < /tmp/div_fuzz_100.in | md5sum | awk '{print $1}')
md5_def=$(./moptm_default < /tmp/div_fuzz_100.in | md5sum | awk '{print $1}')
echo "MD5[baseline]: $md5_base"
echo "MD5[default]:  $md5_def"
if [ "$md5_base" = "$md5_def" ]; then
    echo "FUZZ: 100 cases MATCH"
else
    echo "FUZZ: MISMATCH!!!"
fi
echo ""

echo "=== Standard tests MD5 ==="
md5_1m_base=$(./moptm_baseline < div_1M_500k.in | md5sum | awk '{print $1}')
md5_1m_def=$(./moptm_default < div_1M_500k.in | md5sum | awk '{print $1}')
echo "1M/500k baseline: $md5_1m_base"
echo "1M/500k default:  $md5_1m_def"
md5_200k_base=$(./moptm_baseline < div_200k_100k.in | md5sum | awk '{print $1}')
md5_200k_def=$(./moptm_default < div_200k_100k.in | md5sum | awk '{print $1}')
echo "200k/100k baseline: $md5_200k_base"
echo "200k/100k default:  $md5_200k_def"
echo ""

echo "=== Benchmark (15 runs x 50 loops, ms/loop) ==="
LOOPS=50
RUNS=15
for IN in div_1M_500k.in div_200k_100k.in; do
    echo "--- $IN ---"
    printf '%-20s' 'version'
    for run in $(seq 1 $RUNS); do printf 'run%-6d' $run; done
    printf '%-10s%-10s\n' 'median' 'min'
    for V in moptm_baseline moptm_default; do
        printf '%-20s' $V
        times=''
        for run in $(seq 1 $RUNS); do
            start=$(date +%s.%N)
            for i in $(seq 1 $LOOPS); do
                ./$V < $IN > /dev/null
            done
            end=$(date +%s.%N)
            elapsed=$(awk "BEGIN{print ($end - $start) * 1000 / $LOOPS}")
            printf '%.3f    ' $elapsed
            times="$times $elapsed"
        done
        median=$(echo $times | tr ' ' '\n' | sort -n | awk 'NR==8{print}')
        min=$(echo $times | tr ' ' '\n' | sort -n | head -1)
        printf '%-10s%-10s\n' $median $min
    done
    echo ''
done
