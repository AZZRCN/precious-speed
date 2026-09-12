import random, os, sys

random.seed(20260816)
HEX = '0123456789abcdef'

def rnd_hex(limbs):
    s = ''.join(random.choice(HEX) for _ in range(limbs * 4))
    return random.choice('123456789abcdef') + s[1:]

# (na_limbs, nb_limbs) covering knuthD / newton / bz + large FFT (path 2/3/5)
sizes = [
    (250, 200),        # knuthD
    (1500, 1400),      # knuthD
    (6000, 5800),      # knuthD
    (400, 200),        # newton (na<=2nb, na-nb>=128)
    (8000, 5000),      # newton
    (1000, 200),       # bz (na>2nb)
    (40000, 15000),    # bz
    (50000, 50000),    # newton boundary (na=2nb)
    (30000, 29000),    # knuthD-ish small quotient
    (70000, 68000),    # newton large
    (90000, 88000),    # newton large
    (60000, 40000),    # newton large
    (90000, 30000),    # bz large
    # 追加: 中等~大尺寸铺开, 确保 path 2/3/5 都被命中
    (12000, 9000),
    (20000, 19000),
    (35000, 33000),
    (45000, 44000),
    (55000, 53000),
    (65000, 63000),
    (75000, 73000),
    (85000, 83000),
    (100000, 98000),
    (110000, 50000),
    (120000, 119000),
    (130000, 128000),
    (140000, 60000),
    (150000, 148000),
]

outdir = sys.argv[1] if len(sys.argv) > 1 else 'cases'
os.makedirs(outdir, exist_ok=True)
# 同时写一个全量多 query 主输入
with open(os.path.join(outdir, 'all.in'), 'w') as fall:
    fall.write(f"{len(sizes)}\n")
    for i, (a, b) in enumerate(sizes):
        ha, hb = rnd_hex(a), rnd_hex(b)
        # 单 query 文件
        with open(os.path.join(outdir, f'case_{i:03d}.in'), 'w') as f:
            f.write(f"1\n{ha}\n{hb}\n")
        fall.write(f"{ha}\n{hb}\n")
print(f"wrote {len(sizes)} cases to {outdir}/ (+ all.in)")
