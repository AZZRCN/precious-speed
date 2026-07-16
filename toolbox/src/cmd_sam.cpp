// cmd_sam.cpp - 基于后缀自动机 (SAM) 的文本搜索
// 功能: 统计模式出现次数、首次位置、所有位置；支持通配符 * 和 ?
// SAM 构建一次后可多次查询，适合在大文本中搜索多个模式
#include "common.h"

namespace tbx {

// 将 segment（含 ?）按 ? 拆为字面片段和间隔
struct SegSplit {
    std::vector<std::string> lits;  // 字面片段
    std::vector<size_t> gaps;       // 片段间的 ? 数量（gaps[i] = lits[i] 和 lits[i+1] 间的 ? 数）
};

static SegSplit split_by_q(const std::string& seg) {
    SegSplit s;
    std::string cur;
    for (char c : seg) {
        if (c == '?') {
            s.lits.push_back(cur);
            cur.clear();
            s.gaps.push_back(1);
        } else {
            cur.push_back(c);
        }
    }
    s.lits.push_back(cur);
    // gaps 数量 = lits.size() - 1
    // 连续 ?? 会被多次 push，需合并
    // 重新解析
    s.lits.clear(); s.gaps.clear();
    std::string lit;
    size_t qcount = 0;
    bool in_lit = false;
    for (char c : seg) {
        if (c == '?') {
            if (in_lit) { s.lits.push_back(lit); lit.clear(); in_lit = false; }
            ++qcount;
        } else {
            if (!in_lit) {
                if (!s.lits.empty() || qcount > 0) s.gaps.push_back(qcount);
                qcount = 0;
                in_lit = true;
            }
            lit.push_back(c);
        }
    }
    if (in_lit) {
        if (!s.lits.empty() || qcount > 0) s.gaps.push_back(qcount);
        s.lits.push_back(lit);
    } else if (qcount > 0) {
        // 末尾全 ?，作为空字面片段
        s.gaps.push_back(qcount);
        s.lits.push_back("");
    }
    return s;
}

// 在 text 中查找含 ? 的 segment 的所有结束位置
// 策略：选最长字面片段作为锚点，SAM 查找其所有位置，再验证其余片段
static std::vector<size_t> find_segment(const std::vector<uint8_t>& text,
                                         SuffixAutomatonEx& sam,
                                         const std::string& seg) {
    if (seg.find('?') == std::string::npos) {
        // 纯字面，直接 SAM 查询
        auto raw = sam.all_end_positions((const uint8_t*)seg.data(), seg.size());
        // 转为结束位置（SAM 返回的是结束下标）
        return std::vector<size_t>(raw.begin(), raw.end());
    }
    SegSplit sp = split_by_q(seg);
    // 选最长字面片段作为锚点
    int anchor = 0;
    for (int i = 1; i < (int)sp.lits.size(); ++i)
        if (sp.lits[i].size() > sp.lits[anchor].size()) anchor = i;
    const std::string& a = sp.lits[anchor];
    if (a.empty()) {
        // 全 ?，无法用 SAM，退化为扫描
        std::vector<size_t> res;
        size_t seg_len = seg.size();
        if (seg_len == 0 || seg_len > text.size()) return res;
        for (size_t i = seg_len - 1; i < text.size(); ++i) res.push_back(i);
        return res;
    }
    auto anchor_pos = sam.all_end_positions((const uint8_t*)a.data(), a.size());
    std::vector<size_t> result;
    for (int ep : anchor_pos) {
        // ep 是 anchor 在 text 中的结束下标（0-based）
        // 验证整个 segment 是否匹配
        // segment 结束位置 = ep + (anchor 之后部分的长度)
        // segment 起始位置 = ep - a.size() + 1 - (anchor 之前部分的总长度)
        size_t seg_end = (size_t)ep;
        // 计算 anchor 之后部分长度
        size_t after_len = 0;
        for (int i = anchor + 1; i < (int)sp.lits.size(); ++i) {
            after_len += sp.gaps[i - 1] + sp.lits[i].size();
        }
        seg_end += after_len;
        size_t seg_start = seg_end + 1 - seg.size();
        if (seg_start >= text.size() || seg_end >= text.size()) continue;
        // 验证
        bool ok = true;
        size_t pos = seg_start;
        for (int i = 0; i < (int)sp.lits.size() && ok; ++i) {
            if (i > 0) pos += sp.gaps[i - 1];
            const std::string& lit = sp.lits[i];
            if (lit.empty()) continue;
            if (pos + lit.size() > text.size()) { ok = false; break; }
            for (size_t k = 0; k < lit.size(); ++k) {
                if (text[pos + k] != (uint8_t)lit[k]) { ok = false; break; }
            }
            pos += lit.size();
        }
        if (ok) result.push_back(seg_end);
    }
    return result;
}

// 通配符模式（含 *）匹配：按 * 分割为 segments，贪心匹配
// 返回所有匹配的结束位置
static std::vector<size_t> find_wildcard(const std::vector<uint8_t>& text,
                                          SuffixAutomatonEx& sam,
                                          const std::string& pattern) {
    // 按 * 分割
    std::vector<std::string> segs;
    std::string cur;
    for (char c : pattern) {
        if (c == '*') { segs.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    segs.push_back(cur);

    // 全 * 或空模式：匹配所有位置
    bool all_star = true;
    for (auto& s : segs) if (!s.empty()) { all_star = false; break; }
    if (all_star) {
        std::vector<size_t> res;
        for (size_t i = 0; i <= text.size(); ++i) res.push_back(i == 0 ? 0 : i - 1);
        return res;
    }

    // 找到第一个和最后一个非空 segment
    int first_nonempty = -1, last_nonempty = -1;
    for (int i = 0; i < (int)segs.size(); ++i)
        if (!segs[i].empty()) {
            if (first_nonempty < 0) first_nonempty = i;
            last_nonempty = i;
        }

    // 对第一个非空 segment，查找所有位置
    std::vector<size_t> positions = find_segment(text, sam, segs[first_nonempty]);

    // 如果只有一个非空 segment，直接返回
    if (first_nonempty == last_nonempty) return positions;

    // 多个 segment：贪心匹配
    // 对每个起始匹配，依次找后续 segment 的最早匹配位置
    std::vector<size_t> result;
    for (size_t start_pos : positions) {
        // 第一个 segment 的结束位置 = start_pos
        // 下一个 segment 必须在 start_pos 之后（* 至少匹配 0 字符）
        size_t cur_pos = start_pos;
        bool ok = true;
        for (int i = first_nonempty + 1; i <= last_nonempty; ++i) {
            if (segs[i].empty()) continue; // * 匹配任意
            // 在 cur_pos 之后查找 segs[i] 的最早出现
            auto cand = find_segment(text, sam, segs[i]);
            // 找到第一个 > cur_pos 的（即 segment 起始 > cur_pos - seg.len + 1）
            size_t seg_len = segs[i].size();
            bool found = false;
            for (int cp : cand) {
                // cp 是结束位置，起始 = cp - seg_len + 1
                if (cp + 1 >= seg_len && cp + 1 - seg_len > cur_pos) {
                    cur_pos = (size_t)cp;
                    found = true;
                    break;
                }
            }
            if (!found) { ok = false; break; }
        }
        if (ok) result.push_back(cur_pos);
    }
    return result;
}

int cmd_sam(int argc, char** argv) {
    bool opt_all = false, opt_count = false, opt_wild = false, opt_line = false, opt_json = false;
    int max_out = 1000;
    std::string path, pattern;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-all") opt_all = true;
        else if (a == "-count") opt_count = true;
        else if (a == "-wild" || a == "-wildcard") opt_wild = true;
        else if (a == "-line") opt_line = true;
        else if (a == "-json") opt_json = true;
        else if (a == "-max" && i + 1 < argc) max_out = atoi(argv[++i]);
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx sam <file> <pattern> [options]\n"
                   "  -all       输出所有出现位置\n"
                   "  -count     只输出出现次数\n"
                   "  -wild      启用通配符 (* 任意长度, ? 单字符)\n"
                   "  -line      输出行号而非字节偏移\n"
                   "  -max N     最多输出 N 个位置 (默认 1000)\n"
                   "  -json      JSON 输出\n"
                   "\nSAM 构建一次后可多次查询，适合大文本多次搜索。\n"
                   "通配符 * 分割为子串各自 SAM 查询后贪心组合。\n");
            return 0;
        } else if (path.empty()) path = a;
        else if (pattern.empty()) pattern = a;
    }
    if (path.empty() || pattern.empty()) {
        fprintf(stderr, "error: usage: tbx sam <file> <pattern> [options]\n");
        return 1;
    }

    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return 1;
    }
    if (data.empty()) {
        printf("File is empty.\n");
        return 0;
    }

    // 构建 SAM
    auto t0 = std::chrono::high_resolution_clock::now();
    SuffixAutomatonEx sam(data.size());
    for (size_t i = 0; i < data.size(); ++i)
        sam.extend(data[i], (int)i);
    sam.prepare();
    auto t1 = std::chrono::high_resolution_clock::now();
    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 预计算行偏移（如需行号）
    std::vector<size_t> line_offs;
    if (opt_line) {
        line_offs.push_back(0);
        for (size_t i = 0; i < data.size(); ++i)
            if (data[i] == '\n') line_offs.push_back(i + 1);
    }

    auto to_line = [&](size_t byte_pos) -> size_t {
        // byte_pos 是结束位置，转换为行号（1-based）
        size_t lo = 0, hi = line_offs.size();
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (line_offs[mid] <= byte_pos) lo = mid + 1;
            else hi = mid;
        }
        return lo; // 1-based
    };

    // 查询
    std::vector<size_t> positions;
    long long cnt = -1;

    if (!opt_wild) {
        // 纯字面查询
        if (opt_count) {
            cnt = sam.count((const uint8_t*)pattern.data(), pattern.size());
        } else {
            auto raw = sam.all_end_positions((const uint8_t*)pattern.data(), pattern.size());
            positions.assign(raw.begin(), raw.end());
            cnt = (long long)positions.size();
            if (!opt_all) {
                // 默认只输出次数和首次位置
            }
        }
    } else {
        // 通配符查询
        positions = find_wildcard(data, sam, pattern);
        cnt = (long long)positions.size();
    }

    if (opt_json) {
        printf("{\"file\":\"%s\",\"pattern\":\"%s\",\"build_ms\":%.2f,\"count\":%lld",
               json_escape(path).c_str(), json_escape(pattern).c_str(), build_ms, cnt);
        if (opt_all && !positions.empty()) {
            printf(",\"positions\":[");
            int shown = 0;
            for (size_t i = 0; i < positions.size() && shown < max_out; ++i, ++shown) {
                if (i) printf(",");
                if (opt_line) printf("%zu", to_line(positions[i]));
                else printf("%zu", positions[i]);
            }
            printf("]");
        }
        printf("}\n");
        return 0;
    }

    printf("File   : %s\n", path.c_str());
    printf("Pattern: %s%s\n", pattern.c_str(), opt_wild ? " (wildcard)" : "");
    printf("Size   : %zu bytes\n", data.size());
    printf("Build  : %.2f ms (SAM states: %zu)\n", build_ms, sam.st.size());
    printf("Count  : %lld\n", cnt);

    if (opt_all && !positions.empty()) {
        printf("Positions (%s, max %d):\n", opt_line ? "line" : "byte_end", max_out);
        int shown = 0;
        for (size_t i = 0; i < positions.size() && shown < max_out; ++i, ++shown) {
            if (opt_line)
                printf("  line %zu\n", to_line(positions[i]));
            else
                printf("  %zu\n", positions[i]);
        }
        if ((long long)positions.size() > max_out)
            printf("  ... (%lld more)\n", (long long)positions.size() - max_out);
    } else if (cnt > 0 && !opt_count) {
        // 输出首次位置
        size_t fp = positions.empty() ? (size_t)sam.first_end_pos((const uint8_t*)pattern.data(), pattern.size()) : positions[0];
        if (opt_line)
            printf("First  : line %zu\n", to_line(fp));
        else
            printf("First  : byte %zu\n", fp);
    }
    return 0;
}

} // namespace tbx
