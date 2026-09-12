#!/usr/bin/env python3
# 解析 /tmp/muinv_blk0.txt, 精确复算块0的 qhat, 与 TRUEQ 比对.
# 目的: 隔离 bug —— raw qhat 正确但整体错 => 下游 cyclic 积/余数修正坏; raw qhat 错 => 逆/乘法坏.
import sys, re

B = 65536
def parse(path):
    d = {}
    cur = None
    with open(path) as f:
        for line in f:
            line = line.rstrip()
            if not line: continue
            m = re.match(r'^([A-Z0-9_]+)=', line)
            if m:
                cur = m.group(1); d[cur] = []
                rest = line[m.end():]
                # 纯数组行: 仅含数字/逗号/空格
                if re.fullmatch(r'[0-9,\s]*', rest or ''):
                    d[cur] = [int(x) for x in rest.replace(' ','').rstrip(',').split(',') if x != '']
                else:
                    # 单行多标量: LEN2=200 THIS_IN=64 IN=64
                    for km in re.finditer(r'([A-Z0-9_]+)=(\d+)', line):
                        d[km.group(1)] = int(km.group(2))
            else:
                if cur and line[0].isdigit():
                    d[cur] += [int(x) for x in line.replace(' ','').rstrip(',').split(',') if x != '']
    return d

def to_big(limbs):
    v = 0
    for x in reversed(limbs):
        v = v * B + x
    return v

def main():
    p = sys.argv[1] if len(sys.argv) > 1 else "/tmp/muinv_blk0.txt"
    d = parse(p)
    for k in ['WINDOW','DIVISOR','DH','INV','DHIGH','TRUEQ']:
        if k not in d:
            print(f"missing {k}"); return
    LEN2 = d.get('LEN2', 0); THIS_IN = d.get('THIS_IN', 0); IN = d.get('IN', 0)
    W = to_big(d['WINDOW']); D = to_big(d['DIVISOR'])
    INV = to_big(d['INV']); DH = to_big(d['DH'])
    TRUEQ = to_big(d['TRUEQ'])
    # 独立验算 TRUEQ 是否真 = W//D
    q_indep = W // D
    print(f"LEN2={LEN2} THIS_IN={THIS_IN} IN={IN}")
    print(f"TRUEQ == W//D ? {q_indep == TRUEQ}  (TRUEQ limbs={TRUEQ.bit_length()//16})")
    # raw qhat = (DH*INV) >> (IN+1) limbs, 取低 THIS_IN limb 与 TRUEQ 比
    shift = (IN + 1) * 16
    qhat_full = DH * INV
    qhat_block = (qhat_full >> shift) & ((1 << (THIS_IN * 16)) - 1)
    print(f"raw qhat == TRUEQ ? {qhat_block == TRUEQ}")
    print(f"  raw qhat   = {format(qhat_block,'X')}")
    print(f"  TRUEQ      = {format(TRUEQ,'X')}")
    diff = qhat_block ^ TRUEQ
    if diff:
        low = (diff & (-diff)).bit_length() - 1
        print(f"  FIRST DIFF LIMB = {low//16} (bit {low})")

if __name__ == "__main__":
    main()
