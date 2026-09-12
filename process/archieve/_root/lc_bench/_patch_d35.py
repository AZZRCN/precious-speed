# -*- coding: utf-8 -*-
"""D35 = D34 + Newton 路径商修正循环 absCompare -> 借位 (D31 技术, 纯 O2/LC 安全).
覆盖 absDivNewtonWithInv (4738/4745) 与 absDivNewtonWithInvFast (4775/4784) 的两条修正循环.
跳过: 4902 (absDivRem 顶层, 高位进位传播不直接适用, 且走中尺寸路径非头条热点);
      4820/4830 (#if 0 死代码).

借位技术 (与 D31 absDivBasicCore 完全一致, 已 554 MATCH 证明等价):
  首修正: bool bf = absSub(dividend, prod_span, dividend); while (bf) { qhat--; bf = !absAdd(dividend, divisor, dividend); }
  终归一化: while (true) { bool borrow = absSub(dividend, divisor, dividend); if (borrow) { add back; break; } qhat++; }
absCompare 是 r~0 时深扫的瓶颈 (D31 画像里 absDivBasicCore 的 absCompare 占 11%), Newton 路径同理.
"""
import io, re

SRC = r'D:\precious_speed\best\div_D34.cpp'
OUT = r'D:\precious_speed\best\div_D35.cpp'

t = io.open(SRC, encoding='utf-8').read()

t = t.replace(
    '// D34 = D33 + Newton/Mu Limb 临时缓冲去零化 (resize->reserve, 排除 t_inv)',
    '// D35 = D34 + Newton 商修正循环 absCompare->借位 (D31 技术, 算法层免全扫描)',
    1)

# NewtonWithInv (4738/4745): 首循环后紧跟 absSub(dividend, prod_span, dividend); dividend.size = k;
pat_nw = re.compile(
    r'while \(absCompare\(prod_span, dividend\) > 0\)\s*\{[^}]*\}\s*'
    r'absSub\(dividend, prod_span, dividend\);\s*'
    r'dividend\.size = k;\s*'
    r'while \(absCompare\(dividend, divisor\) >= 0\)\s*\{[^}]*\}'
)
repl_nw = (
    '// D35: 借位即 qhat 估大 (D31 技术, 免 absCompare 全扫描)\n'
    '            bool bf = absSub(dividend, prod_span, dividend);\n'
    '            while (bf) {\n'
    '                absSub1(qhat_span, 1, qhat_span);\n'
    '                bf = !absAdd(dividend, divisor, dividend);\n'
    '            }\n'
    '            dividend.size = k;\n'
    '            while (true) {\n'
    '                bool borrow = absSub(dividend, divisor, dividend);\n'
    '                if (borrow) { absAdd(dividend, divisor, dividend); break; }\n'
    '                absAdd1(qhat_span, 1, qhat_span);\n'
    '            }'
)
t, n1 = pat_nw.subn(repl_nw, t)
assert n1 == 1, 'NewtonWithInv pattern matched x%d (want 1)' % n1

# NewtonWithInvFast (4775/4784): 首循环后有一行 min 截断 + 注释
pat_fast = re.compile(
    r'while \(absCompare\(prod_span, dividend\) > 0\)\s*\{[^}]*\}\s*'
    r'//[^\n]*\n\s*'
    r'prod_span\.size = std::min\(prod_span\.size, dividend\.size\);\s*'
    r'absSub\(dividend, prod_span, dividend\);\s*'
    r'dividend\.size = k;\s*'
    r'while \(absCompare\(dividend, divisor\) >= 0\)\s*\{[^}]*\}'
)
repl_fast = (
    '// D35: 借位即 qhat 估大 (D31 技术)\n'
    '            prod_span.size = std::min(prod_span.size, dividend.size);  // 保证 absSub 安全\n'
    '            bool bf = absSub(dividend, prod_span, dividend);\n'
    '            while (bf) {\n'
    '                absSub1(qhat_span, 1, qhat_span);\n'
    '                bf = !absAdd(dividend, divisor, dividend);\n'
    '            }\n'
    '            dividend.size = k;\n'
    '            while (true) {\n'
    '                bool borrow = absSub(dividend, divisor, dividend);\n'
    '                if (borrow) { absAdd(dividend, divisor, dividend); break; }\n'
    '                absAdd1(qhat_span, 1, qhat_span);\n'
    '            }'
)
t, n2 = pat_fast.subn(repl_fast, t)
assert n2 == 1, 'Fast pattern matched x%d (want 1)' % n2

# 安全闸门: 4902 (absDivRem 顶层) 与 4820/4830 (#if 0) 的 absCompare 必须原样保留
assert 'while (absCompare(prod_span, dividend) > 0 && corrections < 5)' in t, 'Loose block changed!'
assert 'while (absCompare(prod_span, dividend) > 0)\n' in t, '4902 block unexpectedly changed!'

io.open(OUT, 'w', encoding='utf-8').write(t)
print('WROTE', OUT, len(t), 'bytes; nw=%d fast=%d' % (n1, n2))
