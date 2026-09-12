"""Upload local files to /home/azzr/divbench/ (or a given remote dir).

Usage:  python _push.py file1 [file2 ...] [--dir /remote/dir]
"""
import os
import sys
import vmctl

args = sys.argv[1:]
rdir = "/home/azzr/divbench"
if "--dir" in args:
    i = args.index("--dir")
    rdir = args[i + 1]
    args = args[:i] + args[i + 2:]
for f in args:
    vmctl.put(f, rdir + "/" + os.path.basename(f))
