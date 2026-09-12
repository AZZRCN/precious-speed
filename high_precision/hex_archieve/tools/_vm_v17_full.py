# AZZRCN
# https://github.com/AZZRCN
# v17 全量官方 22 点擂台 (统一负重): v13w(A) vs v17(B) vs wbmul(假想敌)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
B = f"{H}/build"
vmctl.run(f"cd {B} && for f in v13w v16 v17 wbmul; do printf '%-6s ' $f; "
          f"objdump -p $f 2>/dev/null | grep NEEDED | awk '{{printf \"%s \", $2}}'; echo; done")
vmctl.run(f"cd {H} && python3 perfab.py data/mul {B}/v13w {B}/v17 {B}/wbmul -r 10", timeout=7200)
