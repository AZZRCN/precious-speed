import random
random.seed(20260822)
HEX='0123456789abcdef'
def rnd_hex(nhex):
    s=''.join(random.choice(HEX) for _ in range(nhex))
    return random.choice('123456789abcdef')+s[1:]

lines=[]
# normal 多形态: 覆盖 <=32位快路径 与 >32位 limb路径 的混合, 含进位链
normal_sizes=[(1,1),(2,5),(8,8),(9,9),(16,16),(17,17),(20,20),
              (32,1),(1,32),(33,33),(40,40),(64,64),(100,100),
              (200,1),(1,200),(500,500),(1000,1000),(1,1),(8,3),(3,8)]
for a,b in normal_sizes:
    lines.append((rnd_hex(a),rnd_hex(b)))
# stress 大数: limb 路径, 批量累积状态 (之前 case34=112216bit/122120bit 出问题)
stress_sizes=[(28000,28000),(5000,5000),(8000,8000),(40000,40000),
              (30554,30554),  # ~122216 bit
              (28054,28054),  # ~112216 bit
              (60000,60000),(90000,90000),(120000,120000),
              (28054,30554),(30554,28054),(70000,70000),(150000,150000)]
for a,b in stress_sizes:
    lines.append((rnd_hex(a),rnd_hex(b)))

with open('D:/precious_speed/hex_best/hexadd_test.in','w') as f:
    f.write(f"{len(lines)}\n")
    for ha,hb in lines:
        f.write(f"{ha}\n{hb}\n")
print(f"wrote {len(lines)} queries")
