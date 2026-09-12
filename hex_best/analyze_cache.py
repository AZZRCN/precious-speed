#!/usr/bin/env python3
# HEX DIV 每点真实时长估算（cache 分层模型）
# 数据来源: Duck.ai 对话 (AMD EPYC 7B13 / Zen3), 频率 2.45 GHz
#   延迟(周期): L1=4, L2=12, L3=46, RAM=245
#   "每秒指令数" 由 频率/延迟 推出 (文件原文: L1=6.1亿, L2=2.04亿, L3=0.53亿, RAM=0.1亿)
# miss 计数: Intel VM (Tiger Lake) 实测 proxy; 应用 Zen3 延迟 => 绝对 ms 为估计值, 相对排序稳健.
import sys

FREQ = 2.45e9            # Hz
LAT = {'L1':4, 'L2':12, 'L3':46, 'RAM':245}
# 指令中心模型: cycles = (instr - L1)*1 + L1*4 + 非RAMmiss*L2orL3_extra + RAM*RAM_extra
#   L1命中额外 +3 (4-1); 非RAM miss 归因 L2(+8) 或 L3(+42); RAM 额外 +241 (245-4)
def cyc(instr, L1, l1m, ram):
    nonram = max(l1m - ram, 0)
    L1h = max(L1 - l1m, 0)
    low  = (instr - L1) + 4*L1 + 8*nonram + 241*ram     # 非RAM miss 全算 L2
    up   = (instr - L1) + 4*L1 + 42*nonram + 241*ram    # 非RAM miss 全算 L3
    return low, up

def ms(c): return c / FREQ * 1000.0

rows = []
with open(sys.argv[1]) as f:
    hdr = f.readline()
    for line in f:
        line=line.strip()
        if not line: continue
        c,instr,l1l,l1m,cm,cr = line.split('\t')
        instr=int(instr); l1l=int(l1l); l1m=int(l1m); cm=int(cm); cr=int(cr)
        low,up = cyc(instr,l1l,l1m,cm)
        t_low=ms(low); t_up=ms(up)
        t_flat = instr/10e6            # 朴素 10M ins/ms
        mr = l1m/l1l if l1l else 0
        rr = cm/l1l if l1l else 0
        rows.append((c,instr,l1l,l1m,cm,cr,t_low,t_up,t_flat,mr,rr))

# 按指令数降序 (= 大致规模/时长降序)
rows.sort(key=lambda r:-r[1])
print(f"{'case':<26}{'instr(M)':>10}{'L1miss%':>8}{'RAM%':>7}{'cache_lo(ms)':>13}{'cache_hi(ms)':>13}{'flat10M(ms)':>13}{'flat/cache':>11}")
print('-'*108)
for c,instr,l1l,l1m,cm,cr,t_low,t_up,t_flat,mr,rr in rows:
    print(f"{c:<26}{instr/1e6:>10.2f}{mr*100:>7.2f}%{rr*100:>6.2f}%{t_low:>13.2f}{t_up:>13.2f}{t_flat:>13.2f}{t_flat/((t_low+t_up)/2):>11.2f}")

# 汇总
tot_i=sum(r[1] for r in rows)
import statistics
avg_ratio = statistics.mean([r[8]/((r[6]+r[7])/2) for r in rows])
print('-'*108)
print(f"总指令数: {tot_i/1e6:.1f}M | 朴素总和: {tot_i/10e6:.1f}ms | 各点 flat/cache 均值倍率: {avg_ratio:.2f}x (即10M/1ms 平均低估该倍数)")
print(f"cache 模型区间总和: lo={sum(ms(cyc(r[1],r[2],r[3],r[4])[0]) for r in rows):.1f}ms  hi={sum(ms(cyc(r[1],r[2],r[3],r[4])[1]) for r in rows):.1f}ms")
