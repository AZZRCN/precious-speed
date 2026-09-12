# AZZRCN
# https://github.com/AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
vmctl.run("which perf valgrind; echo '--- perf paranoid ---'; cat /proc/sys/kernel/perf_event_paranoid")
vmctl.run("ls /home/azzr/hexbench/ ; echo '--- build ---'; ls /home/azzr/hexbench/build/")
vmctl.run("ls /home/azzr/hexbench/tools/ 2>/dev/null | head -20; echo '--- data ---'; ls /home/azzr/hexbench/data/mul/ | head -30")
