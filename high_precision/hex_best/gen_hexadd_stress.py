import random
random.seed(20260822)
HEX='0123456789abcdef'
def rnd_hex(nhex):
    s=''.join(random.choice(HEX) for _ in range(nhex))
    return random.choice('123456789abcdef')+s[1:]
lines=[]
# 高频大数 batch: 强制 limb 路径状态累积
N=200
for i in range(N):
    # 在 ~112k/122k bit 附近抖动, 模拟 case34 区域
    a=random.randint(27000,31000); b=random.randint(27000,31000)
    lines.append((rnd_hex(a),rnd_hex(b)))
# 交错小大数, 让快路径/limb路径反复切换 (状态污染高发区)
for i in range(100):
    if i%2==0:
        lines.append((rnd_hex(random.randint(1,30)),rnd_hex(random.randint(1,30))))
    else:
        lines.append((rnd_hex(random.randint(27000,31000)),rnd_hex(random.randint(27000,31000))))
with open('D:/precious_speed/hex_best/hexadd_stress2.in','w') as f:
    f.write(f"{len(lines)}\n")
    for ha,hb in lines:
        f.write(f"{ha}\n{hb}\n")
print(f"wrote {len(lines)} queries (stress2)")
