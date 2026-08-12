# AZZRCN
# https://github.com/AZZRCN
# 打开 VM 的 perf 用户态权限 (paranoid=-1, kptr_restrict=0), 并持久化。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
S = "echo 1234 | sudo -S "
vmctl.run(S + "sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'")
vmctl.run(S + "sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict'")
vmctl.run(S + "sh -c 'echo \"kernel.perf_event_paranoid=-1\nkernel.kptr_restrict=0\" > /etc/sysctl.d/99-perf.conf'")
vmctl.run("cat /proc/sys/kernel/perf_event_paranoid; cat /proc/sys/kernel/kptr_restrict")
vmctl.run("cd /home/azzr/hexbench && perf stat -e instructions:u,cycles:u,L1-dcache-load-misses,LLC-load-misses build/v13 < data/mul/max_max_01.in > /dev/null")
