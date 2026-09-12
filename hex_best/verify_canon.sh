#!/bin/bash
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"

echo "=== build ==="
g++-15 $FLAGS -o ~/div_canon_pre_bin ~/div_canon_pre.cpp 2> /tmp/build_pre.log
if [ $? -ne 0 ]; then echo "BUILD_PRE_FAIL"; cat /tmp/build_pre.log; exit 1; fi
g++-15 $FLAGS -o ~/div_canon_post_bin ~/div_canon_post.cpp 2> /tmp/build_post.log
if [ $? -ne 0 ]; then echo "BUILD_POST_FAIL"; cat /tmp/build_post.log; exit 1; fi
echo "build OK"

echo "=== oracle byte-equality (pre vs post, 26 cases) ==="
fail=0
for f in /tmp/lccases/*.in; do
  ~/div_canon_pre_bin < "$f" > /tmp/o_pre 2>/dev/null
  ~/div_canon_post_bin < "$f" > /tmp/o_post 2>/dev/null
  if ! cmp -s /tmp/o_pre /tmp/o_post; then echo "ORACLE_DIFF: $(basename "$f")"; fail=1; fi
done
if [ $fail -eq 0 ]; then echo "ORACLE_CLEAN"; else echo "ORACLE_FAIL"; fi

echo "=== perf instructions:u (pre vs post) ==="
python3 ~/lc_max.py ~/div_canon_pre_bin ~/div_canon_post_bin
