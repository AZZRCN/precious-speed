"""Probe heterogeneous PMU availability on the new 285H VM (Arrow Lake).

Arrow Lake exposes three separate PMUs: cpu_core (P/Lion Cove),
cpu_atom (E/Skymont), cpu_lowpower (LPE).  A bare `-e cycles` cannot be
disambiguated, so perf reports "event is not supported".  Explicit
`cpu_core/cycles/` syntax is required.

Read-only probe: does not modify anything on the VM.
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("VM_HOST", "192.168.1.64")

from vmctl import run, VM  # noqa: E402

SCRIPT = "\n".join([
    "echo ===CORE_PMU===",
    "perf stat -e cpu_core/cycles/,cpu_core/instructions/ true 2>&1 | "
    "grep -E 'cycles|instructions|supported|Error' | head -4",
    "echo ===ATOM_PMU===",
    "perf stat -e cpu_atom/cycles/ true 2>&1 | "
    "grep -E 'cycles|supported|Error' | head -3",
    "echo ===TOPOLOGY===",
    "for p in cpu_core cpu_atom cpu_lowpower; do "
    "printf '%-14s ' $p; cat /sys/bus/event_source/devices/$p/cpus "
    "2>/dev/null || echo '(none)'; done",
    "echo ===MAXFREQ===",
    "for c in 0 1 2 3 4 5 6 7 8 9 10 11 12 13; do "
    "f=/sys/devices/system/cpu/cpu$c/cpufreq/cpuinfo_max_freq; "
    "[ -f $f ] && printf 'cpu%-3s %s\\n' $c $(cat $f); done | head -16",
    "echo ===REAL_WORKLOAD===",
    "perf stat -e cpu_core/cycles/,cpu_core/instructions/ "
    "-- awk 'BEGIN{s=0;for(i=0;i<3000000;i++)s+=i;print s}' 2>&1 | "
    "grep -E 'cycles|instructions|elapsed' | head -4",
])

rc, out, err = run(SCRIPT)
print("HOST =", VM["hostname"], "rc =", rc)
print(out)
if err:
    print("STDERR:", err[:400])
