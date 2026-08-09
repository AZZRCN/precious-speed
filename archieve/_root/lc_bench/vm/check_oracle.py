import pickle, os, subprocess
os.system("pkill -9 -f gen_oracle.py 2>/dev/null")
p = "/home/azzr/mulbench/oracle_cache.pkl"
print("cache exists:", os.path.exists(p))
if os.path.exists(p):
    d = pickle.load(open(p, "rb"))
    print(len(d), "cases:", sorted(d.keys()))
r = subprocess.run(["pgrep", "-af", "gen_oracle.py"], capture_output=True, text=True)
print("remaining gen_oracle procs:", r.stdout.strip() or "NONE")
