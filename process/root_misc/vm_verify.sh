#!/bin/bash
# Verification gate for 393027_opt.cpp (cyclic wired, DBG removed)
cd /tmp
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
echo "=== BUILD ==="
g++-15 $FLAGS -o /tmp/opt /tmp/opt_src.cpp 2>&1 | tail -8
echo "OPT_EXIT=$?"
g++-15 $FLAGS -o /tmp/ref /tmp/ref_src.cpp 2>&1 | tail -8
echo "REF_EXIT=$?"

echo "=== ORACLE: canonical 26 LC cases (opt vs ref byte-diff) ==="
python3 - <<'PY'
import glob, subprocess
tests = ['small','medium','large','max','a_max_b_random','power','r_nearly_zero','length_ratio_integer','burnikel_ziegler_bound']
cases=[]
for t in tests:
    for f in sorted(glob.glob('/tmp/lccases/%s_*.in'%t)):
        cases.append(f)
fail=0
for f in cases:
    o=subprocess.run(['/tmp/opt'],stdin=open(f),capture_output=True)
    r=subprocess.run(['/tmp/ref'],stdin=open(f),capture_output=True)
    if o.stdout!=r.stdout:
        print('DIFF',f); fail+=1
print('LC_canonical_diff_fail=%d of %d'%(fail,len(cases)))
PY

echo "=== ORACLE: adversarial 46 cases (adv.in) ==="
/tmp/opt < ~/adv.in > /tmp/opt_adv.out 2>/dev/null
/tmp/ref < ~/adv.in > /tmp/ref_adv.out 2>/dev/null
if diff -q /tmp/opt_adv.out /tmp/ref_adv.out >/dev/null; then echo "ADV_opt_vs_ref: SAME"; else echo "ADV_opt_vs_ref: DIFF"; fi
if diff -q /tmp/ref_adv.out ~/adv_base.out >/dev/null; then echo "ADV_ref_vs_base: SAME"; else echo "ADV_ref_vs_base: DIFF"; fi

echo "=== LC per-case instructions:u (opt vs ref) ==="
python3 ~/lc_max.py /tmp/opt /tmp/ref 2>&1
echo "=== DONE ==="
