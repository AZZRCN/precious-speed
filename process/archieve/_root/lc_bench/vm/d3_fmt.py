import sys
sys.path.insert(0, r"D:\precious_speed\lc_bench\vm")
from vmctl import run

# 输入格式: 取小文件首字节
run(r"echo IN_HEAD; head -c 120 ~/lcp/big_integer/division_of_big_integers/in/medium_00.in; echo")
# 输出格式: 跑 3 个小用例
run(r"printf '100 7\n1000000000000000000000000000000 7\n123456789 987654321\n' > /tmp/fz_fmt.in && echo OUT_HEAD && ~/divbench/bin/div_orig < /tmp/fz_fmt.in")
