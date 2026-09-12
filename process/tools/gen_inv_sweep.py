import random, sys
sys.set_int_max_str_digits(600000)
random.seed(20260813)
BETA = 1 << 64
tests = []
ns = [50, 100, 200, 300, 400, 600, 800, 1000, 1200, 1400, 1600, 1800,
      2000, 2200, 2400, 2525, 2600, 2800, 3000, 3200, 3514, 4000]
for n in ns:
    for rep in range(3):
        # normalized n-limb d: top limb has MSB set
        top = random.randint(1 << 63, (1 << 64) - 1)
        limbs = [random.randint(0, (1 << 64) - 1) for _ in range(n - 1)]
        limbs.append(top)
        d = 0
        for v in reversed(limbs):  # limbs[0] is least significant
            d = (d << 64) | v
        # hex string, most significant limb first
        hexs = format(d, 'x')
        tests.append((n, hexs))
with open(r'D:\precious_speed\tools\inv_sweep_in.txt', 'w') as f:
    f.write(str(len(tests)) + '\n')
    for n, h in tests:
        f.write(h + '\n')
print("wrote", len(tests), "tests")
