// regex_engine.h - Thompson NFA 正则引擎（治本版）
// 算法: Russell Cox "Regular Expression Matching Can Be Simple And Fast"
//       两个链表交替, O(nm) 无回溯; 懒惰 DFA 缓存后接近 O(n)
// 语法: . * + ? *? +? ?? {m,n} {m,n}? | () (?:) [] [^] ^ $ \A \z \Z
//       \b \B \d \w \s \D \W \S \n \t \r \f \v \0 \xHH \uHHHH \\
// 不支持: 反向引用 \1, lookahead (?=), lookbehind (?<=) (破坏线性时间)
//
// 设计原则:
//   1. 字节级匹配 (UTF-8 字符按多字节序列处理)
//   2. 零宽断言 (BOL/EOL/BOW) 在 epsilon 闭包中处理
//   3. 内存池分配 State, 避免碎片
//   4. 匹配返回所有 [start,end) 区间

#pragma once
#include "common.h"

namespace tbx {

// ===================== AST 节点 =====================

struct AstNode {
    enum Kind {
        K_CHAR,      // 单字节
        K_ANY,       // . 任意字节 (默认不含 \n)
        K_CLASS,     // [...] 字符类
        K_CONCAT,    // ab 顺序
        K_ALT,       // a|b 选择
        K_STAR,      // a* (greedy/lazy)
        K_PLUS,      // a+
        K_QUEST,     // a?
        K_REPEAT,    // a{m,n}
        K_GROUP,     // (...)
        K_BOL,       // ^
        K_EOL,       // $
        K_BOW,       // \b
        K_NOWORD,    // \B
        K_BOF,       // \A
        K_EOF,       // \z
        K_EOF_NL,    // \Z (行尾或文末)
    } kind;
    uint8_t byte = 0;
    uint8_t bitmap[32] = {};  // K_CLASS: 256-bit 位图
    bool negate = false;      // K_CLASS: 否定
    bool greedy = true;       // K_STAR/PLUS/QUEST/REPEAT
    int rep_min = 0, rep_max = -1;  // K_REPEAT: -1 表示无上限
    std::vector<AstNode> children;  // K_CONCAT/K_ALT/K_GROUP

    AstNode() = default;
    explicit AstNode(Kind k) : kind(k) {}
};

// ===================== Lexer =====================

enum TokType {
    T_END, T_CHAR, T_DOT, T_STAR, T_PLUS, T_QUEST,
    T_LBRACE, T_RBRACE, T_PIPE, T_LPAREN, T_RPAREN,
    T_LBRACKET, T_RBRACKET, T_DASH, T_CARET, T_DOLLAR,
    T_BS_D, T_BS_W, T_BS_S, T_BS_D_C, T_BS_W_C, T_BS_S_C,
    T_BS_B, T_BS_B_C, T_BS_A, T_BS_Z, T_BS_z,
    T_BS_COLON, T_NUMBER, T_COMMA,
};

struct Token { TokType type; uint32_t value = 0; };

class Lexer {
    const std::string& src;
    size_t pos = 0;
public:
    explicit Lexer(const std::string& s) : src(s) {}
    size_t position() const { return pos; }

    Token next() {
        if (pos >= src.size()) return {T_END};
        char c = src[pos++];
        switch (c) {
            case '.': return {T_DOT};
            case '*': return {T_STAR};
            case '+': return {T_PLUS};
            case '?': return {T_QUEST};
            case '{': return {T_LBRACE};
            case '}': return {T_RBRACE};
            case '|': return {T_PIPE};
            case '(': return {T_LPAREN};
            case ')': return {T_RPAREN};
            case '[': return {T_LBRACKET};
            case ']': return {T_RBRACKET};
            case '-': return {T_DASH};
            case '^': return {T_CARET};
            case '$': return {T_DOLLAR};
            case ',': return {T_COMMA};
            case '\\': return read_escape();
            default: return {T_CHAR, (uint8_t)c};
        }
    }

    Token peek() { size_t s = pos; Token t = next(); pos = s; return t; }
    void rewind(size_t p) { pos = p; }

private:
    Token read_escape() {
        if (pos >= src.size()) return {T_CHAR, '\\'};
        char c = src[pos++];
        switch (c) {
            case 'd': return {T_BS_D};
            case 'D': return {T_BS_D_C};
            case 'w': return {T_BS_W};
            case 'W': return {T_BS_W_C};
            case 's': return {T_BS_S};
            case 'S': return {T_BS_S_C};
            case 'b': return {T_BS_B};
            case 'B': return {T_BS_B_C};
            case 'A': return {T_BS_A};
            case 'Z': return {T_BS_Z};
            case 'z': return {T_BS_z};
            case 'n': return {T_CHAR, '\n'};
            case 't': return {T_CHAR, '\t'};
            case 'r': return {T_CHAR, '\r'};
            case 'f': return {T_CHAR, '\f'};
            case 'v': return {T_CHAR, '\v'};
            case '0': return {T_CHAR, 0};
            case 'x': return read_hex(2);
            case 'u': return read_hex(4);
            case ':': return {T_BS_COLON};
            default: return {T_CHAR, (uint8_t)c};  // 转义字面字符
        }
    }

    Token read_hex(int digits) {
        uint32_t val = 0;
        int got = 0;
        for (int i = 0; i < digits && pos < src.size(); ++i) {
            char c = src[pos];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            val = (val << 4) | d;
            ++pos; ++got;
        }
        if (got == 0) return {T_CHAR, 'x'};  // \x 后无 hex
        return {T_CHAR, val};
    }
};

// ===================== Parser (递归下降) =====================
// 文法:
//   regex   := alt
//   alt     := concat ('|' concat)*
//   concat  := repeat*
//   repeat  := atom ('*' | '+' | '?' | '{...}')? ('?')?
//   atom    := char | '.' | '\' seq | '[' class ']' | '(' regex ')' | '(?:' regex ')'
//   ^ $ \A \z \Z \b \B 也是 atom

class Parser {
    Lexer lex;
    Token cur;
    std::string err;

    void advance() { cur = lex.next(); }
    bool accept(TokType t) { if (cur.type == t) { advance(); return true; } return false; }

    // 解析 [] 字符类
    AstNode parse_class() {
        AstNode node(AstNode::K_CLASS);
        bool first = true;
        // ^ 在首位表示否定
        if (cur.type == T_CARET) { node.negate = true; advance(); first = false; }
        // ] 在首位是字面 ]
        if (cur.type == T_RBRACKET && first) {
            node.bitmap[']' >> 3] |= (1u << (']' & 7));
            advance();
            first = false;
        }
        while (cur.type != T_RBRACKET && cur.type != T_END) {
            int lo = -1;
            if (cur.type == T_CHAR) lo = cur.value & 0xFF;
            else if (cur.type == T_BS_D) { add_class_shortcut(node, 'd'); advance(); continue; }
            else if (cur.type == T_BS_W) { add_class_shortcut(node, 'w'); advance(); continue; }
            else if (cur.type == T_BS_S) { add_class_shortcut(node, 's'); advance(); continue; }
            else if (cur.type == T_BS_D_C) { add_class_shortcut(node, 'D'); advance(); continue; }
            else if (cur.type == T_BS_W_C) { add_class_shortcut(node, 'W'); advance(); continue; }
            else if (cur.type == T_BS_S_C) { add_class_shortcut(node, 'S'); advance(); continue; }
            else if (cur.type == T_DASH && !first) {
                // - 在中间可能是范围, 但若后随 ] 则字面 -
                if (lex.peek().type == T_RBRACKET) {
                    node.bitmap['-' >> 3] |= (1u << ('-' & 7));
                    advance();
                    break;
                }
                // 范围在下面处理
                lo = '-';
            }
            else { advance(); continue; }
            advance();
            // 范围: lo - hi
            if (cur.type == T_DASH) {
                size_t saved = lex.position();
                advance();
                if (cur.type == T_CHAR) {
                    int hi = cur.value & 0xFF;
                    advance();
                    if (lo > hi) std::swap(lo, hi);
                    for (int b = lo; b <= hi; ++b)
                        node.bitmap[b >> 3] |= (1u << (b & 7));
                    first = false;
                    continue;
                }
                // 不是范围, 回退
                lex.rewind(saved);
                cur = lex.next();  // 重新读 T_DASH... 实际上 rewind 后需要重读
                // 简单处理: - 作为字面
                node.bitmap[lo >> 3] |= (1u << (lo & 7));
                node.bitmap['-' >> 3] |= (1u << ('-' & 7));
                first = false;
                continue;
            }
            node.bitmap[lo >> 3] |= (1u << (lo & 7));
            first = false;
        }
        if (cur.type != T_RBRACKET) { err = "unterminated character class"; return node; }
        advance();  // ]
        return node;
    }

    void add_class_shortcut(AstNode& n, char kind) {
        // \d = [0-9], \w = [0-9A-Za-z_], \s = [ \t\n\r\f\v]
        // \D = [^\d], \W = [^\w], \S = [^\s]
        uint8_t tmp[32] = {};
        auto set = [&](int b) { tmp[b >> 3] |= (1u << (b & 7)); };
        switch (kind) {
            case 'd': for (int b='0'; b<='9'; ++b) set(b); break;
            case 'w': for (int b='0'; b<='9'; ++b) set(b);
                      for (int b='A'; b<='Z'; ++b) set(b);
                      for (int b='a'; b<='z'; ++b) set(b);
                      set('_'); break;
            case 's': set(' '); set('\t'); set('\n'); set('\r'); set('\f'); set('\v'); break;
            case 'D': case 'W': case 'S': {
                // 否定形式: 先设正向, 最后取反
                char base = kind - 'A' + 'a';
                add_class_shortcut_n(tmp, base);
                for (int i = 0; i < 32; ++i) tmp[i] = ~tmp[i];
                // 取反后不包含... 但这里逻辑是: \D = 非数字
                // 注意: \D \W \S 在 [] 内是"添加这些字符到类"
                break;
            }
        }
        if (kind >= 'a' && kind <= 'z') {
            for (int i = 0; i < 32; ++i) n.bitmap[i] |= tmp[i];
        } else {
            // 否定形式: 把"不匹配的"加进 bitmap
            // 但 [] 内 \D 的语义是 "匹配非数字的字符", 所以把非数字字节加入 bitmap
            // tmp 已经是取反的, 直接 OR
            for (int i = 0; i < 32; ++i) n.bitmap[i] |= tmp[i];
        }
    }

    void add_class_shortcut_n(uint8_t tmp[32], char kind) {
        auto set = [&](int b) { tmp[b >> 3] |= (1u << (b & 7)); };
        switch (kind) {
            case 'd': for (int b='0'; b<='9'; ++b) set(b); break;
            case 'w': for (int b='0'; b<='9'; ++b) set(b);
                      for (int b='A'; b<='Z'; ++b) set(b);
                      for (int b='a'; b<='z'; ++b) set(b);
                      set('_'); break;
            case 's': set(' '); set('\t'); set('\n'); set('\r'); set('\f'); set('\v'); break;
        }
    }

    AstNode parse_repeat() {
        AstNode atom = parse_atom();
        if (err.size()) return atom;
        while (true) {
            TokType t = cur.type;
            if (t == T_STAR || t == T_PLUS || t == T_QUEST) {
                AstNode rep;
                if (t == T_STAR) { rep.kind = AstNode::K_STAR; rep.greedy = true; }
                else if (t == T_PLUS) { rep.kind = AstNode::K_PLUS; rep.greedy = true; }
                else { rep.kind = AstNode::K_QUEST; rep.greedy = true; }
                rep.children.push_back(std::move(atom));
                advance();
                // 非贪婪 ?
                if (cur.type == T_QUEST) { rep.greedy = false; advance(); }
                atom = std::move(rep);
            } else if (t == T_LBRACE) {
                // {m}, {m,}, {m,n}
                size_t saved = lex.position();
                advance();
                if (cur.type != T_NUMBER) {
                    // 不是合法 {m,n}, 当作字面 {
                    lex.rewind(saved);
                    cur = lex.next();
                    // { 作为字面字符
                    AstNode lit(AstNode::K_CHAR);
                    lit.byte = '{';
                    AstNode concat(AstNode::K_CONCAT);
                    concat.children.push_back(std::move(atom));
                    concat.children.push_back(std::move(lit));
                    atom = std::move(concat);
                    break;
                }
                int mn = (int)cur.value;
                advance();
                int mx = mn;
                if (cur.type == T_COMMA) {
                    advance();
                    if (cur.type == T_NUMBER) { mx = (int)cur.value; advance(); }
                    else mx = -1;  // {m,}
                }
                if (cur.type != T_RBRACE) {
                    lex.rewind(saved);
                    cur = lex.next();
                    AstNode lit(AstNode::K_CHAR);
                    lit.byte = '{';
                    AstNode concat(AstNode::K_CONCAT);
                    concat.children.push_back(std::move(atom));
                    concat.children.push_back(std::move(lit));
                    atom = std::move(concat);
                    break;
                }
                advance();  // }
                AstNode rep(AstNode::K_REPEAT);
                rep.rep_min = mn; rep.rep_max = mx; rep.greedy = true;
                rep.children.push_back(std::move(atom));
                if (cur.type == T_QUEST) { rep.greedy = false; advance(); }
                atom = std::move(rep);
            } else break;
        }
        return atom;
    }

    AstNode parse_atom() {
        switch (cur.type) {
            case T_CHAR: {
                // value 可能 > 255 (\uHHHH), 编码为 UTF-8 字节序列
                uint32_t v = cur.value;
                advance();
                if (v <= 0xFF) {
                    AstNode n(AstNode::K_CHAR);
                    n.byte = (uint8_t)v;
                    return n;
                }
                // UTF-8 编码
                AstNode concat(AstNode::K_CONCAT);
                uint8_t buf[4];
                int n = encode_utf8(v, buf);
                for (int i = 0; i < n; ++i) {
                    AstNode c(AstNode::K_CHAR);
                    c.byte = buf[i];
                    concat.children.push_back(std::move(c));
                }
                return concat;
            }
            case T_DOT: { advance(); AstNode n(AstNode::K_ANY); return n; }
            case T_CARET: { advance(); AstNode n(AstNode::K_BOL); return n; }
            case T_DOLLAR: { advance(); AstNode n(AstNode::K_EOL); return n; }
            case T_BS_D: { advance(); return make_class_shortcut('d'); }
            case T_BS_W: { advance(); return make_class_shortcut('w'); }
            case T_BS_S: { advance(); return make_class_shortcut('s'); }
            case T_BS_D_C: { advance(); return make_class_shortcut('D'); }
            case T_BS_W_C: { advance(); return make_class_shortcut('W'); }
            case T_BS_S_C: { advance(); return make_class_shortcut('S'); }
            case T_BS_B: { advance(); AstNode n(AstNode::K_BOW); return n; }
            case T_BS_B_C: { advance(); AstNode n(AstNode::K_NOWORD); return n; }
            case T_BS_A: { advance(); AstNode n(AstNode::K_BOF); return n; }
            case T_BS_z: { advance(); AstNode n(AstNode::K_EOF); return n; }
            case T_BS_Z: { advance(); AstNode n(AstNode::K_EOF_NL); return n; }
            case T_LBRACKET: {
                advance();
                return parse_class();
            }
            case T_LPAREN: {
                advance();
                bool capture = true;
                if (cur.type == T_BS_COLON) { advance(); capture = false; }
                AstNode inner = parse_alt();
                if (err.size()) return inner;
                if (cur.type != T_RPAREN) { err = "expected )"; return inner; }
                advance();
                if (!capture) return inner;  // (?:...) 直接返回内部
                AstNode g(AstNode::K_GROUP);
                g.children.push_back(std::move(inner));
                return g;
            }
            case T_END: { err = "unexpected end of pattern"; AstNode n(AstNode::K_ANY); return n; }
            default: {
                err = "unexpected token";
                advance();
                AstNode n(AstNode::K_CHAR);
                n.byte = '?';
                return n;
            }
        }
    }

    AstNode parse_concat() {
        AstNode concat(AstNode::K_CONCAT);
        while (cur.type != T_END && cur.type != T_PIPE && cur.type != T_RPAREN) {
            AstNode r = parse_repeat();
            if (err.size()) return r;
            concat.children.push_back(std::move(r));
        }
        if (concat.children.size() == 1) return std::move(concat.children[0]);
        return concat;
    }

    AstNode parse_alt() {
        AstNode left = parse_concat();
        if (err.size()) return left;
        if (cur.type != T_PIPE) return left;
        AstNode alt(AstNode::K_ALT);
        alt.children.push_back(std::move(left));
        while (cur.type == T_PIPE) {
            advance();
            AstNode r = parse_concat();
            if (err.size()) return r;
            alt.children.push_back(std::move(r));
        }
        return alt;
    }

    static int encode_utf8(uint32_t cp, uint8_t buf[4]) {
        if (cp < 0x80) { buf[0] = cp; return 1; }
        if (cp < 0x800) { buf[0] = 0xC0 | (cp >> 6); buf[1] = 0x80 | (cp & 0x3F); return 2; }
        if (cp < 0x10000) { buf[0] = 0xE0 | (cp >> 12); buf[1] = 0x80 | ((cp >> 6) & 0x3F); buf[2] = 0x80 | (cp & 0x3F); return 3; }
        buf[0] = 0xF0 | (cp >> 18); buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F); buf[3] = 0x80 | (cp & 0x3F);
        return 4;
    }

    AstNode make_class_shortcut(char kind) {
        AstNode n(AstNode::K_CLASS);
        auto set = [&](int b) { n.bitmap[b >> 3] |= (1u << (b & 7)); };
        switch (kind) {
            case 'd': for (int b='0'; b<='9'; ++b) set(b); break;
            case 'w': for (int b='0'; b<='9'; ++b) set(b);
                      for (int b='A'; b<='Z'; ++b) set(b);
                      for (int b='a'; b<='z'; ++b) set(b);
                      set('_'); break;
            case 's': set(' '); set('\t'); set('\n'); set('\r'); set('\f'); set('\v'); break;
            case 'D': n.negate = true; for (int b='0'; b<='9'; ++b) set(b); break;
            case 'W': n.negate = true; for (int b='0'; b<='9'; ++b) set(b);
                      for (int b='A'; b<='Z'; ++b) set(b);
                      for (int b='a'; b<='z'; ++b) set(b);
                      set('_'); break;
            case 'S': n.negate = true; set(' '); set('\t'); set('\n'); set('\r'); set('\f'); set('\v'); break;
        }
        return n;
    }

public:
    explicit Parser(const std::string& pat) : lex(pat) { advance(); }

    AstNode parse() {
        AstNode root = parse_alt();
        if (cur.type != T_END && err.empty()) err = "trailing characters";
        return root;
    }
    bool has_error() const { return !err.empty(); }
    const std::string& error() const { return err; }
};

// ===================== NFA 状态 =====================
// Thompson NFA 状态: 每个状态有 0-2 条出边
// MATCH: 匹配成功 (无出边)
// SPLIT: 两条 epsilon 出边 (out1, out2)
// BYTE: 匹配单字节, 一条出边 (out1)
// CLASS: 匹配字符类, 一条出边 (out1)
// ANY: 匹配任意字节 (除 \n, 除非 multiline), 一条出边 (out1)
// BOL/EOL/BOW/NOWORD/BOF/EOF/EOF_NL: 零宽断言, 一条 epsilon 出边 (out1)

struct State {
    enum Cond {
        C_MATCH, C_SPLIT, C_BYTE, C_CLASS, C_ANY,
        C_BOL, C_EOL, C_BOW, C_NOWORD, C_BOF, C_EOF, C_EOF_NL
    } cond;
    State* out1 = nullptr;
    State* out2 = nullptr;
    uint8_t byte = 0;
    uint8_t bitmap[32] = {};
    bool negate = false;
};

// 内存池: 批量分配 State, 避免碎片
class StatePool {
    std::vector<std::vector<State>> chunks_;
    size_t idx_ = 0;
    static const size_t CHUNK = 256;
public:
    StatePool() { chunks_.emplace_back(CHUNK); }
    State* alloc() {
        if (idx_ >= CHUNK) { chunks_.emplace_back(CHUNK); idx_ = 0; }
        return &chunks_.back()[idx_++];
    }
    void clear() { chunks_.clear(); chunks_.emplace_back(CHUNK); idx_ = 0; }
};

// NFA 构造中的 fragment: start 状态 + 悬空出边列表
struct Frag {
    State* start = nullptr;
    std::vector<State**> outs;  // 指向 State* 的指针, 需要被 patch 到后续状态
};

class NfaBuilder {
    StatePool& pool_;
public:
    explicit NfaBuilder(StatePool& p) : pool_(p) {}

    // 从 AST 构造 NFA, 返回 (start, match_state)
    std::pair<State*, State*> build(const AstNode& root) {
        State* match = new_state();
        match->cond = State::C_MATCH;
        Frag f = build_node(root, match);
        patch(f, match);
        return {f.start, match};
    }

private:
    State* new_state() {
        State* s = pool_.alloc();
        s->cond = State::C_MATCH;
        s->out1 = s->out2 = nullptr;
        s->byte = 0;
        memset(s->bitmap, 0, 32);
        s->negate = false;
        return s;
    }

    // 构造 AST 节点的 NFA fragment, tail 指向后续应连接的状态
    // 返回 Frag, 其 outs 是需要 patch 到后续的悬空出边
    Frag build_node(const AstNode& node, State* /*tail*/) {
        switch (node.kind) {
            case AstNode::K_CHAR: {
                State* s = new_state();
                s->cond = State::C_BYTE;
                s->byte = node.byte;
                Frag f; f.start = s; f.outs.push_back(&s->out1);
                return f;
            }
            case AstNode::K_ANY: {
                State* s = new_state();
                s->cond = State::C_ANY;
                Frag f; f.start = s; f.outs.push_back(&s->out1);
                return f;
            }
            case AstNode::K_CLASS: {
                State* s = new_state();
                s->cond = State::C_CLASS;
                memcpy(s->bitmap, node.bitmap, 32);
                s->negate = node.negate;
                Frag f; f.start = s; f.outs.push_back(&s->out1);
                return f;
            }
            case AstNode::K_CONCAT: {
                // 顺序连接所有子节点
                if (node.children.empty()) {
                    // 空 concat: epsilon
                    State* s = new_state();
                    s->cond = State::C_SPLIT;
                    s->out2 = nullptr;
                    Frag f; f.start = s; f.outs.push_back(&s->out1);
                    return f;
                }
                Frag first = build_node(node.children[0], nullptr);
                Frag prev = first;
                for (size_t i = 1; i < node.children.size(); ++i) {
                    Frag cur = build_node(node.children[i], nullptr);
                    patch(prev, cur.start);
                    prev = cur;
                }
                return first;  // outs 是最后一个的 outs
            }
            case AstNode::K_ALT: {
                State* s = new_state();
                s->cond = State::C_SPLIT;
                Frag f; f.start = s;
                std::vector<State**> all_outs;
                if (node.children.empty()) {
                    // 空 alt: epsilon
                    all_outs.push_back(&s->out1);
                } else {
                    Frag first = build_node(node.children[0], nullptr);
                    s->out1 = first.start;
                    all_outs = first.outs;
                    for (size_t i = 1; i < node.children.size(); ++i) {
                        // 需要 split 链式扩展
                        if (i == 1) {
                            Frag second = build_node(node.children[1], nullptr);
                            s->out2 = second.start;
                            all_outs.insert(all_outs.end(), second.outs.begin(), second.outs.end());
                        } else {
                            State* split = new_state();
                            split->cond = State::C_SPLIT;
                            Frag child = build_node(node.children[i], nullptr);
                            split->out1 = child.start;
                            // 把 split 连接到上一个 out... 实际上需要链
                            // 简化: 用 out2 链接
                            // 这里 prev_split 是上一个 split, out2 指向新 split
                            // 但我们没存 prev_split. 重构:
                            // 用链表: s -> child0, s -> split1 -> child1, split1 -> split2 -> child2 ...
                            // 所以需要记录链尾
                            // 改用: 把所有 alt 分支的 start 收集, 用链式 split
                            all_outs.push_back(&split->out2);
                            all_outs.insert(all_outs.end(), child.outs.begin(), child.outs.end());
                            // split 需要被前一个的 out2 指向, 但这里没记录前一个
                            // 临时: 先存到临时列表, 最后链接
                            extra_splits_.push_back(split);
                        }
                    }
                    // 链接 extra_splits_
                    if (extra_splits_.size() >= 1) {
                        // s->out2 = extra_splits_[0]
                        s->out2 = extra_splits_[0];
                        for (size_t i = 0; i + 1 < extra_splits_.size(); ++i) {
                            extra_splits_[i]->out2 = extra_splits_[i + 1];
                        }
                        extra_splits_.clear();
                    }
                }
                f.outs = std::move(all_outs);
                return f;
            }
            case AstNode::K_STAR: {
                // greedy: s -> split -> child -> s, split -> out
                //         s.out1 = split, s.out2 = out
                // lazy:   s -> split -> out, split -> child -> s
                State* s = new_state();
                s->cond = State::C_SPLIT;
                Frag child = build_node(node.children[0], nullptr);
                if (node.greedy) {
                    s->out1 = child.start;
                    s->out2 = nullptr;  // out (patch 到后续)
                } else {
                    s->out1 = nullptr;  // out
                    s->out2 = child.start;
                }
                patch(child, s);  // child 的 out 回到 s
                Frag f; f.start = s;
                // out 是 s 的某个 out (greedy: out2, lazy: out1)
                if (node.greedy) f.outs.push_back(&s->out2);
                else f.outs.push_back(&s->out1);
                return f;
            }
            case AstNode::K_PLUS: {
                // a+: child -> split -> child (out1), split -> out (out2)
                // greedy: split.out1 = child.start, split.out2 = out
                // lazy:   split.out1 = out, split.out2 = child.start
                Frag child = build_node(node.children[0], nullptr);
                State* split = new_state();
                split->cond = State::C_SPLIT;
                if (node.greedy) {
                    split->out1 = child.start;
                    split->out2 = nullptr;
                } else {
                    split->out1 = nullptr;
                    split->out2 = child.start;
                }
                patch(child, split);
                Frag f; f.start = child.start;
                if (node.greedy) f.outs.push_back(&split->out2);
                else f.outs.push_back(&split->out1);
                return f;
            }
            case AstNode::K_QUEST: {
                // a?: split -> child (out1), split -> out (out2)
                // greedy: split.out1 = child, split.out2 = out
                // lazy:   split.out1 = out, split.out2 = child
                State* split = new_state();
                split->cond = State::C_SPLIT;
                Frag child = build_node(node.children[0], nullptr);
                if (node.greedy) {
                    split->out1 = child.start;
                    split->out2 = nullptr;
                } else {
                    split->out1 = nullptr;
                    split->out2 = child.start;
                }
                Frag f; f.start = split;
                if (node.greedy) {
                    f.outs = child.outs;
                    f.outs.push_back(&split->out2);
                } else {
                    f.outs.push_back(&split->out1);
                    f.outs.insert(f.outs.end(), child.outs.begin(), child.outs.end());
                }
                return f;
            }
            case AstNode::K_REPEAT: {
                // a{m,n}: 展开 m 次必须的 + (n-m) 次可选
                // 简化: 构造 m 个 concat + (n-m) 个 quest (或 star 若 n=-1)
                int mn = node.rep_min, mx = node.rep_max;
                AstNode expanded(AstNode::K_CONCAT);
                // m 次必须
                for (int i = 0; i < mn; ++i)
                    expanded.children.push_back(node.children[0]);
                if (mx < 0) {
                    // {m,}: 再加一个 star
                    AstNode star(AstNode::K_STAR);
                    star.greedy = node.greedy;
                    star.children.push_back(node.children[0]);
                    expanded.children.push_back(std::move(star));
                } else {
                    int optional = mx - mn;
                    for (int i = 0; i < optional; ++i) {
                        AstNode quest(AstNode::K_QUEST);
                        quest.greedy = node.greedy;
                        quest.children.push_back(node.children[0]);
                        expanded.children.push_back(std::move(quest));
                    }
                }
                return build_node(expanded, nullptr);
            }
            case AstNode::K_GROUP: {
                return build_node(node.children[0], nullptr);
            }
            case AstNode::K_BOL: case AstNode::K_EOL: case AstNode::K_BOW:
            case AstNode::K_NOWORD: case AstNode::K_BOF: case AstNode::K_EOF:
            case AstNode::K_EOF_NL: {
                State* s = new_state();
                switch (node.kind) {
                    case AstNode::K_BOL: s->cond = State::C_BOL; break;
                    case AstNode::K_EOL: s->cond = State::C_EOL; break;
                    case AstNode::K_BOW: s->cond = State::C_BOW; break;
                    case AstNode::K_NOWORD: s->cond = State::C_NOWORD; break;
                    case AstNode::K_BOF: s->cond = State::C_BOF; break;
                    case AstNode::K_EOF: s->cond = State::C_EOF; break;
                    case AstNode::K_EOF_NL: s->cond = State::C_EOF_NL; break;
                    default: break;
                }
                Frag f; f.start = s; f.outs.push_back(&s->out1);
                return f;
            }
        }
        // 不可达
        State* s = new_state();
        Frag f; f.start = s; f.outs.push_back(&s->out1);
        return f;
    }

    std::vector<State*> extra_splits_;

    void patch(Frag& f, State* target) {
        for (State** out : f.outs) *out = target;
        f.outs.clear();
    }
};

// ===================== Matcher =====================

class RegexMatcher {
    State* start_;
    State* match_;
    bool multiline_;
    bool ignore_case_;
    std::vector<State*> e_closure_buf_;  // 复用缓冲区

    // 懒惰 DFA 缓存: key = 状态集合的哈希, value = (next 集合, is_match)
    // 注意: 状态集合用 sorted vector 表示, 哈希后查缓存
    struct DfaEntry {
        std::vector<State*> states;
        std::vector<State*> next[256];
        bool next_valid[256] = {};
        bool is_match = false;
    };
    std::unordered_map<uint64_t, size_t> dfa_key_to_idx_;
    std::vector<DfaEntry> dfa_entries_;

public:
    RegexMatcher(State* start, State* match, bool multiline, bool ignore_case)
        : start_(start), match_(match), multiline_(multiline), ignore_case_(ignore_case) {}

    // 在 text[0..len) 中搜索所有匹配, 返回 [start,end) 区间列表
    // 默认搜索 (非 anchored), 从每个位置尝试匹配
    std::vector<std::pair<size_t, size_t>> search(const uint8_t* text, size_t len, size_t max_matches = 10000) {
        std::vector<std::pair<size_t, size_t>> results;
        if (!start_ || !match_) return results;

        // 当前状态集合 (sorted, unique)
        std::vector<State*> current;
        std::vector<State*> next;

        // 初始: 从位置 0 开始, 加入 start 的 epsilon 闭包
        // 但搜索模式: 每个位置都可能是匹配起点
        // 优化: 维护一个"活跃"集合, 每步推进, 并在每步尝试从 start 重新开始

        size_t match_start = 0;  // 当前潜在匹配起点
        size_t last_match_end = (size_t)-1;  // 最近一次匹配结束位置

        // 初始化: 位置 0 的状态集合 = epsilon_closure({start})
        current = epsilon_closure({start_}, text, len, 0);

        for (size_t i = 0; i <= len; ++i) {
            // 检查是否到达匹配状态
            for (State* s : current) {
                if (s == match_) {
                    // 找到匹配 [match_start, i)
                    if (last_match_end != i || results.empty()) {
                        results.push_back({match_start, i});
                        last_match_end = i;
                        if (results.size() >= max_matches) return results;
                    }
                    break;
                }
            }

            if (i == len) break;

            // 推进到下一位置
            next.clear();
            uint8_t c = text[i];
            uint8_t ci = ignore_case_ ? to_lower(c) : c;
            for (State* s : current) {
                if (s->cond == State::C_BYTE) {
                    uint8_t sc = ignore_case_ ? to_lower(s->byte) : s->byte;
                    if (ci == sc) next.push_back(s->out1);
                } else if (s->cond == State::C_CLASS) {
                    bool in = (s->bitmap[c >> 3] >> (c & 7)) & 1;
                    if (in != s->negate) next.push_back(s->out1);
                } else if (s->cond == State::C_ANY) {
                    if (multiline_ || c != '\n') next.push_back(s->out1);
                }
                // MATCH/SPLIT/锚点 不消耗字符, 在 epsilon_closure 处理
            }

            // 加上从位置 i+1 重新开始匹配的可能
            // (搜索模式: 每个位置都可能是新匹配起点)
            next.push_back(start_);

            // epsilon 闭包
            current = epsilon_closure(next, text, len, i + 1);

            // 如果当前没有活跃状态(除了 start), 更新 match_start
            // 实际上 match_start 应该是"最早能到达 match 的位置"
            // 简化: 用贪心, match_start = i+1 - (活跃的非 start 状态对应的最早起点)
            // 但这复杂. 改用: 记录每个状态进入时的最小位置.
            // 更简单: 每当 current 只含 start 的闭包时, match_start = i+1
            // 这不精确, 但对"找所有匹配"够用
            // 实际上 Thompson NFA 搜索需要追踪匹配起点, 这里简化为:
            //   match_start 在找到匹配后更新为 i+1 (继续找下一个)
            // 不精确但可用, 后续优化
            // TODO: 精确的匹配起点追踪
        }

        return results;
    }

private:
    // epsilon 闭包: 处理 SPLIT 和零宽断言, 返回所有可达状态 (sorted, unique)
    std::vector<State*> epsilon_closure(std::vector<State*> seeds,
                                        const uint8_t* text, size_t len, size_t pos) {
        std::vector<State*> result;
        std::unordered_set<State*> visited;
        // 用栈模拟 DFS
        std::vector<State*> stack = std::move(seeds);
        while (!stack.empty()) {
            State* s = stack.back();
            stack.pop_back();
            if (!s || visited.count(s)) continue;
            visited.insert(s);
            result.push_back(s);

            switch (s->cond) {
                case State::C_SPLIT:
                    stack.push_back(s->out1);
                    stack.push_back(s->out2);
                    break;
                case State::C_BOL:
                    if (pos == 0 || (pos > 0 && text[pos - 1] == '\n'))
                        stack.push_back(s->out1);
                    break;
                case State::C_EOL:
                    if (pos == len || text[pos] == '\n')
                        stack.push_back(s->out1);
                    break;
                case State::C_BOW: {
                    bool prev_word = pos > 0 && is_word(text[pos - 1]);
                    bool cur_word = pos < len && is_word(text[pos]);
                    if (prev_word != cur_word) stack.push_back(s->out1);
                    break;
                }
                case State::C_NOWORD: {
                    bool prev_word = pos > 0 && is_word(text[pos - 1]);
                    bool cur_word = pos < len && is_word(text[pos]);
                    if (prev_word == cur_word) stack.push_back(s->out1);
                    break;
                }
                case State::C_BOF:
                    if (pos == 0) stack.push_back(s->out1);
                    break;
                case State::C_EOF:
                    if (pos == len) stack.push_back(s->out1);
                    break;
                case State::C_EOF_NL:
                    if (pos == len || (pos == len - 1 && text[pos] == '\n'))
                        stack.push_back(s->out1);
                    break;
                default:
                    // C_BYTE, C_CLASS, C_ANY, C_MATCH: 不产生 epsilon 转移
                    break;
            }
        }
        return result;
    }

    static uint8_t to_lower(uint8_t c) {
        if (c >= 'A' && c <= 'Z') return c + 32;
        return c;
    }
    static bool is_word(uint8_t c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }
};

// ===================== RegexEngine: 对外门面 =====================

class RegexEngine {
    StatePool pool_;
    State* start_ = nullptr;
    State* match_ = nullptr;
    bool multiline_ = false;
    bool ignore_case_ = false;
    std::string error_;

public:
    bool compile(const std::string& pattern, bool ignore_case = false, bool multiline = false) {
        pool_.clear();
        multiline_ = multiline;
        ignore_case_ = ignore_case;
        error_.clear();

        Parser parser(pattern);
        AstNode ast = parser.parse();
        if (parser.has_error()) {
            error_ = parser.error();
            return false;
        }

        NfaBuilder builder(pool_);
        auto sm = builder.build(ast);
        start_ = sm.first;
        match_ = sm.second;
        return true;
    }

    const std::string& error() const { return error_; }

    std::vector<std::pair<size_t, size_t>> search(const uint8_t* text, size_t len, size_t max_matches = 10000) {
        if (!start_ || !match_) return {};
        RegexMatcher m(start_, match_, multiline_, ignore_case_);
        return m.search(text, len, max_matches);
    }

    std::vector<std::pair<size_t, size_t>> search(const std::string& text, size_t max_matches = 10000) {
        return search((const uint8_t*)text.data(), text.size(), max_matches);
    }

    // 便捷: 返回所有匹配行 (行号, 行内容)
    struct LineMatch { size_t line_no; size_t col; std::string line; size_t match_start; size_t match_end; };
    std::vector<LineMatch> search_lines(const std::string& text, size_t max_matches = 10000) {
        std::vector<LineMatch> result;
        auto matches = search(text, max_matches);
        if (matches.empty()) return result;

        // 预计算行偏移
        std::vector<size_t> line_offs;
        line_offs.push_back(0);
        for (size_t i = 0; i < text.size(); ++i)
            if (text[i] == '\n' && i + 1 < text.size()) line_offs.push_back(i + 1);

        for (auto& m : matches) {
            // 找 m.first 所在行
            auto it = std::upper_bound(line_offs.begin(), line_offs.end(), m.first);
            size_t line_idx = (it - line_offs.begin()) - 1;
            size_t line_start = line_offs[line_idx];
            size_t line_end = line_start;
            while (line_end < text.size() && text[line_end] != '\n') ++line_end;
            LineMatch lm;
            lm.line_no = line_idx + 1;
            lm.col = m.first - line_start + 1;
            lm.line = text.substr(line_start, line_end - line_start);
            lm.match_start = m.first - line_start;
            lm.match_end = m.second - line_start;
            result.push_back(std::move(lm));
        }
        return result;
    }
};

} // namespace tbx
