# AZZRCN
# https://github.com/AZZRCN
# 定向测量混合档 case (large_*) 的 IR / cache-miss / cycles 三口径
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
vmctl.run(f"rm -rf {H}/data/_lg && mkdir -p {H}/data/_lg && cd {H}/data/mul && "
          f"for f in large_00 large_01 large_02 fft_killer_00 max_max_01; do "
          f"cp $f.in $f.exp {H}/data/_lg/ 2>/dev/null; done; ls {H}/data/_lg/")
vmctl.run(f"cd {H} && taskset -c 5 python3 perfab.py data/_lg build/v13w build/v16 -r 5", timeout=1200)
