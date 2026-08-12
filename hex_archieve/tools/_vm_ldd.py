# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
B="/home/azzr/hexbench/build"
vmctl.run(f"cd {B} && for f in v13 v16 v16nm; do echo \"--- $f ---\"; file $f | cut -c1-160; ldd $f 2>&1 | head -6; done")
