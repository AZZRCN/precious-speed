#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BINS = sys.argv[1] if len(sys.argv) > 1 else 'd16,d17_8,d17_16,d17_32,d17_64,d17_128'
CASES = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03'
TAG = sys.argv[3] if len(sys.argv) > 3 else 'scan'

put(os.path.join(HERE, "_d17_cg_remote.py"), "/home/azzr/divbench/_d17_cg_remote.py")
rc, out, err = run("cd ~/divbench && python3 _d17_cg_remote.py '%s' '%s'" % (BINS, CASES),
                   timeout=10800)
print(out)
print(err[-2000:])
with open(os.path.join(HERE, "_d17_cg_%s.log" % TAG), "w", encoding="utf-8") as f:
    f.write(out)
