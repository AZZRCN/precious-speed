"""跨用例相对占比：每个用例耗时 / 该候选自己的瓶颈用例耗时。

这是同一候选内部的相对量，不涉及候选间的绝对时间比较，
用来判断某个用例是否可能成为 LC 的制约点。
"""
import json
import statistics as st

d = json.load(open("/home/azzr/mulbench/coldratio.json"))
a = d["abs_ms_diagnostic_only"]
bn = d["bottleneck"]
cands = list(a.keys())

print(f"瓶颈用例 = {bn}；下表为「该用例耗时 / 本候选在瓶颈用例上的耗时」")
print("case".ljust(20) + "".join(c.replace("v3_", "").rjust(10) for c in cands))
print("-" * (20 + 10 * len(cands)))
cases = list(a[cands[0]].keys())
for c in cases:
    row = c.ljust(20)
    for k in cands:
        row += f"{st.median(a[k][c]) / st.median(a[k][bn]):10.3f}"
    print(row)
