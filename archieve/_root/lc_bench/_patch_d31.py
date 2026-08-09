#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D31 = D27 + Knuth Algorithm D 步骤 D3 (3-limb qhat 精修) + G-M 乘法倒数。

动机 (callgrind 实测, r_nearly_zero_01 @ d27):
  absDivBasicCore -> absMul1  267086 次
  absDivBasicCore -> absSub   361815 次   <-- 多出 94729 次 = 35.5% 的 qhat 修正率!
  正常 Knuth 规范化后修正率应为 ~2/BASE = 0.02%, 这里高了 1775 倍,
  根因是 qhat 只用 1 个 limb 估计 (high/divisor_high), 缺 v[n-2] 测试。

改法:
  1) 加 Knuth D3 的 v[n-2] 测试 -> qhat 恰为 q 或 q+1 (后者 ~2/BASE)
  2) 去掉每次迭代的全长 absCompare (r≈0 时要深扫), 改用 absSub 的借位免费判定
  3) 35.5% 的修正 absSub -> 0.02% 的加回 absAdd
  4) 用 Granlund-Montgomery 乘法倒数消掉内循环的硬件 32 位除法
"""
import io
import os
import re

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "best", "div_D27.cpp")
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "best", "div_D31.cpp")

with io.open(SRC, encoding="utf-8") as f:
    lines = f.read().split("\n")

# --- 定位 absDivBasicCore 函数体 -------------------------------------------
sig = None
for i, ln in enumerate(lines):
    if "static void absDivBasicCore(Span dividend, View divisor, Span quotient)" in ln:
        sig = i
        break
assert sig is not None, "absDivBasicCore signature not found"

start = None
for i in range(sig, sig + 40):
    if "size_t quot_idx = len1 - len2;" in lines[i]:
        start = i
        break
assert start is not None, "quot_idx anchor not found"

end = None
for i in range(start + 1, start + 120):
    if lines[i] == "        }":            # 函数收尾 (8 空格 + 花括号)
        end = i
        break
assert end is not None, "function close brace not found"

old = "\n".join(lines[start + 1:end])
assert "absCompare(View(prod_span), View(dividend_span))" in old, "unexpected body:\n" + old[:800]

NEW = r'''
            // ================= D31: Knuth Algorithm D 步骤 D3 =================
            // 实测 (callgrind, r_nearly_zero_01): 本函数发出 267086 次 absMul1 却发出
            // 361815 次 absSub —— 多出的 94729 次全是 qhat 估大后的回退修正,
            // 修正率 35.5%。规范化 (v_high >= BASE/2) 只保证 qhat <= q+2, 真正把
            // 误差压到 "q 或 q+1 (后者概率 ~2/BASE)" 的是 Knuth 的 v[n-2] 测试,
            // 而原实现缺了这一步。补上之后:
            //   1) 每次迭代的全长 absCompare 彻底消失 —— 该比较在 r≈0 的用例上
            //      要一路深扫才能定出大小, 是 absDivBasicCore 自身 11% Ir 的主因;
            //      改由 absSub 已经免费返回的借位来判定。
            //   2) 35.5% 概率的全长修正 absSub -> 0.02% 概率的加回 absAdd。
            //   3) 一个几乎不可预测的分支 (35.5% 命中) -> 恒不跳转的分支。
            //
            // 顺带: 用 Granlund-Montgomery 乘法倒数消掉内循环里的硬件 32 位除法。
            // divisor_high 在整个除法期间不变, 故倒数只算一次, 摊到 ~67 次迭代上。
            //   m = floor(2^42/d) + 1, 对所有 n < 2^27 满足 floor(m*n/2^42) == n/d;
            //   充分条件: m*d - 2^42 = d - (2^42 mod d) <= 2^(42-27) = 32768,
            //   而 d <= BASE-1 = 9999 < 32768 恒成立 ✓
            //   n = high = high1*BASE + high2 <= 99999999 < 2^27 = 134217728 ✓
            //   m <= 2^42/5000 < 2^30, m*n < 2^57 < 2^64 ✓ (无溢出)
            const uint64_t dh_magic = (uint64_t(1) << 42) / divisor_high + 1;
            const Limb divisor_high2 = (len2 >= 2) ? divisor[len2 - 2] : Limb(0);

            thread_local std::vector<Limb> tprod;
            if (tprod.size() < len2 + 1)
                tprod.resize(len2 + 1);
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1];
                Limb2 high = Limb2(high1) * BASE + high2;
                Limb2 qh;
                if (high1 >= divisor_high)
                {
                    qh = BASE - 1;
                }
                else
                {
                    qh = Limb2((dh_magic * uint64_t(high)) >> 42);
#ifdef DIV_D31_VERIFY
                    assert(qh == high / divisor_high);
#endif
                }
                Limb2 rhat = high - qh * Limb2(divisor_high);
                // Knuth D3: 只要 qhat*v[n-2] > rhat*BASE + u[j+n-2] 就说明 qhat 估大
                // (rhat >= BASE 时无需再测, 此时 qhat 必已正确)。至多循环 2 次。
                if (len2 >= 2)
                {
                    Limb2 u2 = dividend[len1 - 2];
                    while (rhat < Limb2(BASE) &&
                           qh * Limb2(divisor_high2) > rhat * Limb2(BASE) + u2)
                    {
                        qh--;
                        rhat += divisor_high;
                    }
                }
                Limb qhat = Limb(qh);
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                // 先减再看借位: 借位 <=> qhat 估大 (D3 之后概率 ~2/BASE)
                bool bf = absSub(dividend_span, prod_span, dividend_span);
                while (bf)
                {
                    qhat--;
                    // 加回除数产生的进位正好抵消之前的借位; 没进位说明还差, 继续
                    bf = !absAdd(dividend_span, divisor, dividend_span);
                }
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }'''

out = lines[:start + 1] + NEW.split("\n") + lines[end:]
txt = "\n".join(out)

# 更新文件头注释里的版本标记
txt = txt.replace("// Division of Big Integers",
                  "// Division of Big Integers\n// D31 = D27 + Knuth D3 qhat refinement + Granlund-Montgomery reciprocal",
                  1)

with io.open(DST, "w", encoding="utf-8", newline="\n") as f:
    f.write(txt)

print("wrote", os.path.abspath(DST), len(txt), "bytes")
print("replaced lines %d..%d (%d -> %d lines)" % (start + 2, end, end - start - 1, len(NEW.split("\n"))))
