import sys
out = open(sys.argv[1],'rb').read()
ref = open(sys.argv[2],'rb').read()
for i in range(min(len(out), len(ref))):
    if out[i] != ref[i]:
        ctx_o = out[max(0,i-30):i+30].decode('ascii','replace')
        ctx_r = ref[max(0,i-30):i+30].decode('ascii','replace')
        print(f'第一个差异在位置 {i}')
        print(f'out: ...{ctx_o}...')
        print(f'ref: ...{ctx_r}...')
        break
else:
    if len(out) != len(ref):
        print(f'长度不同: out={len(out)} ref={len(ref)}')
    else:
        print('完全相同')
out_lines = out.count(b'\n')
ref_lines = ref.count(b'\n')
print(f'out 行数: {out_lines}, ref 行数: {ref_lines}')
print(f'out 长度: {len(out)}, ref 长度: {len(ref)}')
