#!/bin/bash
# 对比 absDivMu vs absDivNewtonCore2 (baseline) 的 profile
# baseline 通过把 "if (mu_in < len2)" 替换为 "if (false && mu_in < len2)" 强制走 Core2
cd /tmp/bench || exit 1

set -e

echo "=== [1/4] 上传/同步源码 (假设已上传 moptm_fusion.cpp) ==="
ls -la moptm_fusion.cpp

echo ""
echo "=== [2/4] 生成 baseline 源码 (强制走 absDivNewtonCore2) ==="
cp moptm_fusion.cpp moptm_baseline.cpp
python3 - <<'PYEOF'
with open('moptm_baseline.cpp') as f: s=f.read()
s2 = s.replace('if (mu_in < len2)', 'if (false && mu_in < len2)')
assert s2 != s, 'replace failed: 没有找到 if (mu_in < len2)'
with open('moptm_baseline.cpp','w') as f: f.write(s2)
print('patched OK: mu_in 路径已禁用，强制走 absDivNewtonCore2')
PYEOF

echo ""
echo "=== [3/4] 编译 baseline (Core2, -DPROFILE_DIV) ==="
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DPROFILE_DIV -o moptm_baseline_prof moptm_baseline.cpp 2>&1 | tail -3
echo "compiled: moptm_baseline_prof"

echo ""
echo "=== [4/4] 编译 absDivMu 版本 (-DPROFILE_DIV) ==="
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DPROFILE_DIV -o moptm_mu_prof moptm_fusion.cpp 2>&1 | tail -3
echo "compiled: moptm_mu_prof"

echo ""
echo "================ BASELINE (absDivNewtonCore2) 1M/500k ================"
./moptm_baseline_prof < div_1M_500k.txt > /dev/null 2> prof_base.txt
cat prof_base.txt

echo ""
echo "================ ABS_DIV_MU 1M/500k ================"
./moptm_mu_prof < div_1M_500k.txt > /dev/null 2> prof_mu.txt
cat prof_mu.txt

echo ""
echo "================ BASELINE 200k/100k ================"
./moptm_baseline_prof < div_200k_100k.txt > /dev/null 2> prof_base_200k.txt
cat prof_base_200k.txt

echo ""
echo "================ ABS_DIV_MU 200k/100k ================"
./moptm_mu_prof < div_200k_100k.txt > /dev/null 2> prof_mu_200k.txt
cat prof_mu_200k.txt

echo ""
echo "================ BASELINE 1M/100k ================"
./moptm_baseline_prof < div_1M_100k.txt > /dev/null 2> prof_base_1M100k.txt
cat prof_base_1M100k.txt

echo ""
echo "================ ABS_DIV_MU 1M/100k ================"
./moptm_mu_prof < div_1M_100k.txt > /dev/null 2> prof_mu_1M100k.txt
cat prof_mu_1M100k.txt
