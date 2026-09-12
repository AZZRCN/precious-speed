#!/usr/bin/env python3
import subprocess, os
BASE = "/home/azzr/mulbench"
cases = sorted(f for f in os.listdir(BASE + "/cases") if f.endswith(".in"))
bins = {
    "gold_385663": BASE + "/bin/gold_385663",
    "best_cpp": BASE + "/bin/best_cpp",
    "r4_387374": BASE + "/bin/r4_387374",
    "r4_current": BASE + "/bin/r4_current",
}
gold = {}
for c in cases:
    p = subprocess.run(["taskset", "-c", "0", bins["gold_385663"]],
                       stdin=open(BASE + "/cases/" + c, "rb"),
                       stdout=subprocess.PIPE)
    gold[c] = p.stdout
print("=== correctness: byte-identical to gold_385663 (the trusted 36ms ref) ===")
for name, b in bins.items():
    if name == "gold_385663":
        continue
    ok = 0
    bad = []
    for c in cases:
        p = subprocess.run(["taskset", "-c", "0", b],
                           stdin=open(BASE + "/cases/" + c, "rb"),
                           stdout=subprocess.PIPE)
        if p.stdout == gold[c]:
            ok += 1
        else:
            bad.append(c)
    print(f"  {name:14}: {ok}/{len(cases)}  mismatch={bad}")
# also sanity: gold output size per case
print("=== gold output bytes per case (proxy for result digit count) ===")
for c in cases:
    print(f"  {c:20}: {len(gold[c])} bytes")
