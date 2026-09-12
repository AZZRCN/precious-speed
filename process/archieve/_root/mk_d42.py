# -*- coding: utf-8 -*-
"""D42 / D43 生成器 —— 只动 carryPropSeg 的标量算术, 不动任何 FFT 形状。

D42 = D41 + cvtRoundU64:
  原写法 `uint64_t(v[k] + 0.5)` 在 x86-64 上无单指令实现, GCC 展开为
      vaddsd (+0.5) ; vcvttsd2si ; vcomisd 2^63 ; jae <unsigned fixup>
  实测热循环 (4 段并行, 每轮 4 limb) 因此多出 4 条 vaddsd + 3 组 (vcomisd+jae),
  并因控制流汇合点造成 4 次栈溢出。改用 vcvtsd2si (就近舍入, 有符号) 一条指令。
  等价性: FFT 卷积输出满足 v >= -0.25 且 |v - round(v)| < 0.25,
          故 round-to-nearest(v) == trunc(v + 0.5), 逐位等价; v < 1e13 << 2^63。

D43 = D42 + mulx:
  divBASE 的 `mul r64` 隐式占用 RAX, 4 条链每轮各重载一次 movabs 魔数。
  BMI2 的 mulx 用 RDX 作隐式乘数且目标寄存器自由 -> 魔数只需装载一次。
"""
import io
import os
import re

PS = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(PS, "best", "div_D41.cpp")
NL = "\r\n"

OLD_DIVBASE = "        static uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }\n"

HELPER_BODY = """        // ================= D42: double -> uint64 单指令转换 =================
        // 原写法 (uint64_t)(v + 0.5) 在 x86-64 上**没有**单条指令: double->unsigned
        // 缺少硬件支持, GCC 必须发 2^63 护栏, 实际展开为
        //     vaddsd (+0.5) ; vcvttsd2si ; vcomisd 2^63 ; jae <unsigned fixup>
        // 反汇编 carryPropSeg 热循环 (4 段并行, 每轮 4 limb) 实测 ~50 条指令,
        // 其中 4 条 vaddsd + 3 组 (vcomisd+jae) 纯属浪费, 且分支汇合点逼出 4 次栈溢出。
        //
        // vcvtsd2si 是**有符号**转换, 硬件单指令, 用 MXCSR 当前舍入模式 (就近偶数)。
        // 等价性: FFT 卷积输入非负 => 输出 v >= -0.25, 且误差界给出 |v-round(v)|<0.25
        //   => round_to_nearest(v) == trunc(v + 0.5) 逐位相同 (v 不可能落在 .5 边界);
        //   且 v < N*(BASE-1)^2 ~ 1e13 << 2^63 不溢出。
        // FTZ/DAZ 只影响非规格化, 不改舍入方向; 本处量级 ~1e12 与非规格化无关。
        static HINT_AI uint64_t cvtRoundU64(const double *p)
        {
            return (uint64_t)_mm_cvtsd_si64(_mm_load_sd(p));
        }
"""

MULX_DIVBASE = """        static uint64_t divBASE(uint64_t s)
        {
#if defined(__BMI2__) && defined(__x86_64__)
            // D43: mul r64 隐式占用 RAX -> 4 条并行进位链每轮各重载一次 movabs 魔数,
            //      且 RAX 争用逼出栈溢出。mulx 隐式乘数在 RDX、目标寄存器自由,
            //      魔数只装载一次。语义与 __int128 高 64 位完全一致。
            unsigned long long _lo;
            return (uint64_t)_mulx_u64((unsigned long long)s,
                                       (unsigned long long)BARRETT_M, &_lo);
#else
            return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
#endif
        }
"""

CONV = [
    ("uint64_t s0 = c0 + uint64_t(v[k]      + 0.5);",
     "uint64_t s0 = c0 + cvtRoundU64(v + k);"),
    ("uint64_t s1 = c1 + uint64_t(v[b1 + k] + 0.5);",
     "uint64_t s1 = c1 + cvtRoundU64(v + b1 + k);"),
    ("uint64_t s2 = c2 + uint64_t(v[b2 + k] + 0.5);",
     "uint64_t s2 = c2 + cvtRoundU64(v + b2 + k);"),
    ("uint64_t s3 = c3 + uint64_t(v[b3 + k] + 0.5);",
     "uint64_t s3 = c3 + cvtRoundU64(v + b3 + k);"),
    ("uint64_t s0 = carry + uint64_t(v[i]   + 0.5);",
     "uint64_t s0 = carry + cvtRoundU64(v + i);"),
]
for j in range(1, 8):
    CONV.append(("uint64_t s%d = q%d + uint64_t(v[i+%d] + 0.5);" % (j, j - 1, j),
                 "uint64_t s%d = q%d + cvtRoundU64(v + i + %d);" % (j, j - 1, j)))

CONV_ALL = [("carry += uint64_t(v[i] + 0.5);", "carry += cvtRoundU64(v + i);", 2)]


def build(dst, with_mulx):
    with io.open(SRC, "r", encoding="utf-8") as f:
        s = f.read()          # universal newlines -> \n
    n0 = len(s)

    assert s.count(OLD_DIVBASE) == 1, "divBASE 定位失败"
    head = MULX_DIVBASE if with_mulx else OLD_DIVBASE
    s = s.replace(OLD_DIVBASE, head + HELPER_BODY, 1)

    for old, new in CONV:
        c = s.count(old)
        assert c == 1, "唯一性失败(%d): %s" % (c, old)
        s = s.replace(old, new, 1)
    for old, new, cnt in CONV_ALL:
        c = s.count(old)
        assert c == cnt, "计数失败(%d != %d): %s" % (c, cnt, old)
        s = s.replace(old, new)

    left = re.findall(r"uint64_t\([^)]*\+ 0\.5\)", s)
    assert not left, "仍有未替换的转换: %r" % (left,)

    with io.open(dst, "w", encoding="utf-8", newline=NL) as f:
        f.write(s)
    print("%s  %d -> %d chars" % (os.path.basename(dst), n0, len(s)))


build(os.path.join(PS, "best", "div_D42.cpp"), False)
build(os.path.join(PS, "best", "div_D43.cpp"), True)
