#!/usr/bin/env python3
# Produce div candidate D1: fuse absDivBasicCore's per-digit
#   absMul1(temp) -> absCompare -> absSub
# into a single Knuth Algorithm-D multiply-subtract pass (no temp buffer,
# no separate compare/subtract). Portable instruction-count win; no FFT change.
import io, sys

SRC = r"D:\precious_speed\best\div.cpp"
OUT = r"D:\precious_speed\lc_bench\exe\div_D1_fused.cpp"

with open(SRC, "r", encoding="utf-8", newline="") as f:
    text = f.read()

START = "        static void absDivBasicCore(Span dividend, View divisor, Span quotient)"
END = "        // B-1 优化: m_dft/m_dft_float_len 为预计算的 m 的 DFT"
assert text.count(START) == 1, f"START anchor count = {text.count(START)}"
assert text.count(END) == 1, f"END anchor count = {text.count(END)}"

i = text.index(START)
j = text.index(END)
assert i < j

NEW = '''        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            // D1: 融合 multiply-subtract (Knuth Algorithm D) —— 去掉 absMul1 临时缓冲 +
            //     absCompare + 独立 absSub 三趟 O(k) 处理, 合并为单趟; 可移植指令数下降, FFT 不变.
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
            size_t quot_idx = len1 - len2;
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1];
                Limb qhat;
                if (high1 >= divisor_high)
                {
                    qhat = BASE - 1;
                }
                else
                {
                    qhat = Limb((Limb2(high1) * BASE + high2) / divisor_high);
                }
                // 融合: dividend[quot_idx .. len1] -= qhat * divisor  (单趟, 无临时缓冲)
                int64_t borrow = 0;
                Limb2 carry = 0;
                for (size_t j = 0; j < len2; j++)
                {
                    Limb2 prod = Limb2(divisor[j]) * qhat + carry;
                    Limb q = Limb(prod / BASE);
                    Limb r = Limb(prod - Limb2(q) * BASE);
                    carry = q;
                    int64_t t = int64_t(dividend[quot_idx + j]) - r - borrow;
                    if (t < 0) { t += BASE; borrow = 1; } else borrow = 0;
                    dividend[quot_idx + j] = Limb(t);
                }
                {
                    int64_t t = int64_t(dividend[len1]) - int64_t(carry) - borrow;
                    if (t < 0) { t += BASE; borrow = 1; } else borrow = 0;
                    dividend[len1] = Limb(t);
                }
                // qhat 过大修正 (Knuth: 至多 2 次)。关键: 加回 divisor 后若顶 limb 产生进位
                // (top_carry=1), 说明余数已变非负(=0 进位被丢弃), 应停止; 否则仍为负, 继续.
                while (borrow)
                {
                    borrow = 0;
                    qhat--;
                    Limb2 c = 0;
                    for (size_t j = 0; j < len2; j++)
                    {
                        Limb2 s = Limb2(dividend[quot_idx + j]) + divisor[j] + c;
                        if (s >= BASE) { s -= BASE; c = 1; } else c = 0;
                        dividend[quot_idx + j] = Limb(s);
                    }
                    Limb2 s = Limb2(dividend[len1]) + c;
                    int top_carry = (s >= BASE) ? 1 : 0;
                    if (top_carry) s -= BASE;
                    dividend[len1] = Limb(s);
                    borrow = (top_carry == 0) ? 1 : 0;
                }
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }
'''

out = text[:i] + NEW + text[j:]
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write(out)
print(f"wrote {OUT} ({len(out)} bytes); replaced {j-i} bytes of original")
