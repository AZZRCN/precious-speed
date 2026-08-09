// data_gen.h - 有特征的大整数测试数据生成器
// FFT KILLER 思想: 最大化卷积中间值、carry chain 长度、边界触发
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>

namespace DataGen {

// ===== xorshift64 伪随机数 (thread_local, 固定种子可复现) =====
inline uint64_t& rngState() {
    static thread_local uint64_t s = 88172645463325252ULL;
    return s;
}
inline uint64_t rng() {
    uint64_t x = rngState();
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    rngState() = x;
    return x;
}
inline uint32_t rngU32(uint32_t mod) { return uint32_t(rng() % mod); }
inline size_t rngSize(size_t lo, size_t hi) { return lo + rng() % (hi - lo + 1); }

// ===== 字符串大整数辅助 (十进制, 支持负号) =====

// 去前导零
inline std::string stripLeadingZeros(std::string s) {
    bool neg = !s.empty() && s[0] == '-';
    size_t start = neg ? 1 : 0;
    while (start < s.size() - 1 && s[start] == '0') start++;
    std::string r = s.substr(start);
    if (neg && r != "0") r = "-" + r;
    return r;
}

// 比较绝对值大小
inline int cmpAbs(const std::string& a, const std::string& b) {
    std::string aa = a[0] == '-' ? a.substr(1) : a;
    std::string bb = b[0] == '-' ? b.substr(1) : b;
    if (aa.size() != bb.size()) return aa.size() > bb.size() ? 1 : -1;
    return aa.compare(bb);
}

// 无符号加法 (a, b >= 0, 无前导零)
inline std::string addAbs(std::string a, std::string b) {
    if (a.size() < b.size()) std::swap(a, b);
    std::reverse(a.begin(), a.end());
    std::reverse(b.begin(), b.end());
    std::string r;
    r.reserve(a.size() + 1);
    int carry = 0;
    for (size_t i = 0; i < a.size(); i++) {
        int s = (a[i] - '0') + carry + (i < b.size() ? (b[i] - '0') : 0);
        r += char('0' + s % 10);
        carry = s / 10;
    }
    if (carry) r += char('0' + carry);
    std::reverse(r.begin(), r.end());
    return r;
}

// 无符号减法 (a >= b >= 0)
inline std::string subAbs(std::string a, std::string b) {
    std::reverse(a.begin(), a.end());
    std::reverse(b.begin(), b.end());
    std::string r;
    r.reserve(a.size());
    int borrow = 0;
    for (size_t i = 0; i < a.size(); i++) {
        int s = (a[i] - '0') - borrow - (i < b.size() ? (b[i] - '0') : 0);
        if (s < 0) { s += 10; borrow = 1; } else borrow = 0;
        r += char('0' + s);
    }
    while (r.size() > 1 && r.back() == '0') r.pop_back();
    std::reverse(r.begin(), r.end());
    return r;
}

// 带符号加法
inline std::string addDecimal(const std::string& a, const std::string& b) {
    bool na = !a.empty() && a[0] == '-';
    bool nb = !b.empty() && b[0] == '-';
    std::string aa = na ? a.substr(1) : a;
    std::string bb = nb ? b.substr(1) : b;
    if (!na && !nb) return addAbs(aa, bb);
    if (na && nb) return "-" + addAbs(aa, bb);
    // 一正一负
    int c = cmpAbs(aa, bb);
    if (c == 0) return "0";
    if (c > 0) {
        if (na) return "-" + subAbs(aa, bb);
        return subAbs(aa, bb);
    } else {
        if (nb) return "-" + subAbs(bb, aa);
        return subAbs(bb, aa);
    }
}

// a - 1 (a > 0)
inline std::string decOne(const std::string& a) {
    if (a == "0") return "-1";
    if (a[0] == '-') return "-" + addAbs(a.substr(1), "1");
    return subAbs(a, "1");
}

// 取反
inline std::string neg(const std::string& a) {
    if (a == "0") return "0";
    if (a[0] == '-') return a.substr(1);
    return "-" + a;
}

// 无符号乘法 O(n*m), 仅用于构造小数据整除用例
inline std::string mulAbs(const std::string& a, const std::string& b) {
    std::vector<int> r(a.size() + b.size(), 0);
    for (int i = a.size() - 1; i >= 0; i--)
        for (int j = b.size() - 1; j >= 0; j--)
            r[i + j + 1] += (a[i] - '0') * (b[j] - '0');
    for (int i = r.size() - 1; i > 0; i--) {
        r[i - 1] += r[i] / 10;
        r[i] %= 10;
    }
    std::string s;
    size_t start = 0;
    while (start < r.size() - 1 && r[start] == 0) start++;
    for (size_t i = start; i < r.size(); i++) s += char('0' + r[i]);
    return s;
}

// ===== 基础生成器 =====

inline std::string allNines(size_t digits) { return std::string(digits, '9'); }
inline std::string zero() { return "0"; }
inline std::string one() { return "1"; }
inline std::string powerOf10(size_t n) { return "1" + std::string(n, '0'); }

// 随机数字字符串 (首位非零, 除非 digits=1)
inline std::string randomDigits(size_t digits) {
    if (digits == 0) return "0";
    std::string s;
    s.reserve(digits);
    s += char('1' + rngU32(9));
    for (size_t i = 1; i < digits; i++) s += char('0' + rngU32(10));
    return s;
}

// 随机带符号数字
inline std::string randomSigned(size_t digits) {
    if (digits == 0) return "0";
    std::string s = randomDigits(digits);
    if (rng() & 1) s = "-" + s;
    return s;
}

// 全 9999 limb (字符串 = "9999" * limbCount)
inline std::string all9999Limbs(size_t limbCount) {
    return std::string(limbCount * 4, '9');
}

// 交替 0000 和 9999 (limb 级别)
inline std::string alternatingLimbs(size_t limbCount) {
    std::string s;
    s.reserve(limbCount * 4);
    for (size_t i = 0; i < limbCount; i++)
        s += (i & 1) ? "9999" : "0000";
    // 去前导零
    size_t start = s.find_first_not_of('0');
    if (start == std::string::npos) return "0";
    return s.substr(start);
}

// 单个高位 limb + 大量 0000
inline std::string singleHighLimb(size_t totalLimbs) {
    if (totalLimbs == 0) return "0";
    std::string s = "9999";
    for (size_t i = 1; i < totalLimbs; i++) s += "0000";
    return s;
}

// 接近 2^53 = 9007199254740992
inline std::string near2Pow53() {
    static const char* vals[] = {
        "9007199254740992",   // 2^53
        "9007199254740991",   // 2^53 - 1
        "9007199254740993",   // 2^53 + 1
        "4503599627370496",   // 2^52
        "18014398509481984",  // 2^54
    };
    return vals[rngU32(5)];
}

// 除数高位 limb = 5000 (归一化边界)
inline std::string divisorHalfBase(size_t limbCount) {
    if (limbCount == 0) return "5000";
    std::string s = "5000";
    for (size_t i = 1; i < limbCount; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%04d", (int)rngU32(10000));
        s += buf;
    }
    return s;
}

// ===== 测试用例结构 =====
struct TestCase {
    std::string a, b;
    std::string pattern; // 数据特征名称
};

// 长度 -> 位数 (BASE=10^4, 1 limb = 4 位)
inline size_t limbsToDigits(size_t limbs) { return limbs * 4; }

// ===== ADD 测试用例生成 =====
inline std::vector<TestCase> generateAddCases(int target) {
    std::vector<TestCase> v;
    v.reserve(target);

    auto add = [&](const std::string& a, const std::string& b, const char* pat) {
        v.push_back({a, b, pat});
    };

    // 1. 全 9 同长度 (carry chain 极限) - 15%
    {
        size_t lens[] = {1, 2, 4, 8, 16, 31, 32, 33, 63, 64, 65, 95, 96, 97,
                         255, 256, 257, 511, 512, 513, 1023, 1024, 1025,
                         4095, 4096, 4097, 9999, 10000, 10001, 49999, 50000, 500001};
        for (size_t len : lens) {
            if (v.size() >= target * 15 / 100) break;
            add(allNines(len), allNines(len), "ALL_9_SAME_LEN");
        }
    }

    // 2. 全 9 + 1 (最长 carry 链) - 10%
    {
        size_t lens[] = {1, 4, 8, 16, 31, 32, 33, 63, 64, 65, 127, 128, 129,
                         255, 256, 257, 511, 512, 513, 1023, 1024, 1025,
                         4095, 4096, 4097, 10000, 50000, 100000, 500000};
        for (size_t len : lens) {
            if (v.size() >= target * 25 / 100) break;
            add(allNines(len), "1", "ALL_9_PLUS_1");
        }
    }

    // 3. 0 + X - 5%
    while (v.size() < target * 30 / 100) {
        size_t len = rngSize(1, 10000);
        add("0", randomSigned(len), "ZERO_PLUS_X");
    }

    // 4. A + (-A) = 0 - 5%
    while (v.size() < target * 35 / 100) {
        size_t len = rngSize(1, 50000);
        std::string a = randomDigits(len);
        add(a, neg(a), "A_PLUS_NEG_A");
    }

    // 5. 正 + 负 (符号变化) - 10%
    while (v.size() < target * 45 / 100) {
        size_t la = rngSize(1, 5000), lb = rngSize(1, 5000);
        add(randomSigned(la), randomSigned(lb), "POS_PLUS_NEG");
    }

    // 6. 长度差异大 - 5%
    while (v.size() < target * 50 / 100) {
        size_t la = rngSize(1, 10), lb = rngSize(1000, 50000);
        if (rng() & 1) std::swap(la, lb);
        add(randomSigned(la), randomSigned(lb), "LEN_MISMATCH");
    }

    // 7. str32 边界 (31/32/33/63/64/65/95/96/97 位) - 10%
    {
        size_t blens[] = {31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128, 129};
        for (size_t len : blens) {
            for (int k = 0; k < 8; k++) {
                if (v.size() >= target * 60 / 100) break;
                add(randomSigned(len), randomSigned(len), "STR32_BOUNDARY");
            }
        }
    }

    // 8. 负 + 负 - 5%
    while (v.size() < target * 65 / 100) {
        size_t la = rngSize(1, 5000), lb = rngSize(1, 5000);
        add("-" + randomDigits(la), "-" + randomDigits(lb), "BOTH_NEG");
    }

    // 9. 随机小 (1-100 位) - 15%
    while (v.size() < target * 80 / 100) {
        add(randomSigned(rngSize(1, 100)), randomSigned(rngSize(1, 100)), "RANDOM_SMALL");
    }

    // 10. 随机中 (100-10000 位) - 10%
    while (v.size() < target * 90 / 100) {
        add(randomSigned(rngSize(100, 10000)), randomSigned(rngSize(100, 10000)), "RANDOM_MED");
    }

    // 11. 随机大 (10000-100000 位) - 5%
    while (v.size() < target * 95 / 100) {
        add(randomSigned(rngSize(10000, 100000)), randomSigned(rngSize(10000, 100000)), "RANDOM_LARGE");
    }

    // 12. 随机超大 (100000-500000 位) - 5%
    while (v.size() < target) {
        add(randomSigned(rngSize(100000, 500000)), randomSigned(rngSize(100000, 500000)), "RANDOM_HUGE");
    }

    // 打乱顺序 (避免同 pattern 连续运行影响缓存)
    for (int i = v.size() - 1; i > 0; i--) {
        int j = (int)rngU32(i + 1);
        std::swap(v[i], v[j]);
    }
    return v;
}

// ===== MUL 测试用例生成 (FFT KILLER 重点) =====
inline std::vector<TestCase> generateMulCases(int target) {
    std::vector<TestCase> v;
    v.reserve(target);

    auto add = [&](const std::string& a, const std::string& b, const char* pat) {
        v.push_back({a, b, pat});
    };

    // 1. 全 9999 limb (FFT 精度极限) - 15%
    {
        size_t limbs[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
                          4096, 8192, 16384, 32768, 62500};
        for (size_t lb : limbs) {
            if (v.size() >= target * 15 / 100) break;
            add(all9999Limbs(lb), all9999Limbs(lb), "ALL_9999_LIMB");
        }
    }

    // 2. 全 9 × 全 9 - 5%
    {
        size_t lens[] = {1, 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 10000, 50000, 100000};
        for (size_t len : lens) {
            if (v.size() >= target * 20 / 100) break;
            add(allNines(len), allNines(len), "ALL_9_MUL");
        }
    }

    // 3. 1 × 大数 - 5%
    while (v.size() < target * 25 / 100) {
        add("1", randomSigned(rngSize(1, 100000)), "ONE_TIMES_LARGE");
    }

    // 4. 0 × 任意 - 3%
    while (v.size() < target * 28 / 100) {
        add("0", randomSigned(rng