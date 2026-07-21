#!/bin/bash
# 调查数据异常: fusion_o2only_O2 vs moptm_fusion_O2, ADD_1M, 10 次运行
cd /tmp/bench

echo "=== 数据异常调查 ==="
echo "时间: $(date)"
echo "VM 负载: $(uptime)"
echo ""

echo "=== 清理 page cache ==="
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null 2>&1 || echo "无法清理 cache (需要 sudo)"
echo ""

echo "=== 10 次 fusion_o2only_O2 ADD_1M ==="
for i in $(seq 1 10); do
    t=$(python3 -c "
import subprocess, time
t0=time.perf_counter()
subprocess.run(['taskset','-c','0','./fusion_o2only_O2_ADD'], stdin=open('add_1M.txt','rb'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f'{(time.perf_counter()-t0)*1000:.3f}')
")
    echo "  run $i: $t ms"
done

echo ""
echo "=== 10 次 moptm_fusion_O2 ADD_1M ==="
for i in $(seq 1 10); do
    t=$(python3 -c "
import subprocess, time
t0=time.perf_counter()
subprocess.run(['taskset','-c','0','./moptm_fusion_O2_ADD'], stdin=open('add_1M.txt','rb'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f'{(time.perf_counter()-t0)*1000:.3f}')
")
    echo "  run $i: $t ms"
done

echo ""
echo "=== 用 /usr/bin/time -v 详细信息 (fusion_o2only_O2) ==="
taskset -c 0 /usr/bin/time -v ./fusion_o2only_O2_ADD < add_1M.txt > /dev/null 2>&1 || true
taskset -c 0 /usr/bin/time -v ./fusion_o2only_O2_ADD < add_1M.txt > /dev/null 2>time_output.txt
cat time_output.txt

echo ""
echo "=== 检查二进制大小 ==="
ls -la fusion_o2only_O2_ADD moptm_fusion_O2_ADD

echo ""
echo "=== 检查 CPU 频率 ==="
cat /proc/cpuinfo | grep MHz | head -2
echo " governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'N/A')"
