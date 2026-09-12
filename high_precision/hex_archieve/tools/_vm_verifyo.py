import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"
rc,o,_ = vmctl.run(f"cd {H} && python3 /home/azzr/verify_official.py div bin/div_v9 2>&1 | grep -E '===|OK=|WA |RE |TLE|SKIP|worst'")
print(o)
