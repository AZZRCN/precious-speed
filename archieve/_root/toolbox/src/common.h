// common.h - toolbox 公共功能头文件
// 提供文件读取、字符分类、SAM 后缀自动机、通配符匹配、HTTP 请求等通用能力
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <chrono>
#include <atomic>

#pragma comment(lib, "winhttp.lib")

namespace tbx {

// ===================== 文件读取 =====================

// 二进制安全读取整个文件，失败返回 false
inline bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, path.c_str(), "rb") != 0 || !fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return false; }
    fseek(fp, 0, SEEK_SET);
    out.resize((size_t)sz);
    size_t rd = fread(out.data(), 1, out.size(), fp);
    fclose(fp);
    out.resize(rd);
    return true;
}

// 文本读取（按字节，不做编码转换），失败返回 false
inline bool read_text(const std::string& path, std::string& out) {
    std::vector<uint8_t> raw;
    if (!read_file(path, raw)) return false;
    out.assign(raw.begin(), raw.end());
    return true;
}

// 跳过 UTF-8 BOM
inline void skip_bom(const std::string& s, size_t& pos) {
    if (s.size() >= 3 && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF)
        pos = 3;
}

// ===================== 字符分类 =====================

enum CharClass {
    TCC_NONE = 0,
    TCC_UPPER = 1,      // A-Z
    TCC_LOWER = 2,      // a-z
    TCC_DIGIT = 3,      // 0-9
    TCC_WHITESPACE = 4, // 空白
    TCC_PUNCT = 5,      // ASCII 标点
    TCC_CTRL = 6,       // 控制符
    TCC_HIGH = 7,       // 非 ASCII（>=0x80，多字节 UTF-8 首字节或续字节）
    TCC_OTHER = 8
};

inline CharClass classify_byte(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return TCC_UPPER;
    if (c >= 'a' && c <= 'z') return TCC_LOWER;
    if (c >= '0' && c <= '9') return TCC_DIGIT;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') return TCC_WHITESPACE;
    if (c < 0x20 || c == 0x7F) return TCC_CTRL;
    if (c >= 0x80) return TCC_HIGH;
    return TCC_PUNCT; // 其余可见 ASCII 标点
}

inline const char* class_name(CharClass c) {
    switch (c) {
    case TCC_UPPER: return "UPPER";
    case TCC_LOWER: return "LOWER";
    case TCC_DIGIT: return "DIGIT";
    case TCC_WHITESPACE: return "WS";
    case TCC_PUNCT: return "PUNCT";
    case TCC_CTRL: return "CTRL";
    case TCC_HIGH: return "HIGH";
    default: return "OTHER";
    }
}

// 判断是否"字母"（大写+小写+非ASCII可视为字母类，按需）
inline bool is_alpha_like(uint8_t c) {
    CharClass k = classify_byte(c);
    return k == TCC_UPPER || k == TCC_LOWER;
}

// ===================== 行处理 =====================

// 返回每行的起始偏移（最后一行即使无 \n 也包含）
inline std::vector<size_t> line_offsets(const std::string& s) {
    std::vector<size_t> offs;
    offs.push_back(0);
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\n' && i + 1 < s.size()) offs.push_back(i + 1);
    return offs;
}

// 取第 (1-based) line_no 行内容（不含换行符）
inline std::string get_line(const std::string& s, size_t line_no) {
    if (line_no == 0) return "";
    size_t line = 1, start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (line == line_no) {
            size_t end = i;
            while (end < s.size() && s[end] != '\n' && s[end] != '\r') ++end;
            return s.substr(start, end - start);
        }
        if (s[i] == '\n') { ++line; start = i + 1; }
    }
    if (line == line_no) {
        size_t end = start;
        while (end < s.size() && s[end] != '\n' && s[end] != '\r') ++end;
        return s.substr(start, end - start);
    }
    return "";
}

// ===================== SAM 后缀自动机 =====================
// 字节 alphabet (0-255)，支持二进制安全搜索
// 提供：出现次数、首次位置、所有位置（link 树 DFS）

struct SamState {
    int len = 0;       // 该状态代表的最长子串长度
    int link = -1;     // 后缀链接
    int first_pos = -1;// 该状态子串的任一结束位置（构建时记录）
    long long occ = 0; // 出现次数（link 树子树叶节点数）
    std::unordered_map<uint8_t, int> next;
};

class SuffixAutomaton {
public:
    std::vector<SamState> st;
    int last = 0;

    explicit SuffixAutomaton(size_t reserve_bytes = 0) {
        // 预留 2n 状态
        st.reserve(reserve_bytes * 2 + 4);
        st.emplace_back(); // state 0
        st[0].len = 0;
        st[0].link = -1;
        last = 0;
    }

    // 在线扩展一个字节 c
    void extend(uint8_t c, int pos) {
        int cur = (int)st.size();
        st.emplace_back();
        st[cur].len = st[last].len + 1;
        st[cur].first_pos = pos;
        st[cur].occ = 1; // 叶节点

        int p = last;
        while (p != -1 && !st[p].next.count(c)) {
            st[p].next[c] = cur;
            p = st[p].link;
        }
        if (p == -1) {
            st[cur].link = 0;
        } else {
            int q = st[p].next[c];
            if (st[p].len + 1 == st[q].len) {
                st[cur].link = q;
            } else {
                int clone = (int)st.size();
                st.emplace_back();
                st[clone].len = st[p].len + 1;
                st[clone].link = st[q].link;
                st[clone].next = st[q].next;
                st[clone].first_pos = st[q].first_pos; // 克隆继承 q 的位置
                st[clone].occ = 0; // 克隆非叶
                while (p != -1 && st[p].next[c] == q) {
                    st[p].next[c] = clone;
                    p = st[p].link;
                }
                st[q].link = clone;
                st[cur].link = clone;
            }
        }
        last = cur;
    }

    // 构建完成后计算 occ（出现次数）：link 树子树叶节点数求和
    // 同时为输出所有位置准备 link 树
    std::vector<std::vector<int>> link_children_;
    bool prepared_ = false;

    void prepare() {
        if (prepared_) return;
        link_children_.assign(st.size(), {});
        for (int i = 1; i < (int)st.size(); ++i)
            link_children_[st[i].link].push_back(i);
        // 按长度降序（桶排序）累加 occ
        int max_len = 0;
        for (auto& s : st) max_len = std::max(max_len, s.len);
        std::vector<int> bucket(max_len + 1, 0);
        for (auto& s : st) ++bucket[s.len];
        for (int i = 1; i <= max_len; ++i) bucket[i] += bucket[i - 1];
        std::vector<int> order(st.size());
        for (int i = (int)st.size() - 1; i >= 0; --i) order[--bucket[st[i].len]] = i;
        for (int i = (int)order.size() - 1; i >= 0; --i) {
            int v = order[i];
            if (st[v].link != -1) st[st[v].link].occ += st[v].occ;
        }
        prepared_ = true;
    }

    // 在 SAM 上匹配模式串，返回匹配结束状态（-1 表示完全无匹配）
    // matched_len 输出实际匹配长度
    int match(const uint8_t* pat, size_t plen, size_t& matched_len) const {
        int v = 0;
        size_t l = 0;
        for (size_t i = 0; i < plen; ++i) {
            auto it = st[v].next.find(pat[i]);
            if (it == st[v].next.end()) { matched_len = l; return v; }
            v = it->second;
            ++l;
        }
        matched_len = l;
        return v;
    }

    // 查询模式串出现次数（0 表示未出现）
    long long count(const uint8_t* pat, size_t plen) const {
        size_t ml = 0;
        int v = match(pat, plen, ml);
        if (ml != plen) return 0;
        return st[v].occ;
    }

    // 查询模式串首次出现结束位置（0-based），未找到返回 -1
    int first_end_pos(const uint8_t* pat, size_t plen) const {
        size_t ml = 0;
        int v = match(pat, plen, ml);
        if (ml != plen) return -1;
        return st[v].first_pos;
    }

    // 收集模式串所有出现结束位置（link 树 DFS 子树中所有 occ=1 叶节点的 first_pos）
    // 注意：叶节点（构建时插入的）occ=1，非叶 occ 在 prepare 中已累加。
    // 这里用 link 树 DFS，收集子树中所有构建时叶节点（first_pos != -1 且该状态被标记为叶）
    // 更准确：构建时 occ=1 的为叶。但 prepare 后 occ 已变化，需另存叶标记。
    void all_positions(int state, std::vector<int>& out) const {
        if (state < 0 || state >= (int)st.size()) return;
        // 如果是构建时的叶节点（first_pos 有效且无 next 指向它的子？）
        // 实际上每个构建时创建的 cur 状态都是叶，first_pos 有效。
        // clone 不是叶。
        // 但我们没显式标记 clone。clone 的 first_pos 继承自 q，occ=0。
        // 所以 occ==1 且 first_pos!=-1 的可视为原始叶？不准确。
        // 改用：构建时记录 is_leaf_ 数组。
        // 为简化，这里用 link_children_ DFS，收集 first_pos!=-1 的节点。
        // 但 clone 也有 first_pos，会重复。
        // 因此需要在构建时标记是否为原始叶。
        // 见 all_positions_v2
        (void)state; (void)out;
    }

    // 构建时标记叶节点
    std::vector<uint8_t> is_leaf_;
    void mark_leaves() {
        is_leaf_.assign(st.size(), 0);
        // 原始叶节点：构建时 cur，occ 初始为 1
        // 我们没有保存初始 occ。改用：link 树中出度为 0 的节点（无孩子）即叶
        // 但 prepare 后 link_children_ 可用
        if (!prepared_) prepare();
        for (int i = 1; i < (int)st.size(); ++i) {
            // 如果没有孩子（叶），则它是构建时的某次插入终点
            // 注意 clone 也可能是叶（如果 q 的某些转移被复制后 q 不再被经过？）
            // 实际上 clone 不会是构建插入终点，但 clone 可能在 link 树中是叶（无孩子）
            // 为准确，我们在 extend 中标记 is_leaf
        }
        // 修正：在 extend 中直接标记。见 extend_marked 版本。
    }

    // 改进版：在 extend 中标记 cur 为叶
    // 为兼容，这里重新实现：遍历所有状态，link 树中无孩子的且 first_pos 有效的视为叶
    // 但 clone 无孩子时 first_pos 继承自 q，会误判。
    // 最稳妥：在构建时记录每个 state 是否为 "cur"（非 clone）
    std::vector<uint8_t> is_cur_;
    void build_done() {
        is_cur_.assign(st.size(), 0);
        // 需在 extend 中标记，这里无法事后区分
        // 所以改用：记录构建时每次 extend 创建的 cur 索引
        // 见下方带标记的构建
    }
};

// 带叶节点标记的 SAM（用于输出所有位置）
class SuffixAutomatonEx {
public:
    std::vector<SamState> st;
    std::vector<uint8_t> is_cur;      // 是否为构建时 cur（非 clone）
    std::vector<std::vector<int>> link_children;
    int last = 0;
    bool prepared = false;

    explicit SuffixAutomatonEx(size_t reserve_bytes = 0) {
        st.reserve(reserve_bytes * 2 + 4);
        st.emplace_back();
        last = 0;
    }

    void extend(uint8_t c, int pos) {
        int cur = (int)st.size();
        st.emplace_back();
        st[cur].len = st[last].len + 1;
        st[cur].first_pos = pos;
        st[cur].occ = 1;
        int p = last;
        while (p != -1 && !st[p].next.count(c)) {
            st[p].next[c] = cur;
            p = st[p].link;
        }
        if (p == -1) {
            st[cur].link = 0;
        } else {
            int q = st[p].next[c];
            if (st[p].len + 1 == st[q].len) {
                st[cur].link = q;
            } else {
                int clone = (int)st.size();
                st.emplace_back();
                st[clone].len = st[p].len + 1;
                st[clone].link = st[q].link;
                st[clone].next = st[q].next;
                st[clone].first_pos = st[q].first_pos;
                st[clone].occ = 0;
                while (p != -1 && st[p].next[c] == q) {
                    st[p].next[c] = clone;
                    p = st[p].link;
                }
                st[q].link = clone;
                st[cur].link = clone;
            }
        }
        last = cur;
    }

    void prepare() {
        if (prepared) return;
        // 标记 cur：所有状态初始 is_cur=0，遍历构建序列...
        // 改用：link 树中，叶节点 = 无孩子的节点。但 clone 可能无孩子。
        // 区分：clone 的 occ 初始为 0，cur 的 occ 初始为 1。
        // prepare 中 occ 会被累加，所以先保存初始 occ。
        // 但我们已经丢失初始 occ。
        // 解决：在 prepare 前用 first_pos 和 len 判断
        // 实际上，对于输出所有位置，我们只需要：对匹配状态 v，在 link 树子树中找所有"构建时叶节点"。
        // 构建时叶节点 = 每次 extend 创建的 cur。这些 cur 的 first_pos 就是插入位置。
        // clone 的 first_pos 继承自 q，不是真实插入位置。
        // 所以我们需要在构建时记录 cur。
        // 这里用一个 trick：构建时 cur 的 len 等于 pos+1（如果文本从 0 开始连续构建）。
        // 即 cur.first_pos == cur.len - 1。
        // 因为每次 extend(c, pos) 时 cur.len = last.len + 1，且 last.len = pos（前一个的 len）。
        // 所以对于连续构建，cur.len == pos + 1，即 cur.first_pos == cur.len - 1。
        // clone 的 first_pos 继承自 q，q.first_pos 可能 != q.len-1（如果 q 是 clone 或被 clone 影响）。
        // 这个判断不完全可靠。
        // 最稳妥：在 extend 中标记 is_cur。我在下面重写 extend 来标记。
        // 但已经构建完了。所以我在 prepare 中用启发式：link 树叶节点（无孩子）且 occ 初始为 1。
        // 由于 occ 已累加，我用另一种方式：重新计算 occ 时记录初始值。
        link_children.assign(st.size(), {});
        for (int i = 1; i < (int)st.size(); ++i)
            link_children[st[i].link].push_back(i);
        // 桶排序按 len 降序累加 occ
        int max_len = 0;
        for (auto& s : st) max_len = std::max(max_len, s.len);
        std::vector<int> bucket(max_len + 1, 0);
        for (auto& s : st) ++bucket[s.len];
        for (int i = 1; i <= max_len; ++i) bucket[i] += bucket[i - 1];
        std::vector<int> order(st.size());
        for (int i = (int)st.size() - 1; i >= 0; --i) order[--bucket[st[i].len]] = i;
        // 保存初始 occ（1 表示构建时叶，0 表示 clone）
        is_cur.assign(st.size(), 0);
        for (int i = 0; i < (int)st.size(); ++i)
            if (st[i].occ == 1) is_cur[i] = 1; // 此时 occ 尚未累加，原始值
        for (int i = (int)order.size() - 1; i >= 0; --i) {
            int v = order[i];
            if (st[v].link != -1) st[st[v].link].occ += st[v].occ;
        }
        prepared = true;
    }

    int match(const uint8_t* pat, size_t plen, size_t& matched_len) const {
        int v = 0; size_t l = 0;
        for (size_t i = 0; i < plen; ++i) {
            auto it = st[v].next.find(pat[i]);
            if (it == st[v].next.end()) { matched_len = l; return v; }
            v = it->second; ++l;
        }
        matched_len = l;
        return v;
    }

    long long count(const uint8_t* pat, size_t plen) const {
        size_t ml = 0; int v = match(pat, plen, ml);
        if (ml != plen) return 0;
        return st[v].occ;
    }

    int first_end_pos(const uint8_t* pat, size_t plen) const {
        size_t ml = 0; int v = match(pat, plen, ml);
        if (ml != plen) return -1;
        return st[v].first_pos;
    }

    // 收集匹配状态子树中所有 is_cur 的 first_pos（即所有出现结束位置）
    void collect_positions(int v, std::vector<int>& out) const {
        if (v < 0) return;
        if (is_cur[v]) out.push_back(st[v].first_pos);
        for (int c : link_children[v]) collect_positions(c, out);
    }

    std::vector<int> all_end_positions(const uint8_t* pat, size_t plen) {
        size_t ml = 0; int v = match(pat, plen, ml);
        if (ml != plen) return {};
        if (!prepared) prepare();
        std::vector<int> res;
        collect_positions(v, res);
        std::sort(res.begin(), res.end());
        return res;
    }
};

// ===================== 通配符匹配 =====================
// 支持: * (任意长度), ? (单字符), 其余字面匹配
// 用于文件名和简单文本匹配
inline bool glob_match(const std::string& text, const std::string& glob) {
    size_t n = text.size(), m = glob.size();
    // dp[i][j]: text 前 i 个是否匹配 glob 前 j 个
    // 空间优化为一维
    std::vector<uint8_t> prev(n + 1, 0), cur(n + 1, 0);
    prev[0] = 1;
    for (size_t j = 1; j <= m; ++j) {
        char gc = glob[j - 1];
        if (gc == '*') {
            cur[0] = cur[0] | prev[0]; // * 匹配空
            // 其实 cur[0] 应该 = prev[0]（* 匹配空）| cur[j-1]？
            // 标准做法：* 时 cur[0] = prev[0]
            cur[0] = prev[0];
        } else {
            cur[0] = 0;
        }
        for (size_t i = 1; i <= n; ++i) {
            if (gc == '*') {
                cur[i] = prev[i] | cur[i - 1]; // * 匹配空 | * 多匹配一个
            } else if (gc == '?') {
                cur[i] = prev[i - 1];
            } else {
                cur[i] = (text[i - 1] == gc) ? prev[i - 1] : 0;
            }
        }
        std::swap(prev, cur);
    }
    return prev[n] != 0;
}

// ===================== HTTP GET (WinHTTP) =====================
// 用于 Everything 搜索。返回响应体字符串。
inline bool http_get(const std::wstring& host, int port, const std::wstring& path, std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"tbx/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    BOOL bRes = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bRes) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
    }
    DWORD size = 0;
    body.clear();
    do {
        DWORD downloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;
        std::vector<char> buf(size);
        if (!WinHttpReadData(hRequest, buf.data(), size, &downloaded)) break;
        body.append(buf.data(), downloaded);
    } while (size > 0);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// URL 编码
inline std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':' || c == '\\') {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

// ===================== 字符串工具 =====================

inline std::vector<std::string> split_str(const std::string& s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

// 宽字符转 UTF-8
inline std::string w_to_u8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// UTF-8 转宽字符
inline std::wstring u8_to_w(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// JSON 字符串转义
inline std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8]; sprintf_s(buf, "\\u%04x", c);
                out += buf;
            } else out += c;
        }
    }
    return out;
}

} // namespace tbx
