import subprocess
import sys

cases = [
    (98,  r"d:\precious_speed\tester\failures\div_div_medium_98_0.in",  r"d:\precious_speed\tester\failures\div_div_medium_98_0.ref.out"),
    (386, r"d:\precious_speed\tester\failures\div_div_medium_386_1.in", r"d:\precious_speed\tester\failures\div_div_medium_386_1.ref.out"),
    (620, r"d:\precious_speed\tester\failures\div_div_medium_620_2.in", r"d:\precious_speed\tester\failures\div_div_medium_620_2.ref.out"),
]

exe = r"d:\precious_speed\tester\cur_div.exe"
passed = 0
failed = 0

for seed, in_file, ref_file in cases:
    new_file = in_file.replace('.in', '.new.out')
    
    with open(in_file, 'rb') as f:
        in_data = f.read()
    
    proc = subprocess.Popen(exe, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out_data, err_data = proc.communicate(in_data, timeout=30)
    
    # Print stderr (path selection debug info)
    if err_data:
        err_text = err_data.decode("utf-8", errors="replace")
        # Print only non-empty lines
        for line in err_text.splitlines():
            line = line.strip()
            if line:
                print(f"[STDERR seed {seed}] {line}")
    
    with open(new_file, 'wb') as f:
        f.write(out_data)
    
    with open(ref_file, 'rb') as f:
        ref_data = f.read()
    
    # Remove CR bytes for comparison
    ref_clean = bytes(b for b in ref_data if b != 13)
    new_clean = bytes(b for b in out_data if b != 13)
    
    if ref_clean == new_clean:
        print(f"[PASS] seed {seed}")
        passed += 1
    else:
        print(f"[FAIL] seed {seed} (ref={len(ref_clean)} new={len(new_clean)})")
        # Find first diff
        for i in range(min(len(ref_clean), len(new_clean))):
            if ref_clean[i] != new_clean[i]:
                print(f"  First diff at byte {i}")
                break
        failed += 1

print(f"\n=== Results: {passed} passed, {failed} failed ===")
sys.exit(0 if failed == 0 else 1)