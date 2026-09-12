#!/bin/bash
# Verification of EDITED 393027_opt.cpp: invertappr call-1 high-product.
# Uses opt2/ref2 names so it never collides with a concurrently-running baseline check.
cd /tmp
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
echo "=== BUILD opt2 (edited) / ref2 (baseline) ==="
g++-15 $FLAGS -o /tmp/opt2 /tmp/opt2_src.cpp 2>&1 | tail -8
echo "OPT2_EXIT=$?"
g++-15 $FLAGS -o /tmp/ref2 /tmp/ref_src.cpp 2>&1 | tail -8
echo "REF2_EXIT=$?"
echo "=== ORACLE: 26 LC (opt2 vs ref2 byte-diff) ==="
python3 - <<'PY'
import glob, subprocess
tests = ['small','medium','large','max','a_max_b_random','power','r_nearly_zero','length_ratio_integer','burnikel_ziegler_bound']
cases=[]
for t in tests:
    for f in sorted(glob.glob('/tmp/lccases/%s_*.in'%t)):
        cases.append(f)
fail=0
for f in cases:
    o=subprocess.run(['/tmp/opt2'],stdin=open(f),capture_output=True)
    r=subprocess.run(['/tmp/ref2'],stdin=open(f),capture_output=True)
    if o.stdout!=r.stdout:
        print('DIFF',f); fail+=1
print('LC_diff_fail=%d of %d'%(fail,len(cases)))
PY
echo "=== ORACLE: adversarial 46 (opt2 vs ref2) ==="
/tmp/opt2 < ~/adv.in > /tmp/opt2_adv.out 2>/dev/null
/tmp/ref2 < ~/adv.in > /tmp/ref2_adv.out 2>/dev/null
if diff -q /tmp/opt2_adv.out /tmp/ref2_adv.out >/dev/null; then echo "ADV_opt2_vs_ref2: SAME"; else echo "ADV_opt2_vs_ref2: DIFF"; fi
if diff -q /tmp/ref2_adv.out ~/adv_base.out >/dev/null; then echo "ADV_ref2_vs_base: SAME"; else echo "ADV_ref2_vs_base: DIFF"; fi
echo "=== LC per-case instructions:u (opt2 vs ref2) ==="
python3 ~/lc_max.py /tmp/opt2 /tmp/ref2 2>&1
echo "=== DONE ==="
