// cmd_es.cpp - Everything HTTP 搜索 (localhost:21465)
// 调用 Everything 的 HTTP API，解析 JSON 并格式化输出
#include "common.h"

namespace tbx {

// ===================== 迷你 JSON 解析器 =====================
struct JsonValue {
    enum Type { JNull, JBool, JNum, JStr, JArr, JObj };
    Type type = JNull;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    const JsonValue* find(const std::string& key) const {
        if (type != JObj) return nullptr;
        for (auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

struct JsonParser {
    const char* p;
    const char* end;

    void skip_ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }
    bool parse(JsonValue& out) {
        skip_ws();
        if (p >= end) return false;
        char c = *p;
        if (c == '{') return parse_obj(out);
        if (c == '[') return parse_arr(out);
        if (c == '"') return parse_str(out);
        if (c == 't' || c == 'f') return parse_bool(out);
        if (c == 'n') return parse_null(out);
        return parse_num(out);
    }
    bool parse_obj(JsonValue& out) {
        out.type = JsonValue::JObj;
        ++p; skip_ws();
        if (p < end && *p == '}') { ++p; return true; }
        while (p < end) {
            skip_ws();
            if (p >= end || *p != '"') return false;
            JsonValue key;
            if (!parse_str(key)) return false;
            skip_ws();
            if (p >= end || *p != ':') return false;
            ++p;
            JsonValue val;
            if (!parse(val)) return false;
            out.obj.emplace_back(key.str, val);
            skip_ws();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; return true; }
            return false;
        }
        return false;
    }
    bool parse_arr(JsonValue& out) {
        out.type = JsonValue::JArr;
        ++p; skip_ws();
        if (p < end && *p == ']') { ++p; return true; }
        while (p < end) {
            JsonValue val;
            if (!parse(val)) return false;
            out.arr.push_back(val);
            skip_ws();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; return true; }
            return false;
        }
        return false;
    }
    bool parse_str(JsonValue& out) {
        out.type = JsonValue::JStr;
        ++p; // skip "
        while (p < end) {
            char c = *p++;
            if (c == '"') return true;
            if (c == '\\' && p < end) {
                char e = *p++;
                switch (e) {
                case '"': out.str += '"'; break;
                case '\\': out.str += '\\'; break;
                case '/': out.str += '/'; break;
                case 'n': out.str += '\n'; break;
                case 't': out.str += '\t'; break;
                case 'r': out.str += '\r'; break;
                case 'b': out.str += '\b'; break;
                case 'f': out.str += '\f'; break;
                case 'u':
                    if (p + 4 <= end) {
                        char buf[5] = { p[0],p[1],p[2],p[3],0 };
                        unsigned cp = (unsigned)strtoul(buf, nullptr, 16);
                        p += 4;
                        if (cp < 0x80) out.str += (char)cp;
                        else if (cp < 0x800) {
                            out.str += (char)(0xC0 | (cp >> 6));
                            out.str += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out.str += (char)(0xE0 | (cp >> 12));
                            out.str += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out.str += (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                }
            } else {
                out.str += c;
            }
        }
        return false;
    }
    bool parse_num(JsonValue& out) {
        out.type = JsonValue::JNum;
        const char* s = p;
        while (p < end && (*p == '-' || *p == '+' || *p == '.' || (*p >= '0' && *p <= '9') || *p == 'e' || *p == 'E')) ++p;
        out.num = strtod(std::string(s, p).c_str(), nullptr);
        return true;
    }
    bool parse_bool(JsonValue& out) {
        out.type = JsonValue::JBool;
        if (p + 4 <= end && strncmp(p, "true", 4) == 0) { out.b = true; p += 4; return true; }
        if (p + 5 <= end && strncmp(p, "false", 5) == 0) { out.b = false; p += 5; return true; }
        return false;
    }
    bool parse_null(JsonValue& out) {
        out.type = JsonValue::JNull;
        if (p + 4 <= end && strncmp(p, "null", 4) == 0) { p += 4; return true; }
        return false;
    }
};

// JSON 美化输出
static void print_json(const JsonValue& v, int indent) {
    auto pad = [indent]() { for (int i = 0; i < indent; ++i) printf("  "); };
    switch (v.type) {
    case JsonValue::JNull: printf("null"); break;
    case JsonValue::JBool: printf(v.b ? "true" : "false"); break;
    case JsonValue::JNum:
        if (v.num == (long long)v.num) printf("%lld", (long long)v.num);
        else printf("%g", v.num);
        break;
    case JsonValue::JStr: printf("\"%s\"", json_escape(v.str).c_str()); break;
    case JsonValue::JArr:
        if (v.arr.empty()) { printf("[]"); return; }
        printf("[\n");
        for (size_t i = 0; i < v.arr.size(); ++i) {
            pad(); printf("  ");
            print_json(v.arr[i], indent + 1);
            if (i + 1 < v.arr.size()) printf(",");
            printf("\n");
        }
        pad(); printf("]");
        break;
    case JsonValue::JObj:
        if (v.obj.empty()) { printf("{}"); return; }
        printf("{\n");
        for (size_t i = 0; i < v.obj.size(); ++i) {
            pad(); printf("  \"%s\": ", json_escape(v.obj[i].first).c_str());
            print_json(v.obj[i].second, indent + 1);
            if (i + 1 < v.obj.size()) printf(",");
            printf("\n");
        }
        pad(); printf("}");
        break;
    }
}

// ===================== Everything 搜索 =====================
int cmd_es(int argc, char** argv) {
    bool opt_json = false;
    bool opt_raw = false;
    int count = 50;
    int offset = 0;
    bool opt_path = true;
    bool opt_size = false;
    std::string query;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-json") opt_json = true;
        else if (a == "-raw") opt_raw = true;
        else if (a == "-count" && i + 1 < argc) count = atoi(argv[++i]);
        else if (a == "-offset" && i + 1 < argc) offset = atoi(argv[++i]);
        else if (a == "-size") opt_size = true;
        else if (a == "-no-path") opt_path = false;
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx es <query> [options]\n"
                   "  -count N    结果数 (默认 50)\n"
                   "  -offset N   偏移\n"
                   "  -size       显示文件大小\n"
                   "  -no-path    不显示路径\n"
                   "  -json       JSON 格式输出\n"
                   "  -raw        原始 JSON (Everything 返回)\n"
                   "\n需要 Everything 开启 HTTP 服务 (端口 21465)\n");
            return 0;
        } else {
            if (!query.empty()) query += " ";
            query += a;
        }
    }
    if (query.empty()) {
        fprintf(stderr, "error: usage: tbx es <query> [options]\n");
        return 1;
    }

    // 构建 URL
    std::string url = "/?search=" + url_encode(query) + "&json=1";
    url += "&count=" + std::to_string(count);
    if (offset > 0) url += "&offset=" + std::to_string(offset);
    if (opt_path) url += "&path_column=1";
    if (opt_size) url += "&size_column=1";

    std::string body;
    if (!http_get(L"localhost", 21465, u8_to_w(url), body)) {
        fprintf(stderr, "error: HTTP request failed. Is Everything running with HTTP server on :21465?\n");
        return 1;
    }

    if (opt_raw) {
        printf("%s\n", body.c_str());
        return 0;
    }

    // 解析 JSON
    JsonParser parser{ body.c_str(), body.c_str() + body.size() };
    JsonValue root;
    if (!parser.parse(root)) {
        fprintf(stderr, "error: JSON parse failed. Raw:\n%s\n", body.c_str());
        return 1;
    }

    if (opt_json) {
        print_json(root, 0);
        printf("\n");
        return 0;
    }

    // 表格化输出
    const JsonValue* total = root.find("totalResults");
    const JsonValue* results = root.find("results");
    long long totalN = total ? (long long)total->num : 0;
    printf("Everything: %s\n", query.c_str());
    printf("Total: %lld, Showing: %zu\n\n", totalN, results ? results->arr.size() : (size_t)0);

    if (!results) return 0;
    for (size_t i = 0; i < results->arr.size(); ++i) {
        const JsonValue& r = results->arr[i];
        const JsonValue* name = r.find("name");
        const JsonValue* path = r.find("path");
        const JsonValue* size = r.find("size");
        const JsonValue* type = r.find("type");

        std::string nameStr = name ? name->str : "?";
        std::string pathStr = path ? path->str : "";
        std::string typeStr = type ? type->str : "";

        if (opt_path && !pathStr.empty()) {
            // Windows 路径：path 已含完整路径
            printf("[%s] %s\\%s", typeStr.c_str(), pathStr.c_str(), nameStr.c_str());
        } else {
            printf("[%s] %s", typeStr.c_str(), nameStr.c_str());
        }
        if (opt_size && size) {
            if (size->num == (long long)size->num)
                printf("  (%lld bytes)", (long long)size->num);
            else
                printf("  (%g bytes)", size->num);
        }
        printf("\n");
    }
    return 0;
}

} // namespace tbx
