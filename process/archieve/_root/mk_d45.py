# -*- coding: utf-8 -*-
"""D45 = D44 + divBASE 用内联汇编的真 mulx。

为什么不用 _mulx_u64:
  该 intrinsic 的低 64 位是**指针出参** (&lo), GCC 必然把 lo 实体化到栈槽,
  反汇编实测出现 `mov %r11,-0x38(%rsp)` 紧接 `mov -0x38(%rsp),%r15` 的
  荒唐存-取对; D43 四段热循环因此从 10.88 涨到 13.20 指令/limb (更差)。

为什么内联汇编能赢:
  `mul %reg` 隐式吃 RAX、写 RDX:RAX -> 每条链都要重新把魔数塞进 RAX
  (movabs 或 mov), 且 RDX 被连环冲掉, 商必须立刻落栈。
  `mulx src, lo, hi` 隐式乘数在 RDX、两个目标寄存器自由:
    - 魔数以 "d"(BARRETT_M) 约束绑定 RDX, 循环不变量 -> GCC 提到循环外只装一次
    - 商直接落在自由寄存器, 不再冲突 -> 溢出消失
  单条指令替掉 (mov+mul) 两条。语义与 __int128 高 64 位逐位一致。
"""
import io
import os

PS = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(PS, "best", "div_D44.cpp")
DST = os.path.join(PS, "best", "div_D45.cpp")
NL = "\r\n"

OLD = "        static uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }\n"

NEW = """        static HINT_AI uint64_t divBASE(uint64_t s)
        {
#if defined(__BMI2__) && defined(__x86_64__) && defined(__GNUC__)
            // D45: 真 mulx —— 魔数常驻 RDX (循环不变量, GCC 提到循环外),
            //      lo/hi 落自由寄存器, 一条指令替掉 (mov %rX,%rax + mul %rM)。
            //      不用 _mulx_u64: 其 &lo 指针出参会把低位实体化到栈 (见 D43)。
            uint64_t _lo, _hi;
            __asm__("mulx %[s], %[lo], %[hi]"
                    : [lo] "=r"(_lo), [hi] "=r"(_hi)
                    : "d"(BARRETT_M), [s] "r"(s));
            (void)_lo;
            return _hi;
#else
            return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
#endif
        }
"""

with io.open(SRC, "r", encoding="utf-8") as f:
    s = f.read()

assert s.count(OLD) == 1, "divBASE 定位失败: %d" % s.count(OLD)
s = s.replace(OLD, NEW, 1)

with io.open(DST, "w", encoding="utf-8", newline=NL) as f:
    f.write(s)
print("div_D45.cpp written, %d chars" % len(s))
