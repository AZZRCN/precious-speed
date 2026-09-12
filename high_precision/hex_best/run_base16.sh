#!/bin/bash
# run_base16.sh -- 在 VM Ubuntu (g++-15) 上编译并验证 div_base16 (HEX base-2^16)
# 用法: bash run_base16.sh
set -e
CXX="${CXX:-g++-15}"
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"

# 路径按 VM 实际布局调整 (本机 Windows 源在 D:\precious_speed\...)
HERE="$(cd "$(dirname "$0")" && pwd)"
DIV_BASE16="$HERE/div_base16.cpp"
DIV_BEST="${DIV_BEST:-$HERE/div.cpp}"            # 当前 HEX best (base-2^64)
DIV_DEC="${DIV_DEC:-$HERE/../dec_best/origin/div.cpp}"  # DEC best (base-10000)

echo "[1/3] 编译..."
$CXX $FLAGS -o div_base16 "$DIV_BASE16"
$CXX $FLAGS -o div_best   "$DIV_BEST"
[ -f "$DIV_DEC" ] && $CXX $FLAGS -o div_dec "$DIV_DEC"

echo "[2/3] 正确性交叉校验 (div_base16 vs div_best) ..."
python3 "$HERE/test_base16.py" --a ./div_base16 --b ./div_best --n 300

echo "[3/3] perf 指令数对比 (含 HEX/DEC 收敛比) ..."
if [ -f ./div_dec ]; then
  python3 "$HERE/test_base16.py" --a ./div_base16 --b ./div_best --dec ./div_dec --n 300 --perf
else
  python3 "$HERE/test_base16.py" --a ./div_base16 --b ./div_best --n 300 --perf
fi
echo "done."
