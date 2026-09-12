import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
rc,o,_ = vmctl.run("cat /home/azzr/verify_official.py")
print(o)
