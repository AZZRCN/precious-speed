# AZZRCN
# https://github.com/AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
vmctl.put(os.path.join(R, "tools", "twdump.cpp"), "/home/azzr/hexbench/scratch/twdump.cpp")
vmctl.run("cd ~/hexbench/scratch && g++ -O2 -march=x86-64-v3 -std=c++23 twdump.cpp -o twdump && ./twdump")
