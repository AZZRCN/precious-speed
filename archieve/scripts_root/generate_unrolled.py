#!/usr/bin/env python3
"""
Fast O3→O2 minimal unroll strategy
Copy + inject pragmas at identified hot loops
"""
import shutil
from pathlib import Path

def create_unrolled_version(src_file, out_file):
    """
    Minimal strategy: copy + inject unroll pragmas at hot FFT loops
    Key patterns from analysis:
    - Lines 1029-1036: inner butterfly loop (critical)
    - Lines 1047-1058: top butterfly loop
    - Lines 660-669: omega generation loop
    - Lines 1096-1104: transpose loop
    - Lines 1114-1139: twiddle loop
    """
    with open(src_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # Inject pragmas: find patterns and add #pragma GCC unroll(8)
    # Strategy: look for "for (" at specific line ranges and prepend pragma
    
    critical_zones = [
        (1028, 1030, "butterfly inner loop"),      # line ~1029
        (1046, 1048, "butterfly top loop"),        # line ~1047
        (659, 661, "omega generation"),            # line ~660
        (1095, 1097, "transpose loop i"),          # line ~1096
        (1098, 1100, "transpose loop j"),          # line ~1099
        (1113, 1115, "twiddle loop outer"),        # line ~1114
        (1121, 1123, "twiddle loop inner"),        # line ~1122
    ]
    
    # Mark lines to inject pragmas (before the 'for' statement)
    inject_set = set()
    for start, end, desc in critical_zones:
        for i in range(start - 1, min(end, len(lines))):  # -1 for 0-indexing
            line = lines[i].strip()
            if line.startswith("for") and "(" in line:
                inject_set.add(i)
                print(f"[+] Marked line {i+1} for pragma: {desc}")
                break
    
    # Rebuild with pragmas
    result = []
    for i, line in enumerate(lines):
        if i in inject_set:
            # Add pragma before the for loop
            indent = len(line) - len(line.lstrip())
            result.append(' ' * indent + '#pragma GCC unroll(8)\n')
        result.append(line)
    
    with open(out_file, 'w', encoding='utf-8') as f:
        f.writelines(result)
    
    print(f"[+] Created {out_file} with {len(inject_set)} pragmas injected")

def main():
    root = Path("D:/precious_speed")
    
    for name in ["mul.cpp", "add.cpp", "div.cpp"]:
        src = root / name
        out = root / f"PRE_{name}"
        
        if src.exists():
            create_unrolled_version(src, out)
            print(f"[+] Processed: {name} → PRE_{name}")
        else:
            print(f"[-] Not found: {src}")

if __name__ == "__main__":
    main()
