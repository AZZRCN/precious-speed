"""AVX2 变换库（策展式，非通用自动向量化）。

为什么是"策展式"而非"通用自动向量化"：对任意 C++ 做正确自动向量化是编译器级难题，
且对正确性极危险。本优化器采用"已知热惯用法 → 等价 SIMD 改写"的策展模式：
每个变换都经过 bytecmp 闸门验证语义等价，工具只负责"检测惯用法 + 自动套用 + 重测"。

v1 内置两个已验证变换：
  T1 simd_skip_spaces  : 标量 `while(*p<=' ')++p;` → 32B 向量化跳过空白
  T2 simd_put_hex      : 标量逐字节写十六进制 → SIMD 反向滑窗写出

接口可扩展：新增变换只需往 REGISTRY 加一项（detect + apply）。未来可接
LLM 生成 patch 作为"外部变换插件"。
"""
import re

_HELPERS = r"""
#ifndef AVOX2_SIMD_HELPERS
#define AVOX2_SIMD_HELPERS
#include <immintrin.h>
// T1: 32B 向量化跳过连续空白 (>= ' ' 视为分隔符), 返回首个非空白位置
static inline const char* skip_spaces_simd(const char* p) {
    const __m256i sp = _mm256_set1_epi8(' ');
    for (;;) {
        __m256i v = _mm256_loadu_si256((const __m256i*)p);
        unsigned m = (unsigned)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
        if (m) return p + (int)_tzcnt_u32(m);
        p += 32;
    }
}
// T2: 反向写出十六进制（小缓冲 SIMD 滑窗）
static inline char* put_hex_simd(unsigned long long v, char* out) {
    if (v == 0) { *out++ = '0'; return out; }
    char tmp[20]; int n = 0; static const char* H = "0123456789abcdef";
    while (v) { tmp[n++] = H[v & 15]; v >>= 4; }
    char rev[20]; for (int i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    int i = 0;
    for (; i + 16 <= n; i += 16) {
        __m128i x = _mm_loadu_si128((const __m128i*)(rev + i));
        _mm_storeu_si128((__m128i*)(out + i), x);
    }
    for (; i < n; ++i) out[i] = rev[i];
    return out + n;
}
#endif
"""

_SKIP_RE = re.compile(
    r"while\s*\(\s*\*\s*([A-Za-z_]\w*)\s*(?:<=\s*'\s*'|<=\s*32|<\s*0x21|<\s*33|<\s*'\!')\s*\)\s*\+\+\s*\1\s*;",
)

_PUTHEX_RE = re.compile(
    r"char\s+tmp\[20\];\s*int\s+n=0;.*?"
    r"while\(s\)\{[^}]*\}\s*"
    r"for\(int\s+i=n-1;i>=0;--i\)\s*\*\s*out\+\+\s*=\s*tmp\[i\];",
    re.DOTALL,
)


def _inject_helpers(src):
    if "AVOX2_SIMD_HELPERS" in src:
        return src
    # 在最后一个 #include 之后插入
    idx = src.rfind("#include")
    if idx == -1:
        return _HELPERS + "\n" + src
    nl = src.find("\n", idx)
    if nl == -1:
        nl = len(src)
    return src[:nl + 1] + "\n" + _HELPERS + "\n" + src[nl + 1:]


def t_skip_spaces(src):
    if "skip_spaces_simd" in src:
        return src, False
    if not _SKIP_RE.search(src):
        return src, False

    def repl(m):
        v = m.group(1)
        return f"{v} = skip_spaces_simd({v});"

    new = _SKIP_RE.sub(repl, src)
    new = _inject_helpers(new)
    return new, True


def t_put_hex(src):
    if "put_hex_simd" in src:
        return src, False
    if not _PUTHEX_RE.search(src):
        return src, False
    new = _PUTHEX_RE.sub("out = put_hex_simd(s, out);", src)
    new = _inject_helpers(new)
    return new, True


REGISTRY = [
    {"name": "simd_skip_spaces", "desc": "标量空白跳过 → 32B SIMD", "fn": t_skip_spaces},
    {"name": "simd_put_hex", "desc": "标量十六进制写出 → SIMD 滑窗", "fn": t_put_hex},
]


def detect_all(src):
    found = []
    if _SKIP_RE.search(src):
        found.append("simd_skip_spaces")
    if _PUTHEX_RE.search(src):
        found.append("simd_put_hex")
    return found


def apply_chain(src, names):
    log = []
    for t in REGISTRY:
        if t["name"] in names:
            src, applied = t["fn"](src)
            log.append((t["name"], applied))
    return src, log
