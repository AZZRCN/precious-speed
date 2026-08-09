// 全域穷举证明: G,P 不相交时
//    cin_bits = (P + c0) ^ (P + 2G)          [3 ops]
// 等价于当前 div.cpp 的
//    A=G, B=G|P; s=A+B+c0; cin_bits = s^A^B  [5 ops]
// 枚举所有不相交 (G,P) 对 = 3^16 = 43,046,721, 再 x2 (c0 = 0/1)。
#include <cstdio>
#include <cstdint>
#include <chrono>

int main()
{
    const uint32_t W = 16;
    const uint32_t FULL = (1u << W) - 1;
    uint64_t checked = 0, bad = 0;
    uint32_t firstBadG = 0, firstBadP = 0, firstBadC = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (uint32_t G = 0; G <= FULL; G++)
    {
        uint32_t mask = FULL & ~G;          // P 只能取 ~G 的子集
        uint32_t sub = mask;
        while (true)
        {
            uint32_t P = sub;
            for (uint32_t c0 = 0; c0 < 2; c0++)
            {
                uint32_t A = G, B = G | P;
                uint32_t s = A + B + c0;
                uint32_t ref = s ^ A ^ B;           // 参考: 当前实现
                uint32_t got = (P + c0) ^ (P + 2u * G);   // 候选: 超优化器解
                checked++;
                if (got != ref)
                {
                    if (bad == 0) { firstBadG = G; firstBadP = P; firstBadC = c0; }
                    bad++;
                }
                // 顺带核对总进位位
                uint32_t refCarry = (s >> 16) & 1u;
                uint32_t gotCarry = (got >> 16) & 1u;
                if (refCarry != gotCarry)
                {
                    if (bad == 0) { firstBadG = G; firstBadP = P; firstBadC = c0; }
                    bad++;
                }
            }
            if (sub == 0) break;
            sub = (sub - 1) & mask;
        }
    }
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("checked=%llu  mismatches=%llu  %.2fs\n",
           (unsigned long long)checked, (unsigned long long)bad, el);
    if (bad) printf("  first bad: G=%#x P=%#x c0=%u\n", firstBadG, firstBadP, firstBadC);
    else     printf("PROVED: cin_bits = (P + c0) ^ (P + 2G)   for all disjoint G,P (16-bit)\n");
    return bad ? 1 : 0;
}
