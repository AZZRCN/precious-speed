# AZZRCN
# https://github.com/AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
HERE = os.path.dirname(os.path.abspath(__file__))
vmctl.put(os.path.join(HERE, "perfab.py"), "/home/azzr/hexbench/perfab.py")
vmctl.run("cd /home/azzr/hexbench && taskset -c 5 python3 perfab.py data/mul build/v13 build/v16 build/v16nm -r 3", timeout=1800)
