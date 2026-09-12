#!/usr/bin/env python3
# vm_profile.py - AMD EPYC 7B13 (Zen3 Milan) semantic perf profiler.
#
# Runs on the LOCAL INTEL VM as a PROXY. The VM's PMU does NOT expose LLC/L3
# events (LLC-load-misses = <not supported> under VMware), so this script
# collects what IS available and maps it onto the Zen3 PMC vocabulary for
# human interpretation. Final LEAF/threshold tuning MUST be confirmed by the
# real LC (AMD EPYC 7B13) submission receipt. Intel numbers = direction only.
#
# Events collected (all supported non-root on the VM, paranoid=-1):
#   cycles, instructions, cache-references, cache-misses,
#   L1-dcache-loads, L1-dcache-load-misses, branch-misses,
#   dTLB-load-misses, minor-faults, task-clock
#
# Zen3 semantic mapping (for interpretation, NOT measured on AMD here):
#   L1-dcache-load-misses  -> Zen3 L1D miss (L2 lookup)
#   cache-misses           -> approximated LLC/L3 miss proxy (Intel: last-level)
#   dTLB-load-misses       -> Zen3 DTLB miss (4K/2M walk)
#   minor-faults           -> page-faults from demand paging / hugepage setup
#   instructions/cycles    -> IPC proxy (uarch differs, trend is what matters)
#
# Usage:
#   python vm_profile.py <remote_bin> <remote_input> [repeats]
#   python vm_profile.py build        # self-test: compile & profile a tiny prog
import subprocess, sys, re, os

VM = os.path.join(os.path.dirname(__file__), "vm_ssh.py")
PY = "C:/Users/Administrator/.workbuddy/binaries/python/envs/default/Scripts/python.exe"

EVENTS = ("cycles,instructions,cache-references,cache-misses,"
          "L1-dcache-loads,L1-dcache-load-misses,branch-misses,"
          "dTLB-load-misses,minor-faults,task-clock")

def _run_ssh(cmd):
    r = subprocess.run([PY, VM, "run", cmd], capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr

def profile(remote_bin, remote_in, repeats=1):
    # Pin to a single core (taskset -c 0) to mimic LC's 1-core limit.
    inner = (f"cd /home/azzr/precious_speed && "
             f"taskset -c 0 perf stat -x, -e {EVENTS} ./{remote_bin} "
             f"< {remote_in} > /dev/null 2>/tmp/perf_err.txt; cat /tmp/perf_err.txt")
    rc, out, err = _run_ssh(inner)
    return parse_perf(out, repeats)

def parse_perf(perf_stderr, repeats=1):
    # perf stat -x, emits CSV lines: <value>,<unit>,<event>,...
    vals = {}
    for line in perf_stderr.splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) >= 3 and parts[2] in EVENTS:
            try:
                vals[parts[2]] = int(parts[0].replace(",", ""))
            except ValueError:
                vals[parts[2]] = parts[0]
    cyc = vals.get("cycles", 0) or 0
    ins = vals.get("instructions", 0) or 0
    cm  = vals.get("cache-misses", 0) or 0
    l1m = vals.get("L1-dcache-load-misses", 0) or 0
    l1l = vals.get("L1-dcache-loads", 0) or 0
    mf  = vals.get("minor-faults", 0) or 0
    out = dict(vals)
    out["IPC"] = round(ins / cyc, 3) if cyc else 0
    out["L1_miss_rate"] = round(l1m / l1l, 4) if l1l else 0
    out["cache_miss"] = cm
    out["repeats"] = repeats
    return out

def build_selftest():
    code = r'''
#include <cstdint>
#include <cstring>
#include <immintrin.h>
int main(){
  const int N=1<<20;
  volatile uint64_t s=0;
  for(int i=0;i<N;++i){ __m256i v=_mm256_set1_epi64x(i); s+=_mm256_extract_epi64(v,0); }
  return (int)s&1;
}'''
    _run_ssh("mkdir -p /home/azzr/precious_speed/_selftest")
    with open("/tmp/_sp.cpp", "w") as f: f.write(code)
    _run_ssh("")  # noop to keep conn warm
    # upload
    r = subprocess.run([PY, VM, "put", "/tmp/_sp.cpp",
                        "/home/azzr/precious_speed/_selftest/sp.cpp"], capture_output=True, text=True)
    rc, out, err = _run_ssh(
        "cd /home/azzr/precious_speed/_selftest && "
        "g++ -O2 -std=c++23 -mavx2 -o sp.bin sp.cpp 2>&1 && echo BUILD_OK")
    print("build:", out.strip(), err.strip())
    rc, out, err = _run_ssh(
        "cd /home/azzr/precious_speed/_selftest && "
        "echo '' | taskset -c 0 perf stat -x, -e " + EVENTS + " ./sp.bin > /dev/null 2>/tmp/perf_err.txt; cat /tmp/perf_err.txt")
    print("=== selftest perf (Intel proxy) ===")
    print(parse_perf(out))

if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "build":
        build_selftest()
    elif len(sys.argv) >= 3:
        rep = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        res = profile(sys.argv[1], sys.argv[2], rep)
        print(res)
    else:
        print("usage: vm_profile.py <remote_bin> <remote_input> [repeats] | build")
