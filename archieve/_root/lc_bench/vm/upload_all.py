#!/usr/bin/env python3
"""Upload everything needed for the authoritative VM benchmark.

Local paths use Windows style (D:/... E:/...); remote VM paths are Linux (/home/...).
Layout on VM (/home/azzr/mulbench):
  gen/            <- gen_stage: params.h + src/*.cpp + src/random.h
  gen/example_00.in
  src/            <- 4 mul candidate sources (renamed)
  gen_oracle.py, vm_bench.py
"""
from vmctl import put, put_tree

VM = "/home/azzr/mulbench"
LOCAL = "D:/precious_speed/lc_bench"
GEN_REPO = "E:/library-checker-problems-master/big_integer/multiplication_of_big_integers"

# 1) GEN staging tree (params.h + src/*.cpp + src/random.h)
put_tree(f"{LOCAL}/vm/gen_stage", f"{VM}/gen")

# 2) fixed example case
put(f"{GEN_REPO}/gen/example_00.in", f"{VM}/gen/example_00.in")

# 3) 4 candidate sources (renamed to match vm_bench.BINS)
mapping = [
    ("D:/precious_speed/best/mul_385663.cpp", "mul_gold.cpp"),
    ("D:/precious_speed/best/mul.cpp",        "mul_best.cpp"),
    ("D:/precious_speed/best/mul_387374.cpp", "mul_387374.cpp"),
    ("D:/precious_speed/lc_bench/exe/mul_r4.cpp", "mul_r4.cpp"),
]
for local, name in mapping:
    put(local, f"{VM}/src/{name}")

# 4) scripts
put(f"{LOCAL}/vm/gen_oracle.py", f"{VM}/gen_oracle.py")
put(f"{LOCAL}/vm/vm_bench.py",   f"{VM}/vm_bench.py")
put(f"{LOCAL}/vm/gen_cases.sh",  f"{VM}/gen_cases.sh")

print("UPLOAD DONE")
