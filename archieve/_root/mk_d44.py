# -*- coding: utf-8 -*-
"""D44 = D42 - carryPropSeg 四段主循环里的条件预取块。

理由 (源码级):
  该块每轮固定付 `test $0x7,%bpl; jne` 2 条, 每 8 轮再付 4 条 prefetchnta,
  且 4 个预取地址常驻 -> 抬高寄存器压力, 逼 GCC 每轮把 q0..q3 溢出到栈
  (反汇编实测 4 store + 2 reload)。而 4 条流全是顺序步进, Zen3/TGL 的
  L2 stream prefetcher 天然覆盖 -> 手工 prefetchnta 是净亏。
  prefetch 是纯提示, 删除对语义零影响 (正确性不变)。
"""
import io
import os

PS = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(PS, "best", "div_D42.cpp")
DST = os.path.join(PS, "best", "div_D44.cpp")
NL = "\r\n"

OLD = """                    if ((k & 7) == 0)
                    {
                        HINT_PREFETCH(v + k + 32, 0, 0);
                        HINT_PREFETCH(v + b1 + k + 32, 0, 0);
                        HINT_PREFETCH(v + b2 + k + 32, 0, 0);
                        HINT_PREFETCH(v + b3 + k + 32, 0, 0);
                    }
"""

NEW = """                    // D44: 删除手工预取。4 条流均为顺序步进, 硬件 L2 stream
                    // prefetcher 已覆盖; 而条件预取块每轮固定付 test+jne, 且 4 个
                    // 预取地址常驻抬高寄存器压力, 逼 GCC 每轮把 q0..q3 溢出到栈。
"""

with io.open(SRC, "r", encoding="utf-8") as f:
    s = f.read()

assert s.count(OLD) == 1, "预取块定位失败: %d" % s.count(OLD)
s = s.replace(OLD, NEW, 1)

with io.open(DST, "w", encoding="utf-8", newline=NL) as f:
    f.write(s)
print("div_D44.cpp written, %d chars" % len(s))
