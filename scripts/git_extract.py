#!/usr/bin/env python3
"""从 git 历史中提取指定 commit 的文件"""
import subprocess
import sys

if len(sys.argv) < 4:
    print("Usage: python git_extract.py <commit> <git_path> <output_path>")
    sys.exit(1)

commit = sys.argv[1]
git_path = sys.argv[2]
output_path = sys.argv[3]

result = subprocess.run(
    ["git", "show", f"{commit}:{git_path}"],
    capture_output=True,
    cwd=r"d:\precious_speed"
)

if result.returncode != 0:
    print(f"Error: {result.stderr.decode('utf-8', errors='replace')}")
    sys.exit(1)

with open(output_path, "wb") as f:
    f.write(result.stdout)

print(f"Extracted {commit}:{git_path} -> {output_path} ({len(result.stdout)} bytes)")
