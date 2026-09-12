import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"; B = f"{H}/build"
print("\n########## bigmix: v16(A) vs v19(B) ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/_big {B}/v16 {B}/v19 -r 10", timeout=3600)
print("\n########## 全量 22 点: v13w / v16 / v19 / wbmul ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/mul {B}/v13w {B}/v16 {B}/v19 {B}/wbmul -r 10", timeout=7200)
