# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
vmctl.run("which perf; which valgrind; cat /proc/sys/kernel/perf_event_paranoid; nproc; ls /home/azzr/hexbench/data/mul/*.in | wc -l")
