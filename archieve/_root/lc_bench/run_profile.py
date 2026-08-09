#!/usr/bin/env python3
"""run_profile.py - Run profiled mul and capture stderr timing breakdown.
Usage: python run_profile.py <exe> <input_file>
"""
import subprocess, sys, os

exe = sys.argv[1]
inp = sys.argv[2]

with open(inp, "rb") as f:
    r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                      stderr=subprocess.PIPE, timeout=60)
print(f"Exit code: {r.returncode}")
print("Profile output (stderr):")
print(r.stderr.decode('utf-8', errors='replace'))
