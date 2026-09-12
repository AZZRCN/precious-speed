// cmd_outline.cpp - C++ 代码大纲解析器
// 提取 namespace/class/struct/union/enum/function/typedef/using/macro/var 等符号
// 支持树形/平铺/JSON 输出，按名称搜索、按行号反向查询
//
// 设计要点：
//   1. 手写词法分析器，跳过注释/字符串/字符/预处理指令（保留行号）
//   2. 简单语法分析，跟踪 {} 嵌套作用域
//   3. 启发式识别函数定义（标识符 + ( + ... + ) + 可选修饰 + {）
//   4. 类/结构体识别（关键字 + 名字 + 可选基类 + {）
//   5. 准确度约 80%，对模板/宏/特殊成员函数可能误判
#include "common.h"
#include <functional>

namespace tbx {

// ============================================================
// 符号类型
// ============================================================
enum SymKind {
    SK_UNKNOWN = 0,
    SK_NAMESPACE,
    SK_CLASS,
    SK_STRUCT,
    SK_UNION,
    SK_ENUM,
    SK_FUNCTION,
    SK_TYPEDEF,
    SK_USING,
    SK_MACRO,
    SK_VAR,
    SK_TEMPLATE,
    SK_FRIEND
};

inline const char* kind_name(SymKind k) {
    switch (k) {
    case SK_NAMESPACE: return "namespace";
    case SK_CLASS:     return "class";
    case SK_STRUCT:    return "struct";
    case SK_UNION:     return "union";
    case SK_ENUM:      return "enum";
    case SK_FUNCTION:  return "function";
    case SK_TYPEDEF:   return "typedef";
    case SK_USING:     return "using";
    case SK_MACRO:     return "macro";
    case SK_VAR:       return "var";
    case SK_TEMPLATE:  return "template";
    case SK_FRIEND:    return "friend";
    default:           return "unknown";
    }
}

inline SymKind kind_from_str(const std::string& s) {
    if (s == "namespace") return SK_NAMESPACE;
    if (s == "class")     return SK_CLASS;
    if (s == "struct")    return SK_STRUCT;
    if (s == "union")     return SK_UNION;
    if (s == "enum")      return SK_ENUM;
    if (s == "function")  return SK_FUNCTION;
    if (s == "typedef")   return SK_TYPEDEF;
    if (s == "using")     return SK_USING;
    if (s == "macro")     return SK_MACRO;
    if (s == "var")       return SK_VAR;
    if (s == "template")  return SK_TEMPLATE;
    if (s == "friend")    return SK_FRIEND;
    return SK_UNKNOWN;
}

// ============================================================
// 符号结构
// ============================================================
struct Symbol {
    SymKind kind = SK_UNKNOWN;
    std::string name;
    std::string signature;     // 函数签名（含参数列表）
    std::string bases;         // 基类列表（class/struct）
    std::string modifiers;     // 修饰符（static/const/virtual/override...）
    std::vector<std::string> template_params;
    int line = 0;              // 起始行号(1-based)
    int end_line = 0;          // 结束行号
    int depth = 0;             // 嵌套深度
    std::vector<Symbol> children;
    bool is_definition = true;
    bool is_ctor = false;
    bool is_dtor = false;
    bool is_operator = false;

    // 按行号查找包含此符号的最深层符号（含自身）
    const Symbol* find_at_line(int ln) const {
        if (line == 0) return nullptr;
        if (end_line > 0 && (ln < line || ln > end_line)) return nullptr;
        if (ln < line) return nullptr;
        // 在子符号中递归查找
        for (auto& c : children) {
            if (c.line > 0 && c.line <= ln) {
                if (auto* r = c.find_at_line(ln)) return r;
            }
        }
        if (ln >= line) return this;
        return nullptr;
    }

    // 收集所有符号（含自身和所有后代）到 flat 列表
    void collect_flat(std::vector<const Symbol*>& out) const {
        out.push_back(this);
        for (auto& c : children) c.collect_flat(out);
    }

    // 按名称搜索（支持通配符），返回所有匹配（含自身和后代）
    void find_by_name(const std::string& pattern, std::vector<const Symbol*>& out) const {
        if (glob_match(name, pattern)) out.push_back(this);
        for (auto& c : children) c.find_by_name(pattern, out);
    }
};

// ============================================================
// 词法分析器
// ============================================================
enum TokKind {
    TK_EOF = 0,
    TK_IDENT,
    TK_PUNCT,
    TK_NUMBER,
    TK_STRING,
    TK_CHAR,
    TK_PP,         // 预处理指令（非 define）
    TK_MACRO_DEF   // #define 定义的宏
};

struct Token {
    TokKind kind = TK_EOF;
    std::string text;
    int line = 0;
    int col = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src), pos_(0), line_(1), col_(1) {}

    Token next() {
        skip_ws_and_comments();
        if (pos_ >= src_.size()) return Token{TK_EOF, "", line_, col_};

        char c = src_[pos_];
        if (c == '#') return read_pp_directive();
        if (c == '"') return read_string();
        if (c == '\'') return read_char();
        if (is_ident_start(c)) return read_ident();
        if (is_digit(c)) return read_number();
        return read_punct();
    }

    int cur_line() const { return line_; }

private:
    const std::string& src_;
    size_t pos_;
    int line_;
    int col_;

    static bool is_ident_start(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80;
    }
    static bool is_ident_char(char c) {
        return is_ident_start(c) || (c >= '0' && c <= '9');
    }
    static bool is_digit(char c) { return c >= '0' && c <= '9'; }

    void advance() {
        if (pos_ < src_.size()) {
            if (src_[pos_] == '\n') { ++line_; col_ = 1; }
            else { ++col_; }
            ++pos_;
        }
    }

    char peek(size_t off = 0) const {
        return (pos_ + off < src_.size()) ? src_[pos_ + off] : '\0';
    }

    void skip_ws_and_comments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                while (pos_ < src_.size() && src_[pos_] != '\n') advance();
            } else if (c == '/' && peek(1) == '*') {
                advance(); advance();
                while (pos_ < src_.size() && !(src_[pos_] == '*' && peek(1) == '/')) advance();
                if (pos_ < src_.size()) { advance(); advance(); }
            } else {
                break;
            }
        }
    }

    Token read_pp_directive() {
        Token t;
        t.kind = TK_PP;
        t.line = line_;
        t.col = col_;
        size_t start = pos_;
        advance(); // '#'
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t')) advance();

        // #define 特殊处理
        if (pos_ + 6 <= src_.size() && src_.compare(pos_, 6, "define") == 0 && !is_ident_char(peek(6))) {
            advance(); advance(); advance(); advance(); advance(); advance(); // "define"
            while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t')) advance();
            std::string macro_name;
            while (pos_ < src_.size() && is_ident_char(src_[pos_])) {
                macro_name += src_[pos_];
                advance();
            }
            // 跳过到行尾（含行连接符）
            while (pos_ < src_.size()) {
                char c = src_[pos_];
                if (c == '\\' && peek(1) == '\n') { advance(); advance(); continue; }
                if (c == '\n') break;
                advance();
            }
            t.kind = TK_MACRO_DEF;
            t.text = macro_name;
            return t;
        }

        // 其他预处理指令：跳过到行尾（含行连接符）
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '\\' && peek(1) == '\n') { advance(); advance(); continue; }
            if (c == '\n') break;
            advance();
        }
        t.text = std::string(src_.substr(start, pos_ - start));
        return t;
    }

    Token read_string() {
        Token t;
        t.kind = TK_STRING;
        t.line = line_;
        t.col = col_;
        t.text = "\"";
        advance();
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '\\') {
                t.text += c;
                advance();
                if (pos_ < src_.size()) { t.text += src_[pos_]; advance(); }
            } else if (c == '"') {
                t.text += c;
                advance();
                break;
            } else {
                t.text += c;
                advance();
            }
        }
        return t;
    }

    Token read_char() {
        Token t;
        t.kind = TK_CHAR;
        t.line = line_;
        t.col = col_;
        t.text = "'";
        advance();
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '\\') {
                t.text += c;
                advance();
                if (pos_ < src_.size()) { t.text += src_[pos_]; advance(); }
            } else if (c == '\'') {
                t.text += c;
                advance();
                break;
            } else {
                t.text += c;
                advance();
            }
        }
        return t;
    }

    Token read_ident() {
        Token t;
        t.kind = TK_IDENT;
        t.line = line_;
        t.col = col_;
        while (pos_ < src_.size() && is_ident_char(src_[pos_])) {
            t.text += src_[pos_];
            advance();
        }
        return t;
    }

    Token read_number() {
        Token t;
        t.kind = TK_NUMBER;
        t.line = line_;
        t.col = col_;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (is_digit(c) || is_ident_start(c) || c == '.' || c == '+' || c == '-') {
                t.text += c;
                advance();
            } else {
                break;
            }
        }
        return t;
    }

    Token read_punct() {
        Token t;
        t.kind = TK_PUNCT;
        t.line = line_;
        t.col = col_;
        if (pos_ < src_.size()) {
            t.text = std::string(1, src_[pos_]);
            advance();
        }
        return t;
    }
};

// ============================================================
// 关键字集合
// ============================================================
static const std::unordered_set<std::string>& cpp_keywords() {
    static const std::unordered_set<std::string> kw = {
        "alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch",
        "char", "char8_t", "char16_t", "char32_t", "class", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue",
        "co_await", "co_return", "co_yield", "decltype", "default", "delete",
        "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
        "extern", "false", "final", "float", "for", "friend", "goto", "if",
        "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "nullptr", "operator", "override", "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "short", "signed",
        "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true", "try", "typedef",
        "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
        "volatile", "wchar_t", "while",
        "and", "or", "not", "xor", "bitand", "bitor", "compl",
        "and_eq", "or_eq", "xor_eq", "not_eq"
    };
    return kw;
}

static bool is_type_decl_kw(const std::string& s) {
    return s == "class" || s == "struct" || s == "union" || s == "enum" || s == "namespace";
}

static bool is_func_modifier(const std::string& s) {
    static const std::unordered_set<std::string> mods = {
        "const", "volatile", "override", "final", "noexcept", "static",
        "inline", "virtual", "explicit", "constexpr", "consteval", "constinit",
        "thread_local", "mutable", "register", "extern", "friend"
    };
    return mods.count(s) > 0;
}

static bool is_control_kw(const std::string& s) {
    static const std::unordered_set<std::string> ctrl = {
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "goto", "try", "catch", "throw"
    };
    return ctrl.count(s) > 0;
}

static bool is_type_kw(const std::string& s) {
    static const std::unordered_set<std::string> types = {
        "void", "int", "long", "short", "char", "float", "double", "bool",
        "unsigned", "signed", "auto", "wchar_t", "char8_t", "char16_t", "char32_t",
        "size_t", "ptrdiff_t", "nullptr_t"
    };
    return types.count(s) > 0;
}

// ============================================================
// 语法分析器
// ============================================================
class Parser {
public:
    Symbol parse(const std::string& src) {
        Lexer lex(src);
        root_ = Symbol{};
        root_.kind = SK_UNKNOWN;
        root_.name = "<root>";
        root_.line = 1;
        root_.depth = 0;

        scope_stack_.clear();
        scope_stack_.push_back(&root_);

        std::vector<std::string> pending_template_params;
        std::vector<Token> decl_tokens;

        while (true) {
            Token t = lex.next();
            if (t.kind == TK_EOF) break;

            if (t.kind == TK_MACRO_DEF) {
                Symbol s;
                s.kind = SK_MACRO;
                s.name = t.text;
                s.line = t.line;
                s.depth = (int)scope_stack_.size() - 1;
                scope_stack_.back()->children.push_back(std::move(s));
                decl_tokens.clear();
                continue;
            }

            if (t.kind == TK_PP) {
                decl_tokens.clear();
                continue;
            }

            if (t.kind == TK_STRING || t.kind == TK_CHAR || t.kind == TK_NUMBER) {
                decl_tokens.push_back(t);
                continue;
            }

            if (t.kind == TK_IDENT) {
                decl_tokens.push_back(t);
                continue;
            }

            // TK_PUNCT
            if (t.text == "{") {
                analyze_decl_before_brace(decl_tokens, pending_template_params, t.line);
                if (!scope_entered_) {
                    // 普通块语句，入栈占位符
                    scope_stack_.push_back(nullptr);
                }
                scope_entered_ = false;
                decl_tokens.clear();
            } else if (t.text == "}") {
                if (scope_stack_.size() > 1) {
                    // 找到最近一个非 nullptr 的作用域符号设置 end_line
                    for (int i = (int)scope_stack_.size() - 1; i >= 0; --i) {
                        if (scope_stack_[i] != nullptr) {
                            if (scope_stack_[i]->end_line == 0)
                                scope_stack_[i]->end_line = t.line;
                            break;
                        }
                    }
                    scope_stack_.pop_back();
                }
                decl_tokens.clear();
            } else if (t.text == ";") {
                analyze_decl_before_semicolon(decl_tokens, pending_template_params, t.line);
                decl_tokens.clear();
                pending_template_params.clear();
            } else {
                decl_tokens.push_back(t);
            }
        }

        if (root_.end_line == 0) root_.end_line = lex.cur_line();
        return root_;
    }

private:
    Symbol root_;
    std::vector<Symbol*> scope_stack_;
    bool scope_entered_ = false;

    void skip_attributes(const std::vector<Token>& toks, size_t& i) const {
        while (i < toks.size()) {
            if (toks[i].text == "[" && i + 1 < toks.size() && toks[i+1].text == "[") {
                int depth = 0;
                while (i < toks.size()) {
                    if (toks[i].text == "[") ++depth;
                    else if (toks[i].text == "]") {
                        --depth;
                        if (depth == 0) { ++i; break; }
                    }
                    ++i;
                }
            } else if (toks[i].kind == TK_IDENT && toks[i].text == "alignas" &&
                       i + 1 < toks.size() && toks[i+1].text == "(") {
                int depth = 0;
                i += 2;
                ++depth;
                while (i < toks.size() && depth > 0) {
                    if (toks[i].text == "(") ++depth;
                    else if (toks[i].text == ")") --depth;
                    ++i;
                }
            } else {
                break;
            }
        }
    }

    void skip_parens(const std::vector<Token>& toks, size_t& i) const {
        if (i >= toks.size() || toks[i].text != "(") return;
        int depth = 0;
        while (i < toks.size()) {
            if (toks[i].text == "(") ++depth;
            else if (toks[i].text == ")") {
                --depth;
                if (depth == 0) { ++i; break; }
            }
            ++i;
        }
    }

    void skip_template_args(const std::vector<Token>& toks, size_t& i) const {
        if (i >= toks.size() || toks[i].text != "<") return;
        int depth = 0;
        while (i < toks.size()) {
            if (toks[i].text == "<") ++depth;
            else if (toks[i].text == ">") {
                --depth;
                if (depth == 0) { ++i; break; }
            }
            ++i;
        }
    }

    // 提取从位置 i 开始的限定名（A::B::C），i 推进到名字之后
    std::string extract_qualified_name(const std::vector<Token>& toks, size_t& i) const {
        std::string name;
        while (i < toks.size()) {
            if (toks[i].kind == TK_IDENT) {
                name += toks[i].text;
                ++i;
                if (i + 1 < toks.size() && toks[i].text == ":" && toks[i+1].text == ":") {
                    name += "::";
                    i += 2;
                    continue;
                }
                break;
            } else break;
        }
        return name;
    }

    std::string extract_bases(const std::vector<Token>& toks, size_t i, size_t end) const {
        std::string bases;
        bool last_was_ident = false;
        for (; i < end; ++i) {
            bool is_ident = (toks[i].kind == TK_IDENT);
            if (!bases.empty() && is_ident && last_was_ident) bases += " ";
            bases += toks[i].text;
            last_was_ident = is_ident;
        }
        return bases;
    }

    // 检查 toks 中 [0, i) 区间是否有 friend 关键字
    bool has_friend_before(const std::vector<Token>& toks, size_t i) const {
        for (size_t j = 0; j < i; ++j) {
            if (toks[j].kind == TK_IDENT && toks[j].text == "friend") return true;
        }
        return false;
    }

    void analyze_decl_before_brace(std::vector<Token>& toks,
                                   std::vector<std::string>& pending_template_params,
                                   int brace_line) {
        scope_entered_ = false;
        if (toks.empty()) return;

        size_t i = 0;
        skip_attributes(toks, i);
        if (i >= toks.size()) return;

        // 收集前导修饰符
        std::vector<std::string> modifiers;
        while (i < toks.size() && toks[i].kind == TK_IDENT && is_func_modifier(toks[i].text)) {
            modifiers.push_back(toks[i].text);
            ++i;
            skip_attributes(toks, i);
        }
        if (i >= toks.size()) return;

        // namespace
        if (toks[i].kind == TK_IDENT && toks[i].text == "namespace") {
            int ns_line = toks[i].line;
            ++i;
            std::string name;
            if (i < toks.size() && toks[i].kind == TK_IDENT) {
                name = extract_qualified_name(toks, i);
            } else {
                name = "<anonymous>";
            }
            Symbol s;
            s.kind = SK_NAMESPACE;
            s.name = name;
            s.line = ns_line;
            s.modifiers = join(modifiers, " ");
            s.depth = (int)scope_stack_.size() - 1;
            s.template_params = pending_template_params;
            scope_stack_.back()->children.push_back(std::move(s));
            scope_stack_.push_back(&scope_stack_.back()->children.back());
            scope_entered_ = true;
            pending_template_params.clear();
            return;
        }

        // class/struct/union/enum
        if (toks[i].kind == TK_IDENT && is_type_decl_kw(toks[i].text)) {
            std::string kw = toks[i].text;
            int decl_line = toks[i].line;
            ++i;
            // enum class / enum struct
            if (kw == "enum" && i < toks.size() && toks[i].kind == TK_IDENT &&
                (toks[i].text == "class" || toks[i].text == "struct")) {
                ++i;
            }
            std::string name;
            if (i < toks.size() && toks[i].kind == TK_IDENT) {
                name = toks[i].text;
                ++i;
                if (i < toks.size() && toks[i].text == "<") {
                    skip_template_args(toks, i);
                }
            } else {
                name = "<anonymous>";
            }
            std::string bases;
            if (i < toks.size() && toks[i].text == ":") {
                ++i;
                bases = extract_bases(toks, i, toks.size());
            }
            // final 关键字处理（简化：忽略）
            SymKind k = SK_UNKNOWN;
            if (kw == "class") k = SK_CLASS;
            else if (kw == "struct") k = SK_STRUCT;
            else if (kw == "union") k = SK_UNION;
            else if (kw == "enum") k = SK_ENUM;

            Symbol s;
            s.kind = k;
            s.name = name;
            s.bases = bases;
            s.line = decl_line;
            s.modifiers = join(modifiers, " ");
            s.depth = (int)scope_stack_.size() - 1;
            s.template_params = pending_template_params;
            scope_stack_.back()->children.push_back(std::move(s));
            scope_stack_.push_back(&scope_stack_.back()->children.back());
            scope_entered_ = true;
            pending_template_params.clear();
            return;
        }

        // template<...>
        if (toks[i].kind == TK_IDENT && toks[i].text == "template") {
            ++i;
            if (i < toks.size() && toks[i].text == "<") {
                ++i;
                std::vector<std::string> params;
                std::string cur;
                int depth = 1;
                while (i < toks.size() && depth > 0) {
                    if (toks[i].text == "<") ++depth;
                    else if (toks[i].text == ">") {
                        --depth;
                        if (depth == 0) { ++i; break; }
                    }
                    else if (toks[i].text == "," && depth == 1) {
                        params.push_back(cur);
                        cur.clear();
                        ++i;
                        continue;
                    }
                    if (!cur.empty() && toks[i].kind != TK_PUNCT) cur += " ";
                    cur += toks[i].text;
                    ++i;
                }
                if (!cur.empty()) params.push_back(cur);
                pending_template_params = params;
            }
            // template 之后还有内容，递归分析剩余
            if (i < toks.size()) {
                std::vector<Token> remaining(toks.begin() + i, toks.end());
                toks = remaining;
                analyze_decl_before_brace(toks, pending_template_params, brace_line);
            }
            return;
        }

        // extern "C" { }
        if (toks[i].kind == TK_IDENT && toks[i].text == "extern" && i + 1 < toks.size()) {
            if (toks[i+1].kind == TK_STRING) {
                Symbol s;
                s.kind = SK_NAMESPACE;
                s.name = std::string("extern ") + toks[i+1].text;
                s.line = toks[i].line;
                s.depth = (int)scope_stack_.size() - 1;
                scope_stack_.back()->children.push_back(std::move(s));
                scope_stack_.push_back(&scope_stack_.back()->children.back());
                scope_entered_ = true;
                return;
            }
        }

        // 函数定义
        try_parse_function(toks, brace_line, modifiers, pending_template_params);
    }

    void try_parse_function(const std::vector<Token>& toks, int brace_line,
                            const std::vector<std::string>& modifiers,
                            const std::vector<std::string>& template_params) {
        if (toks.empty()) return;

        // 找最后一个 )
        int last_close_paren = -1;
        int depth = 0;
        for (int j = (int)toks.size() - 1; j >= 0; --j) {
            if (toks[j].text == ")") {
                if (depth == 0) { last_close_paren = j; break; }
                --depth;
            } else if (toks[j].text == "(") {
                ++depth;
            }
        }
        if (last_close_paren < 0) return;

        // 找匹配的 (
        int open_paren = -1;
        depth = 0;
        for (int j = last_close_paren; j >= 0; --j) {
            if (toks[j].text == ")") ++depth;
            else if (toks[j].text == "(") {
                --depth;
                if (depth == 0) { open_paren = j; break; }
            }
        }
        if (open_paren < 0) return;

        int name_end = open_paren - 1;
        if (name_end < 0) return;

        std::string func_name;
        std::string return_type;
        bool is_ctor = false;
        bool is_dtor = false;
        bool is_operator = false;

        if (toks[name_end].kind == TK_IDENT && toks[name_end].text == "operator") {
            // 运算符重载：把 operator 后面到 ( 之前的所有 token 作为函数名
            // 注意 operator() 的情况：operator 后面直接是 (
            // 这里 name_end 是 operator 的位置，open_paren 是 ( 的位置
            // 如果 name_end + 1 == open_paren，则是 operator()
            // 否则 operator 后面有运算符 token（如 ==, +, ++ 等）
            is_operator = true;
            func_name = "operator";
            // 简化：不收集运算符 token，统一为 "operator"
            // 用户可通过 -find "operator*" 查找
            // 收集返回类型
            for (int k = 0; k < name_end; ++k) {
                if (toks[k].kind == TK_IDENT || toks[k].kind == TK_STRING || toks[k].kind == TK_NUMBER) {
                    if (!return_type.empty()) return_type += " ";
                    return_type += toks[k].text;
                } else {
                    return_type += toks[k].text;
                }
            }
        } else if (toks[name_end].kind == TK_IDENT) {
            // 普通函数名，可能带限定符 Foo::Bar::baz
            int j = name_end;
            std::vector<std::string> parts;
            parts.push_back(toks[j].text);
            --j;
            while (j >= 1 && toks[j].text == ":" && toks[j-1].text == ":") {
                parts.push_back(toks[j-1].text);
                j -= 2;
            }
            std::reverse(parts.begin(), parts.end());
            func_name = join(parts, "::");

            if (!func_name.empty() && func_name[0] == '~') {
                is_dtor = true;
            } else if (name_end == 0) {
                is_ctor = true;
            } else {
                // 构造函数判断：限定名最后部分等于倒数第二部分
                if (parts.size() >= 2 && parts.back() == parts[parts.size()-2]) {
                    is_ctor = true;
                }
            }

            if (name_end > 0 && !is_ctor) {
                for (int k = 0; k < name_end; ++k) {
                    if (toks[k].kind == TK_IDENT || toks[k].kind == TK_STRING || toks[k].kind == TK_NUMBER) {
                        if (!return_type.empty()) return_type += " ";
                        return_type += toks[k].text;
                    } else {
                        return_type += toks[k].text;
                    }
                }
            }
        } else {
            return;
        }

        if (is_control_kw(func_name)) return;

        // 收集 ) 之后的修饰符
        std::vector<std::string> post_modifiers;
        for (int j = last_close_paren + 1; j < (int)toks.size(); ++j) {
            if (toks[j].kind == TK_IDENT) {
                post_modifiers.push_back(toks[j].text);
            } else if (toks[j].text == "->") {
                // trailing return type
                std::string trailing;
                for (int k = j + 1; k < (int)toks.size(); ++k) {
                    if (!trailing.empty()) trailing += " ";
                    trailing += toks[k].text;
                }
                return_type = trailing;
                break;
            }
        }

        // 参数列表
        std::string params;
        for (int j = open_paren + 1; j < last_close_paren; ++j) {
            if (toks[j].kind == TK_IDENT || toks[j].kind == TK_STRING || toks[j].kind == TK_NUMBER) {
                if (!params.empty()) params += " ";
                params += toks[j].text;
            } else {
                params += toks[j].text;
            }
        }

        std::vector<std::string> all_mods = modifiers;
        for (auto& m : post_modifiers) all_mods.push_back(m);

        Symbol s;
        s.kind = SK_FUNCTION;
        s.name = func_name;
        s.signature = "(" + params + ")";
        s.modifiers = join(all_mods, " ");
        s.line = toks[0].line;
        s.depth = (int)scope_stack_.size() - 1;
        s.template_params = template_params;
        s.is_ctor = is_ctor;
        s.is_dtor = is_dtor;
        s.is_operator = is_operator;
        if (!return_type.empty()) {
            s.signature = return_type + " " + func_name + s.signature;
        }

        scope_stack_.back()->children.push_back(std::move(s));
        scope_entered_ = false;
    }

    void analyze_decl_before_semicolon(std::vector<Token>& toks,
                                       std::vector<std::string>& pending_template_params,
                                       int semi_line) {
        if (toks.empty()) return;

        size_t i = 0;
        skip_attributes(toks, i);
        if (i >= toks.size()) return;

        // typedef
        if (toks[i].kind == TK_IDENT && toks[i].text == "typedef") {
            ++i;
            std::string typedef_name;
            int typedef_line = semi_line;
            if (i < toks.size() && toks[i].kind == TK_IDENT && is_type_decl_kw(toks[i].text)) {
                for (size_t j = i; j < toks.size(); ++j) {
                    if (toks[j].kind == TK_IDENT && !is_type_decl_kw(toks[j].text)) {
                        typedef_name = toks[j].text;
                        typedef_line = toks[j].line;
                    }
                }
            } else {
                for (size_t j = i; j < toks.size(); ++j) {
                    if (toks[j].kind == TK_IDENT) {
                        typedef_name = toks[j].text;
                        typedef_line = toks[j].line;
                    }
                }
            }
            if (!typedef_name.empty()) {
                Symbol s;
                s.kind = SK_TYPEDEF;
                s.name = typedef_name;
                s.line = typedef_line;
                s.depth = (int)scope_stack_.size() - 1;
                s.template_params = pending_template_params;
                scope_stack_.back()->children.push_back(std::move(s));
            }
            pending_template_params.clear();
            return;
        }

        // using
        if (toks[i].kind == TK_IDENT && toks[i].text == "using") {
            ++i;
            if (i < toks.size() && toks[i].kind == TK_IDENT && toks[i].text == "namespace") {
                pending_template_params.clear();
                return;
            }
            if (i < toks.size() && toks[i].kind == TK_IDENT) {
                Symbol s;
                s.kind = SK_USING;
                s.name = toks[i].text;
                s.line = toks[i].line;
                s.depth = (int)scope_stack_.size() - 1;
                s.template_params = pending_template_params;
                scope_stack_.back()->children.push_back(std::move(s));
            }
            pending_template_params.clear();
            return;
        }

        // 前向声明：class/struct/union/enum Name;
        if (toks[i].kind == TK_IDENT && is_type_decl_kw(toks[i].text)) {
            std::string kw = toks[i].text;
            int decl_line = toks[i].line;
            ++i;
            if (kw == "enum" && i < toks.size() && toks[i].kind == TK_IDENT &&
                (toks[i].text == "class" || toks[i].text == "struct")) {
                ++i;
            }
            bool is_friend = has_friend_before(toks, i);
            if (i < toks.size() && toks[i].kind == TK_IDENT) {
                std::string name = toks[i].text;
                Symbol s;
                if (is_friend) {
                    s.kind = SK_FRIEND;
                } else {
                    s.kind = (kw == "class") ? SK_CLASS : (kw == "struct") ? SK_STRUCT :
                             (kw == "union") ? SK_UNION : SK_ENUM;
                }
                s.name = name;
                s.line = decl_line;
                s.depth = (int)scope_stack_.size() - 1;
                s.is_definition = false;
                s.template_params = pending_template_params;
                scope_stack_.back()->children.push_back(std::move(s));
            }
            pending_template_params.clear();
            return;
        }

        // 函数声明：标识符 ( ... ) ;
        bool has_paren = false;
        for (auto& t : toks) if (t.text == "(") { has_paren = true; break; }
        if (has_paren) {
            bool saved = scope_entered_;
            try_parse_function(toks, semi_line, {}, pending_template_params);
            scope_entered_ = saved;
            pending_template_params.clear();
            return;
        }

        // 变量声明（简化）
        if (toks[i].kind == TK_IDENT && !is_control_kw(toks[i].text) &&
            !cpp_keywords().count(toks[i].text)) {
            Symbol s;
            s.kind = SK_VAR;
            s.name = toks[i].text;
            s.line = toks[i].line;
            s.depth = (int)scope_stack_.size() - 1;
            s.template_params = pending_template_params;
            scope_stack_.back()->children.push_back(std::move(s));
        }
        pending_template_params.clear();
    }

    static std::string join(const std::vector<std::string>& v, const std::string& sep) {
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out += sep;
            out += v[i];
        }
        return out;
    }
};

// ============================================================
// 输出格式化
// ============================================================

static void print_tree(const Symbol& s, int max_depth, bool show_line, int indent = 0) {
    if (max_depth >= 0 && s.depth > max_depth && s.kind != SK_UNKNOWN) return;
    if (s.kind != SK_UNKNOWN) {
        for (int i = 0; i < indent; ++i) printf("  ");
        if (show_line && s.line > 0) printf("L%-5d ", s.line);
        printf("%s %s", kind_name(s.kind), s.name.c_str());
        if (s.kind == SK_FUNCTION && !s.signature.empty()) {
            size_t paren = s.signature.find('(');
            if (paren != std::string::npos) {
                printf("%s", s.signature.substr(paren).c_str());
            }
        }
        if (!s.bases.empty()) printf(" : %s", s.bases.c_str());
        if (!s.modifiers.empty()) printf("  [%s]", s.modifiers.c_str());
        if (s.is_ctor) printf("  [ctor]");
        if (s.is_dtor) printf("  [dtor]");
        if (s.is_operator) printf("  [operator]");
        if (!s.is_definition) printf("  [decl]");
        if (!s.template_params.empty()) {
            printf("  <template: %zu>", s.template_params.size());
        }
        if (s.end_line > 0 && s.end_line != s.line) {
            printf("  (end L%d)", s.end_line);
        }
        printf("\n");
    }
    for (auto& c : s.children) {
        print_tree(c, max_depth, show_line, indent + 1);
    }
}

static void print_flat(const Symbol& s, int max_depth, bool show_line) {
    if (max_depth >= 0 && s.depth > max_depth && s.kind != SK_UNKNOWN) return;
    if (s.kind != SK_UNKNOWN) {
        if (show_line && s.line > 0) printf("L%-5d ", s.line);
        printf("D%-2d %s %s", s.depth, kind_name(s.kind), s.name.c_str());
        if (s.kind == SK_FUNCTION && !s.signature.empty()) {
            size_t paren = s.signature.find('(');
            if (paren != std::string::npos) {
                printf("%s", s.signature.substr(paren).c_str());
            }
        }
        if (!s.bases.empty()) printf(" : %s", s.bases.c_str());
        if (!s.modifiers.empty()) printf("  [%s]", s.modifiers.c_str());
        if (s.is_ctor) printf("  [ctor]");
        if (s.is_dtor) printf("  [dtor]");
        if (!s.is_definition) printf("  [decl]");
        printf("\n");
    }
    for (auto& c : s.children) print_flat(c, max_depth, show_line);
}

static void print_json_symbol(const Symbol& s, int max_depth, int indent, std::string& out) {
    if (s.kind == SK_UNKNOWN) {
        // root: 输出 children 数组
        out += "[\n";
        for (size_t i = 0; i < s.children.size(); ++i) {
            out += std::string(indent + 2, ' ');
            print_json_symbol(s.children[i], max_depth, indent + 2, out);
            if (i + 1 < s.children.size()) out += ",";
            out += "\n";
        }
        out += std::string(indent, ' ') + "]";
        return;
    }
    if (max_depth >= 0 && s.depth > max_depth) {
        out += "null";
        return;
    }
    out += "{";
    out += "\"kind\":\"" + std::string(kind_name(s.kind)) + "\",";
    out += "\"name\":\"" + json_escape(s.name) + "\",";
    out += "\"line\":" + std::to_string(s.line) + ",";
    out += "\"end_line\":" + std::to_string(s.end_line) + ",";
    out += "\"depth\":" + std::to_string(s.depth) + ",";
    out += "\"is_definition\":" + std::string(s.is_definition ? "true" : "false");
    if (s.kind == SK_FUNCTION) {
        out += ",\"signature\":\"" + json_escape(s.signature) + "\"";
        out += ",\"is_ctor\":" + std::string(s.is_ctor ? "true" : "false");
        out += ",\"is_dtor\":" + std::string(s.is_dtor ? "true" : "false");
        out += ",\"is_operator\":" + std::string(s.is_operator ? "true" : "false");
    }
    if (!s.bases.empty()) out += ",\"bases\":\"" + json_escape(s.bases) + "\"";
    if (!s.modifiers.empty()) out += ",\"modifiers\":\"" + json_escape(s.modifiers) + "\"";
    if (!s.template_params.empty()) {
        out += ",\"template_params\":[";
        for (size_t i = 0; i < s.template_params.size(); ++i) {
            if (i) out += ",";
            out += "\"" + json_escape(s.template_params[i]) + "\"";
        }
        out += "]";
    }
    if (!s.children.empty() && (max_depth < 0 || s.depth < max_depth)) {
        out += ",\"children\":[\n";
        for (size_t i = 0; i < s.children.size(); ++i) {
            out += std::string(indent + 2, ' ');
            print_json_symbol(s.children[i], max_depth, indent + 2, out);
            if (i + 1 < s.children.size()) out += ",";
            out += "\n";
        }
        out += std::string(indent, ' ') + "]";
    }
    out += "}";
}

// 查找 root 树中 target 的祖先链（不含 target 自身）
static void find_parent_chain(const Symbol& root, const Symbol* target,
                              std::vector<const Symbol*>& chain) {
    if (&root == target) return;
    for (auto& c : root.children) {
        if (&c == target) {
            chain.push_back(&root);
            return;
        }
        size_t saved = chain.size();
        chain.push_back(&root);
        find_parent_chain(c, target, chain);
        if (!chain.empty() && chain.back() == &root && saved == chain.size() - 1) {
            // 没找到，回溯
            chain.pop_back();
        } else {
            return;
        }
    }
}

// ============================================================
// 主命令
// ============================================================
int cmd_outline(int argc, char** argv) {
    std::string path;
    std::string find_pattern;
    std::string kind_filter_str;
    int target_line = -1;
    int max_depth = -1;
    bool flat = false;
    bool json_out = false;
    bool show_line = true;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printf(
                "tbx outline - C++ 代码大纲解析器\n\n"
                "USAGE: tbx outline <file> [options]\n\n"
                "OPTIONS:\n"
                "  -find NAME    按名称搜索符号 (支持通配符 * ?)\n"
                "  -kind K       过滤类型 (class/struct/union/enum/namespace/function/typedef/using/macro/var)\n"
                "  -line N       显示包含第 N 行的符号及其父作用域链\n"
                "  -flat         平铺输出 (不缩进)\n"
                "  -json         JSON 输出\n"
                "  -no-line      不显示行号\n"
                "  -depth N      最大嵌套深度 (-1 无限)\n"
                "  -h, --help    显示帮助\n\n"
                "EXAMPLES:\n"
                "  tbx outline main.cpp\n"
                "  tbx outline main.cpp -find \"abs*Newton*\"\n"
                "  tbx outline main.cpp -kind function -flat\n"
                "  tbx outline main.cpp -line 1234\n"
                "  tbx outline main.cpp -json -depth 2\n"
            );
            return 0;
        } else if (a == "-find" && i + 1 < argc) {
            find_pattern = argv[++i];
        } else if (a == "-kind" && i + 1 < argc) {
            kind_filter_str = argv[++i];
        } else if (a == "-line" && i + 1 < argc) {
            target_line = atoi(argv[++i]);
        } else if (a == "-flat") {
            flat = true;
        } else if (a == "-json") {
            json_out = true;
        } else if (a == "-no-line") {
            show_line = false;
        } else if (a == "-depth" && i + 1 < argc) {
            max_depth = atoi(argv[++i]);
        } else if (path.empty()) {
            path = a;
        }
    }

    if (path.empty()) {
        fprintf(stderr, "error: missing file. Run 'tbx outline -h' for help.\n");
        return 1;
    }

    std::string text;
    if (!read_text(path, text)) {
        fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return 1;
    }

    int total_lines = 1;
    for (char c : text) if (c == '\n') ++total_lines;

    Parser parser;
    Symbol root = parser.parse(text);

    // 模式 1: 按名称搜索
    if (!find_pattern.empty()) {
        std::vector<const Symbol*> matches;
        root.find_by_name(find_pattern, matches);
        if (matches.empty()) {
            printf("No symbols matching '%s'\n", find_pattern.c_str());
            return 0;
        }
        printf("Found %zu symbol(s) matching '%s':\n\n", matches.size(), find_pattern.c_str());
        for (auto* s : matches) {
            if (show_line && s->line > 0) printf("L%-5d ", s->line);
            printf("D%-2d %s %s", s->depth, kind_name(s->kind), s->name.c_str());
            if (s->kind == SK_FUNCTION && !s->signature.empty()) {
                size_t paren = s->signature.find('(');
                if (paren != std::string::npos) printf("%s", s->signature.substr(paren).c_str());
            }
            if (!s->bases.empty()) printf(" : %s", s->bases.c_str());
            if (s->end_line > 0) printf("  (end L%d)", s->end_line);
            printf("\n");
            std::vector<const Symbol*> chain;
            find_parent_chain(root, s, chain);
            if (!chain.empty()) {
                printf("       path: ");
                for (size_t i = 0; i < chain.size(); ++i) {
                    if (i) printf(" :: ");
                    printf("%s", chain[i]->name.c_str());
                }
                printf("\n");
            }
        }
        return 0;
    }

    // 模式 2: 按行号反向查询
    if (target_line > 0) {
        const Symbol* s = root.find_at_line(target_line);
        if (!s) {
            printf("Line %d: not inside any symbol\n", target_line);
            return 0;
        }
        printf("Line %d belongs to:\n\n", target_line);
        std::vector<const Symbol*> chain;
        find_parent_chain(root, s, chain);
        for (size_t i = 0; i < chain.size(); ++i) {
            for (size_t j = 0; j < i; ++j) printf("  ");
            const Symbol* p = chain[i];
            printf("L%-5d %s %s", p->line, kind_name(p->kind), p->name.c_str());
            if (p->end_line > 0) printf("  (L%d-%d)", p->line, p->end_line);
            printf("\n");
        }
        for (size_t j = 0; j < chain.size(); ++j) printf("  ");
        printf("L%-5d %s %s", s->line, kind_name(s->kind), s->name.c_str());
        if (s->kind == SK_FUNCTION && !s->signature.empty()) {
            size_t paren = s->signature.find('(');
            if (paren != std::string::npos) printf("%s", s->signature.substr(paren).c_str());
        }
        if (s->end_line > 0) printf("  (L%d-%d)", s->line, s->end_line);
        printf("\n");
        return 0;
    }

    // 模式 3: 完整大纲 / 类型过滤
    SymKind kind_filter = kind_filter_str.empty() ? SK_UNKNOWN : kind_from_str(kind_filter_str);

    if (json_out) {
        std::string out;
        out += "{\n";
        out += "  \"file\": \"" + json_escape(path) + "\",\n";
        out += "  \"total_lines\": " + std::to_string(total_lines) + ",\n";
        out += "  \"symbols\": ";
        if (kind_filter == SK_UNKNOWN) {
            print_json_symbol(root, max_depth, 2, out);
        } else {
            // 类型过滤的 JSON 输出：平铺所有匹配符号
            std::vector<const Symbol*> flat_syms;
            root.collect_flat(flat_syms);
            out += "[\n";
            bool first = true;
            for (auto* s : flat_syms) {
                if (s->kind == kind_filter) {
                    if (!first) out += ",\n";
                    first = false;
                    out += std::string(2, ' ') + "{";
                    out += "\"kind\":\"" + std::string(kind_name(s->kind)) + "\",";
                    out += "\"name\":\"" + json_escape(s->name) + "\",";
                    out += "\"line\":" + std::to_string(s->line) + ",";
                    out += "\"end_line\":" + std::to_string(s->end_line) + ",";
                    out += "\"depth\":" + std::to_string(s->depth);
                    if (s->kind == SK_FUNCTION) out += ",\"signature\":\"" + json_escape(s->signature) + "\"";
                    if (!s->bases.empty()) out += ",\"bases\":\"" + json_escape(s->bases) + "\"";
                    if (!s->modifiers.empty()) out += ",\"modifiers\":\"" + json_escape(s->modifiers) + "\"";
                    out += "}";
                }
            }
            out += "\n" + std::string(2, ' ') + "]";
        }
        out += "\n}\n";
        printf("%s", out.c_str());
        return 0;
    }

    // 文本输出
    printf("%s (%d lines)\n", path.c_str(), total_lines);
    if (kind_filter != SK_UNKNOWN) {
        std::vector<const Symbol*> flat_syms;
        root.collect_flat(flat_syms);
        int shown = 0;
        for (auto* s : flat_syms) {
            if (s->kind == kind_filter) {
                if (show_line && s->line > 0) printf("L%-5d ", s->line);
                printf("D%-2d %s %s", s->depth, kind_name(s->kind), s->name.c_str());
                if (s->kind == SK_FUNCTION && !s->signature.empty()) {
                    size_t paren = s->signature.find('(');
                    if (paren != std::string::npos) printf("%s", s->signature.substr(paren).c_str());
                }
                if (!s->bases.empty()) printf(" : %s", s->bases.c_str());
                if (s->end_line > 0 && s->end_line != s->line) printf("  (end L%d)", s->end_line);
                printf("\n");
                ++shown;
            }
        }
        printf("\n%d %s symbol(s)\n", shown, kind_name(kind_filter));
    } else {
        if (flat) print_flat(root, max_depth, show_line);
        else print_tree(root, max_depth, show_line);
    }
    return 0;
}

} // namespace tbx
