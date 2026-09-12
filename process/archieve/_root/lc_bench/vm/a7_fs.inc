    // single-pass parse: one SIMD sweep detects delimiter and converts limbs together
    const char* fromCharRangeSingle(const char* start, const char* end) {
        const char* p = start;
        if (*p == '-') { sign = true; p++; } else { sign = false; }
        data.clear();
        const char* r = p;
        while (r + 16 <= end) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(r));
            __m128i sub = _mm_sub_epi8(v, _mm_set1_epi8('0'));
            __m128i le9 = _mm_cmpeq_epi8(_mm_min_epu8(sub, _mm_set1_epi8(9)), sub);
            uint16_t m = uint16_t(_mm_movemask_epi8(le9));
            if (m == 0xFFFF) {
                uint16_t blk[4];
                str16to4limbs(r, blk);
                data.push_back(blk[3]);
                data.push_back(blk[2]);
                data.push_back(blk[1]);
                data.push_back(blk[0]);
                r += 16;
            } else {
                unsigned mm = (~uint32_t(m)) & 0xFFFFu;
                size_t off = __builtin_ctz(mm);
                const char* e2 = r + off;
                size_t tl = e2 - r;
                if (tl) {
                    size_t lead = tl % 4;
                    const char* t = r;
                    if (lead) {
                        uint16_t vv = 0;
                        for (size_t i = 0; i < lead; i++) vv = uint16_t(vv * 10 + (t[i] - '0'));
                        data.push_back(vv);
                        t += lead;
                    }
                    while (t + 4 <= e2) { data.push_back(str4toi(t)); t += 4; }
                }
                r = e2;
                break;
            }
        }
        if (r < end && *r >= '0' && *r <= '9') {
            const char* e2 = r;
            while (e2 < end && e2[0] >= '0' && e2[0] <= '9') e2++;
            size_t tl = e2 - r;
            size_t lead = tl % 4;
            const char* t = r;
            if (lead) {
                uint16_t vv = 0;
                for (size_t i = 0; i < lead; i++) vv = uint16_t(vv * 10 + (t[i] - '0'));
                data.push_back(vv);
                t += lead;
            }
            while (t + 4 <= e2) { data.push_back(str4toi(t)); t += 4; }
            r = e2;
        }
        if (data.empty()) { clear(); return r; }
        for (size_t i = 0; i < data.size() / 2; i++) {
            uint16_t tmp = data[i]; data[i] = data[data.size() - 1 - i]; data[data.size() - 1 - i] = tmp;
        }
        removeLeadingZero();
        return r;
    }
