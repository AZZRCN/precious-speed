import os, sys, subprocess, time, json, statistics

VM_CASES = "/home/azzr/mulbench/cases"
BIN_DIR = "/home/azzr/mulbench/bin"
CASES = [
    "example_00.in","small_00.in",
    "medium_00.in","medium_01.in","medium_02.in",
    "large_00.in","large_01.in","large_02.in",
    "max_max_00.in","max_max_01.in","max_max_02.in","max_max_03.in",
    "max_max_04.in","max_max_05.in","max_max_06.in","max_max_07.in",
    "zero_00.in","fft_killer_00.in","fft_killer_01.in","large_small_00.in",
]
CANDIDATES = ["gold_385663","r4_current","best_cpp","r4_387374"]

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 5

def time_case(binname, case, reps):
    b = os.path.join(BIN_DIR, binname)
    c = os.path.join(VM_CASES, case)
    samples = []
    for _ in range(reps):
        t0 = time.perf_counter()
        subprocess.run(["taskset","-c","0", b], stdin=open(c,"rb"),
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        t1 = time.perf_counter()
        samples.append((t1 - t0) * 1000.0)
    return samples

print(f"COLD-RUN benchmark (fresh process per rep, reps={REPS}, taskset -c 0)")
print(f"{'case':20}" + "".join(c[:9].rjust(11) for c in CANDIDATES))
results = {c: {} for c in CANDIDATES}
for case in CASES:
    row = f"{case:20}"
    for cand in CANDIDATES:
        s = time_case(cand, case, REPS)
        results[cand][case] = {"min":min(s),"mean":statistics.mean(s),"max":max(s)}
        row += f"{statistics.mean(s):11.2f}"
    print(row)

# LC-worst style = max over cases of per-case mean (and of per-case max)
print()
print("LC-worst (max over cases of per-case MEAN), ms:")
worst_mean = {}
for cand in CANDIDATES:
    wm = max(results[cand][c]["mean"] for c in CASES)
    worst_mean[cand] = wm
    print(f"  {cand:14} {wm:8.3f}")
print("LC-worst (max over cases of per-case MAX), ms:")
for cand in CANDIDATES:
    wmx = max(results[cand][c]["max"] for c in CASES)
    print(f"  {cand:14} {wmx:8.3f}")
print()
print("gold_385663 mean as baseline; ratio r4_current/gold:")
g = worst_mean["gold_385663"]
print(f"  r4_current worst_mean = {worst_mean['r4_current']:.3f}  ratio={worst_mean['r4_current']/g:.3f}x")

json.dump({"reps":REPS,"per_case":results,"worst_mean":worst_mean},
          open("/home/azzr/mulbench/cold_result.json","w"), indent=2)
print("\nwrote cold_result.json")
