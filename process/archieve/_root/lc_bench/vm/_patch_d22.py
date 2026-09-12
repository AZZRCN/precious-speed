# _patch_d22.py —— D21 -> D22: 把 pack/unpack 融合进 C4 主循环的 load/store
#
# D21 实测 (lri_03):
#   difC4  33.08M  (D19 dif  70.81M, -53%)   <- C4 蝶形本身完全达标
#   iditC4 30.71M  (D19 idit 63.93M, -52%)
#   但总 Ir 只降 28.2M 而非 71M => 独立的 packC4 趟次吃掉 ~43M, 外加 +827K D1 miss。
#
# 独立 pack: 每 8 double = 2 load + 2 perm + 2 store + ~3 循环开销 = 9 条 + 一整趟内存流量
# 融合 pack: 每 8 double = 2 条 perm (借用主循环本来就有的 load/store), 零额外内存流量
#   => 省 7/9 的指令 + 全部额外 cache miss
#
# 做法: difC4/iditC4 加模板参数 MODE 描述"边界格式"
#   0 = C4 (递归内部)   1 = C2/RRII (与原 C2 路径交界)   2 = RIRI (最外层实数打包格式)
#   dif : 最外层 difC4<1|2>  主循环 load 时转换; 递归 difC4<0>
#   idit: 最外层 iditC4<1|2> 主循环 store 时转换 (idit 主循环在递归之后, 恰好是最后一步)
# 叶子处的 packC4 暂时保留 (要消掉它需要 C4 版 difSmall/difDispatch, 下一步再做)。
import io

SRC = r"D:\precious_speed\best\div_D21.cpp"
DST = r"D:\precious_speed\best\div_D22.cpp"

s = io.open(SRC, "r", encoding="utf-8", newline="").read()
orig = len(s)

# ---------------------------------------------------------------- 1) 边界 load/store 原语
ANCHOR_INFRA = """            // RRII|RRII <-> RRRR|IIII 。perm2f128(A,B,0x20)/(A,B,0x31) 这一对是**对合**,"""
NEW_INFRA = r"""            // ---- 边界格式的融合 load/store (D22) ----
            // C2/RRII 内存 -> C4 寄存器: 2 条 perm2f128, 复用主循环本来就有的 load
            HINT_AI C4d c4loadC2(const double *p)
            {
                const __m256d a = _mm256_load_pd(p), b = _mm256_load_pd(p + 4);
                return C4d{_mm256_permute2f128_pd(a, b, 0x20), _mm256_permute2f128_pd(a, b, 0x31)};
            }
            HINT_AI void c4storeC2(double *p, const C4d &c)
            {
                _mm256_store_pd(p, _mm256_permute2f128_pd(c.re, c.im, 0x20));
                _mm256_store_pd(p + 4, _mm256_permute2f128_pd(c.re, c.im, 0x31));
            }
            // RIRI 内存 <-> C4 寄存器: 4 条 shuffle
            HINT_AI C4d c4loadRIRI(const double *p)
            {
                const __m256d a = _mm256_load_pd(p), b = _mm256_load_pd(p + 4);
                return C4d{_mm256_permute4x64_pd(_mm256_unpacklo_pd(a, b), 0xD8),
                           _mm256_permute4x64_pd(_mm256_unpackhi_pd(a, b), 0xD8)};
            }
            HINT_AI void c4storeRIRI(double *p, const C4d &c)
            {
                const __m256d lo = _mm256_unpacklo_pd(c.re, c.im);
                const __m256d hi = _mm256_unpackhi_pd(c.re, c.im);
                _mm256_store_pd(p, _mm256_permute2f128_pd(lo, hi, 0x20));
                _mm256_store_pd(p + 4, _mm256_permute2f128_pd(lo, hi, 0x31));
            }
            // MODE: 0 = C4, 1 = C2/RRII, 2 = RIRI
            template <int MODE>
            HINT_AI C4d c4loadM(const double *p)
            {
                if constexpr (MODE == 0)
                    return c4load(p);
                else if constexpr (MODE == 1)
                    return c4loadC2(p);
                else
                    return c4loadRIRI(p);
            }
            template <int MODE>
            HINT_AI void c4storeM(double *p, const C4d &c)
            {
                if constexpr (MODE == 0)
                    c4store(p, c);
                else if constexpr (MODE == 1)
                    c4storeC2(p, c);
                else
                    c4storeRIRI(p, c);
            }
            // RRII|RRII <-> RRRR|IIII 。perm2f128(A,B,0x20)/(A,B,0x31) 这一对是**对合**,"""
assert s.count(ANCHOR_INFRA) == 1, "infra"
s = s.replace(ANCHOR_INFRA, NEW_INFRA)

# ---------------------------------------------------------------- 2) difC4 -> 模板
OLD_DIF = """                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            packC4(inout, float_len); // C4 -> RRII, 回到原路径
                            dif<false>(inout, float_len);
                            return;
                        }"""
NEW_DIF = """                template <int IN_MODE>
                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            // 只有递归内部 (IN_MODE==0) 会落到叶子: 最外层进来时 float_len > FFT_C4_MIN
                            packC4(inout, float_len); // C4 -> RRII, 回到原路径
                            dif<false>(inout, float_len);
                            return;
                        }"""
assert s.count(OLD_DIF) == 1, "difC4 head"
s = s.replace(OLD_DIF, NEW_DIF)

OLD_DIF_BODY = """                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4load(it + s2), c3 = c4load(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                        }
                        const size_t stride = float_len / 4;
                        difC4(inout, stride * 2);
                        difC4(inout + stride * 2, stride);
                        difC4(inout + stride * 3, stride);"""
NEW_DIF_BODY = """                            C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
                            C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                        }
                        const size_t stride = float_len / 4;
                        difC4<0>(inout, stride * 2);
                        difC4<0>(inout + stride * 2, stride);
                        difC4<0>(inout + stride * 3, stride);"""
assert s.count(OLD_DIF_BODY) == 1, "difC4 body"
s = s.replace(OLD_DIF_BODY, NEW_DIF_BODY)

# ---------------------------------------------------------------- 3) iditC4 -> 模板
OLD_IDIT = """                void iditC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len); // RRII -> C4, 交回上层
                            return;
                        }
                        const size_t stride = float_len / 4;
                        iditC4(inout, stride * 2);
                        iditC4(inout + stride * 2, stride);
                        iditC4(inout + stride * 3, stride);"""
NEW_IDIT = """                template <int OUT_MODE>
                void iditC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len); // RRII -> C4, 交回上层
                            return;
                        }
                        const size_t stride = float_len / 4;
                        iditC4<0>(inout, stride * 2);
                        iditC4<0>(inout + stride * 2, stride);
                        iditC4<0>(inout + stride * 3, stride);"""
assert s.count(OLD_IDIT) == 1, "iditC4 head"
s = s.replace(OLD_IDIT, NEW_IDIT)

OLD_IDIT_BODY = """                            iditSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c2);
                            c4store(it + s3, c3);"""
NEW_IDIT_BODY = """                            iditSplitC4(c0, c1, c2, c3);
                            c4storeM<OUT_MODE>(it, c0);
                            c4storeM<OUT_MODE>(it + s1, c1);
                            c4storeM<OUT_MODE>(it + s2, c2);
                            c4storeM<OUT_MODE>(it + s3, c3);"""
assert s.count(OLD_IDIT_BODY) == 1, "iditC4 body"
s = s.replace(OLD_IDIT_BODY, NEW_IDIT_BODY)

# ---------------------------------------------------------------- 4) 入口: 去掉独立 pack 趟
OLD_ENTRY_DIF = """                        if (float_len > FFT_C4_MIN)
                        {
                            if constexpr (RIRI_IN)
                            {
                                packC4RIRI(inout, float_len);
                            }
                            else
                            {
                                packC4(inout, float_len);
                            }
                            difC4(inout, float_len);
                            return;
                        }"""
NEW_ENTRY_DIF = """                        if (float_len > FFT_C4_MIN)
                        {
                            // D22: 边界格式转换融合进主循环的 load, 不再单独走一趟内存
                            difC4<RIRI_IN ? 2 : 1>(inout, float_len);
                            return;
                        }"""
assert s.count(OLD_ENTRY_DIF) == 1, "entry dif"
s = s.replace(OLD_ENTRY_DIF, NEW_ENTRY_DIF)

OLD_ENTRY_IDIT = """                        if (float_len > FFT_C4_MIN)
                        {
                            iditC4(inout, float_len);
                            if constexpr (RIRI_OUT)
                            {
                                unpackC4RIRI(inout, float_len);
                            }
                            else
                            {
                                packC4(inout, float_len);
                            }
                            return;
                        }"""
NEW_ENTRY_IDIT = """                        if (float_len > FFT_C4_MIN)
                        {
                            // D22: idit 主循环在递归之后, 最外层那趟正好是最后一步 -> store 时转换
                            iditC4<RIRI_OUT ? 2 : 1>(inout, float_len);
                            return;
                        }"""
assert s.count(OLD_ENTRY_IDIT) == 1, "entry idit"
s = s.replace(OLD_ENTRY_IDIT, NEW_ENTRY_IDIT)

io.open(DST, "w", encoding="utf-8", newline="").write(s)
print("D22 written, %d -> %d bytes (%+d)" % (orig, len(s), len(s) - orig))
