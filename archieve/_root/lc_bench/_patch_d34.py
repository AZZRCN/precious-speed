# -*- coding: utf-8 -*-
"""D34 = D33 + Newton/Mu 路径 Limb 临时缓冲去零化 (resize->reserve).
基于已验证的覆写安全性 (逐一核对):
  tprod/tinv2 (absInvNewton 4148/4149, fallback 4692/4693): absSqr/absAdd 全写 + fill_n
  txp_mod (4342): 显式 fill_n(...,0) 覆盖 -> resize 零初始化冗余
  txp_full (4475): 显式 fill_n(...,0) 覆盖
  tinv_p1 (4631): copy+absAdd1 写 [0,k+1), 索引 k+1 未读
  tchk (4632): fill_n + absMul 覆盖
  tqhat/tprod (NewtonWithInv 4735/4737, Fast 4776/4778): absMul/fftMulPre 全写
  t_prod (absDivRem 4906): absMul 全写
  tqhat/tprod (absDivMu 分块 5131/5132): fftMulPre/absMul 全写
不可转 (保留 resize): t_inv (inv_extra>0 时 absSub 只写 [0,k+1), 使用区间含更高位靠零初始化)
  + data.resize (数值本体) + table.resize (twiddle 表).
"""
import io, re

SRC = r'D:\precious_speed\best\div_D33.cpp'
OUT = r'D:\precious_speed\best\div_D34.cpp'

t = io.open(SRC, encoding='utf-8').read()

# 版本注释
t = t.replace(
    '// D33 = D31 + FFT buffer de-zeroing (resize->reserve) + NDEBUG (源码内 assert 消除)',
    '// D34 = D33 + Newton/Mu Limb 临时缓冲去零化 (resize->reserve, 排除 t_inv)',
    1)

# 统一替换: X.resize(N) -> if (X.capacity() < N) X.reserve(N)
# 排除 t_inv.resize(...)
repl = [
    'tprod.resize(prod_size)',       # 4148, 4693 (单)
    'tinv2.resize(inv2_size)',       # 4149, 4693 (单)  -- 注 4693 行是两行 tprod/tinv2
    'txp_mod.resize(mn + 2)',        # 4343
    'txp_full.resize(2 * k + 2)',    # 4476
    'tinv_p1.resize(k + 2)',         # 4631
    'tchk.resize(2 * k + 3)',        # 4632
    'tqhat.resize(qhat_len)',        # 4736, 4777 (多行)
    'tprod.resize(prod_len)',        # 4738, 4779 (多行)
    't_prod.resize(prod_size)',      # 4907
    'tqhat.resize(qhat_len_max)',    # 5132
    'tprod.resize(prod_len_max)',    # 5133
]
for old in repl:
    cnt = t.count(old)
    assert cnt >= 1, 'not found: ' + old
    t = t.replace(old, 'if (%s.capacity() < %s) %s.reserve(%s)' % (
        old.split('.')[0], old[old.index('(')+1:old.rindex(')')],
        old.split('.')[0], old[old.index('(')+1:old.rindex(')')]))
    print('replaced %s x%d' % (old, cnt))

# 收尾: 塌缩 "if (X.size()<N) <ws> if (X.capacity()<N) X.reserve(N);" -> 单层
pat = re.compile(
    r'if \(([A-Za-z_]\w*)\.size\(\) < ([^)]+)\)\s+if \(\1\.capacity\(\) < \2\) \1\.reserve\(\2\);')
t, n = pat.subn(r'if (\1.capacity() < \2) \1.reserve(\2);', t)
print('collapsed double-guards x%d' % n)

# 安全闸门: t_inv.resize 必须原样保留
assert 't_inv.resize(inv_size)' in t, 't_inv was wrongly touched!'
# 不应残留双层守卫
leftover = re.compile(r'\.size\(\) < [^)]+\)\s+if \(')
assert not leftover.search(t), 'leftover double-guard'

io.open(OUT, 'w', encoding='utf-8').write(t)
print('WROTE', OUT, len(t), 'bytes')
