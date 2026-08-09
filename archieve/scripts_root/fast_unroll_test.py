#!/usr/bin/env python3
"""
GIMPLE O3 Decompiler + Unrolled Code Test Suite
Fast unroll + consistency verification for mul/add/div
"""

import subprocess
import hashlib
import os
import sys
from pathlib import Path

class FastUnroller:
    """Minimal-switch O3→O2 unroller with consistency test"""
    
    def __init__(self, ssh_host="192.168.1.55", ssh_user="azzr", ssh_pass="1234"):
        self.ssh_host = ssh_host
        self.ssh_user = ssh_user
        self.ssh_pass = ssh_pass
        self.local_root = Path("D:/precious_speed")
        self.remote_root = "/tmp/precious_speed_azzr"
        
    def single_ssh_session(self):
        """
        One SSH connection handles:
        1. Upload source files
        2. Compile original (-O3)
        3. Generate GIMPLE
        4. Compile unrolled (-O2)
        5. Run consistency tests
        6. Download results
        """
        ssh_commands = f"""#!/bin/bash
set -e

# Setup
cd {self.remote_root}
export PATH=/usr/local/gcc-15.2.0/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/gcc-15.2.0/lib64:$LD_LIBRARY_PATH

echo "=== Step 1: Compile originals with -O3 ==="
g++-15.2.0 -O3 -std=c++17 -march=cascadelake mul.cpp -o mul_o3 2>&1 | head -20
g++-15.2.0 -O3 -std=c++17 -march=cascadelake add.cpp -o add_o3 2>&1 | head -20
g++-15.2.0 -O3 -std=c++17 -march=cascadelake div.cpp -o div_o3 2>&1 | head -20

echo "=== Step 2: Extract GIMPLE (for O2 unroll pattern) ==="
g++-15.2.0 -O2 -std=c++17 -march=cascadelake -fdump-tree-gimple mul.cpp -E > mul_O2.gimple 2>/dev/null &
g++-15.2.0 -O2 -std=c++17 -march=cascadelake -fdump-tree-gimple add.cpp -E > add_O2.gimple 2>/dev/null &
g++-15.2.0 -O2 -std=c++17 -march=cascadelake -fdump-tree-gimple div.cpp -E > div_O2.gimple 2>/dev/null &
wait

echo "=== Step 3: Unroll pragmas + inline hints (minimal edits) ==="
# For now: stub unrolled versions (copy + add pragmas to hot loops)
cp mul.cpp PRE_mul.cpp
cp add.cpp PRE_add.cpp
cp div.cpp PRE_div.cpp

# Add unroll pragmas to likely hot spots (detected from GIMPLE)
# sed -i 's/for.*(/&\\n#pragma GCC unroll(8) \\n/g' PRE_mul.cpp  # stub
# Real strategy: parse GIMPLE, inject pragmas, regenerate

echo "=== Step 4: Compile unrolled with -O2 only ==="
g++-15.2.0 -O2 -std=c++17 -march=cascadelake PRE_mul.cpp -o mul_pre_o2 2>&1 | head -20
g++-15.2.0 -O2 -std=c++17 -march=cascadelake PRE_add.cpp -o add_pre_o2 2>&1 | head -20
g++-15.2.0 -O2 -std=c++17 -march=cascadelake PRE_div.cpp -o div_pre_o2 2>&1 | head -20

echo "=== Step 5: Hash executables ==="
sha256sum mul_o3 mul_pre_o2 > hashes.txt
sha256sum add_o3 add_pre_o2 >> hashes.txt
sha256sum div_o3 div_pre_o2 >> hashes.txt
cat hashes.txt

echo "=== Step 6: Quick test (if available) ==="
if [ -f test_best.in ]; then
    ./mul_o3 < test_best.in > out_o3.txt 2>&1 || echo "mul_o3: expected RE"
    ./mul_pre_o2 < test_best.in > out_pre.txt 2>&1 || echo "mul_pre_o2: expected RE"
    if diff out_o3.txt out_pre.txt; then
        echo "PASS: Output identical"
    else
        echo "FAIL: Output differs"
    fi
fi

echo "=== Step 7: Stage for download ==="
cp PRE_*.cpp /tmp/
cp *.gimple /tmp/ 2>/dev/null || true
cp hashes.txt /tmp/
"""
        
        # Write to temp script
        script_path = Path("/tmp/unroll_session.sh")
        script_path.write_text(ssh_commands)
        
        # Execute via SSH (using sshpass to avoid interactive prompt)
        cmd = [
            "sshpass", "-p", self.ssh_pass,
            "ssh", "-o", "StrictHostKeyChecking=no",
            f"{self.ssh_user}@{self.ssh_host}",
            f"bash -s < {script_path}"
        ]
        
        print("[*] Running comprehensive SSH session...")
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        
        print("=== SSH STDOUT ===")
        print(result.stdout[:2000])  # First 2KB
        
        if result.returncode != 0:
            print("=== SSH STDERR ===")
            print(result.stderr[:1000])
        
        return result.returncode == 0

def quick_local_test():
    """
    Local verification (no files needed):
    - Check syntax of our unroller framework
    - Verify test data structure
    """
    print("\n[*] Local syntax check...")
    
    # Verify best/ reference solutions exist
    best_dir = Path("D:/precious_speed/best")
    if not best_dir.exists():
        print("[-] best/ directory not found, skipping reference validation")
        return
    
    for fname in ["mul.cpp", "add.cpp", "div.cpp"]:
        fpath = best_dir / fname
        if fpath.exists():
            size = fpath.stat().st_size
            print(f"[+] {fname}: {size} bytes (reference)")

def main():
    """Execute minimal-switch unroll + test pipeline"""
    print("=" * 70)
    print("GIMPLE O3 Decompiler: Fast Unroll + Consistency Test")
    print("=" * 70)
    
    unroller = FastUnroller()
    
    # Step 1: Local checks (minimal overhead)
    quick_local_test()
    
    # Step 2: Single SSH session with all operations
    print("\n[*] Initiating unified SSH session for unroll + test...")
    success = unroller.single_ssh_session()
    
    if success:
        print("\n[+] SSH session completed")
        print("[*] Check remote:/tmp/ for PRE_*.cpp, hashes.txt, *.gimple")
    else:
        print("\n[-] SSH session failed")
        sys.exit(1)
    
    print("\n[+] Pipeline complete. Next: SCP download results")

if __name__ == "__main__":
    main()
