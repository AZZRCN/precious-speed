// cmd_split.cpp - 文本拆分分析器
// 按字符类型（字母/数字/特殊字符/空白等）拆分文本，统计各类 token
#include "common.h"
#include <map>
#include <sstream>

namespace tbx {

// 统计字符类型分布
int cmd_split(int argc, char** argv) {
    bool as_json = false;
    bool show_tokens = false;
    bool by_class = false; // 按字符类分组输出连续 token
    std::string path;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-json") as_json = true;
        else if (a == "-tokens") show_tokens = true;
        else if (a == "-class") by_class = true;
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx split <file> [-json] [-tokens] [-class]\n"
                   "  -json    输出 JSON 格式\n"
                   "  -tokens  输出所有 token\n"
                   "  -class   按字符类分组输出连续 token\n");
            return 0;
        } else if (path.empty()) path = a;
    }
    if (path.empty()) {
        fprintf(stderr, "error: missing file. usage: tbx split <file> [-json] [-tokens] [-class]\n");
        return 1;
    }
    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return 1;
    }

    // 字符类统计
    long long cls_count[9] = {0};
    for (uint8_t c : data) ++cls_count[classify_byte(c)];

    // token 统计：连续同类字符为一个 token
    struct Token { CharClass cls; std::string text; size_t start; };
    std::vector<Token> tokens;
    if (show_tokens || by_class) {
        CharClass prev = TCC_NONE;
        size_t run_start = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            CharClass k = classify_byte(data[i]);
            if (k != prev || i == 0) {
                if (i > 0 && prev != TCC_NONE) {
                    Token t; t.cls = prev; t.start = run_start;
                    t.text.assign((char*)data.data() + run_start, i - run_start);
                    tokens.push_back(t);
                }
                run_start = i;
                prev = k;
            }
        }
        if (!data.empty()) {
            Token t; t.cls = prev; t.start = run_start;
            t.text.assign((char*)data.data() + run_start, data.size() - run_start);
            tokens.push_back(t);
        }
    }

    if (as_json) {
        printf("{\"file\":\"%s\",\"size\":%zu,\"classes\":{",
               json_escape(path).c_str(), data.size());
        const char* names[] = { "UPPER","LOWER","DIGIT","WS","PUNCT","CTRL","HIGH","OTHER" };
        CharClass ids[] = { TCC_UPPER, TCC_LOWER, TCC_DIGIT, TCC_WHITESPACE, TCC_PUNCT, TCC_CTRL, TCC_HIGH, TCC_OTHER };
        for (int j = 0; j < 8; ++j) {
            if (j) printf(",");
            printf("\"%s\":%lld", names[j], cls_count[ids[j]]);
        }
        printf("}");
        if (show_tokens || by_class) {
            printf(",\"tokens\":[");
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i) printf(",");
                printf("{\"cls\":\"%s\",\"start\":%zu,\"text\":\"%s\"}",
                       class_name(tokens[i].cls), tokens[i].start, json_escape(tokens[i].text).c_str());
            }
            printf("]");
        }
        printf("}\n");
        return 0;
    }

    // 文本输出
    printf("File: %s\n", path.c_str());
    printf("Size: %zu bytes\n\n", data.size());
    printf("Char class distribution:\n");
    printf("  UPPER     : %lld\n", cls_count[TCC_UPPER]);
    printf("  LOWER     : %lld\n", cls_count[TCC_LOWER]);
    printf("  DIGIT     : %lld\n", cls_count[TCC_DIGIT]);
    printf("  WHITESPACE: %lld\n", cls_count[TCC_WHITESPACE]);
    printf("  PUNCT     : %lld\n", cls_count[TCC_PUNCT]);
    printf("  CTRL      : %lld\n", cls_count[TCC_CTRL]);
    printf("  HIGH(0x80): %lld\n", cls_count[TCC_HIGH]);
    printf("  OTHER     : %lld\n", cls_count[TCC_OTHER]);

    if (by_class) {
        printf("\nTokens by class (consecutive same-class runs):\n");
        for (size_t i = 0; i < tokens.size(); ++i) {
            printf("  [%s @%zu] \"", class_name(tokens[i].cls), tokens[i].start);
            // 截断过长 token
            if (tokens[i].text.size() > 60)
                printf("%.60s...", tokens[i].text.c_str());
            else
                printf("%s", tokens[i].text.c_str());
            printf("\"\n");
        }
    } else if (show_tokens) {
        printf("\nAll tokens:\n");
        for (size_t i = 0; i < tokens.size(); ++i) {
            printf("  %4zu [%s] \"", tokens[i].start, class_name(tokens[i].cls));
            if (tokens[i].text.size() > 80)
                printf("%.80s...", tokens[i].text.c_str());
            else
                printf("%s", tokens[i].text.c_str());
            printf("\"\n");
        }
    }
    return 0;
}

} // namespace tbx
