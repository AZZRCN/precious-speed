import json
d = json.load(open("/home/azzr/mulbench/bench_result.json"))
print("FLAGS:", d["flags"])
print("ROUNDS/WARMUP:", d["rounds"], d["warmup"])
print("CORRECTNESS:")
for k, v in d["correctness"].items():
    print(f"  {k}: oracle={v['oracle_ok']} consensus={v['consensus_ok']} bad={v['bad']} {v['bad_cases']}")
print("r4_eq_gold:", d["r4_eq_gold"])
print("WORST_ms:", d["worst_ms"])
lbls = list(d["worst_ms"].keys())
print("PER-CASE MIN ms (LC-worst = max of these rows):")
print("  %-20s" % "" + "".join("%12s" % l[:10] for l in lbls))
for c in d["per_case"]:
    print("  %-20s" % c + "".join("%12.3f" % d["per_case"][c][l]["min_ms"] for l in lbls))
