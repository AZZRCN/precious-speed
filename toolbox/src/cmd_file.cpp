// cmd_file.cpp - 文件操作工具集
// tree:   递归打印目录树（解决 LS 显示不全）
// head:   文件头部采样（前 N 行/字节）
// lines:  按行号范围读取
// hex:    十六进制查看器
// wc:     统计行数/单词数/字节数
#include "common.h"

namespace tbx {

// ===================== tree =====================
static void tree_walk(const std::string& dir, const std::string& prefix, int depth, int max_depth,
                      bool show_size, int& file_count, int& dir_count) {
    if (max_depth >= 0 && depth > max_depth) return;
    WIN32_FIND_DATAA fd;
    std::string search = dir + "\\*";
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    // 收集条目
    struct Entry { std::string name; bool is_dir; long long size; };
    std::vector<Entry> dirs, files;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        long long sz = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        if (isDir) dirs.push_back({name, true, sz});
        else files.push_back({name, false, sz});
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    std::vector<Entry> entries;
    for (auto& d : dirs) entries.push_back(d);
    for (auto& f : files) entries.push_back(f);

    for (size_t i = 0; i < entries.size(); ++i) {
        bool last = (i + 1 == entries.size());
        const Entry& e = entries[i];
        printf("%s%s%s", prefix.c_str(), last ? "`-- " : "|-- ", e.name.c_str());
        if (show_size && !e.is_dir) printf("  (%lld B)", e.size);
        printf("\n");
        if (e.is_dir) {
            ++dir_count;
            std::string next_prefix = prefix + (last ? "    " : "|   ");
            tree_walk(dir + "\\" + e.name, next_prefix, depth + 1, max_depth, show_size, file_count, dir_count);
        } else {
            ++file_count;
        }
    }
}

int cmd_tree(int argc, char** argv) {
    std::string dir = ".";
    int max_depth = -1;
    bool show_size = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-depth" && i + 1 < argc) max_depth = atoi(argv[++i]);
        else if (a == "-size") show_size = true;
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx tree [dir] [-depth N] [-size]\n"
                   "  -depth N  最大深度 (-1 无限)\n"
                   "  -size     显示文件大小\n");
            return 0;
        } else dir = a;
    }
    // 去掉末尾反斜杠
    while (dir.size() > 1 && dir.back() == '\\') dir.pop_back();

    printf("%s\n", dir.c_str());
    int fc = 0, dc = 0;
    tree_walk(dir, "", 1, max_depth, show_size, fc, dc);
    printf("\n%d directories, %d files\n", dc, fc);
    return 0;
}

// ===================== head =====================
int cmd_head(int argc, char** argv) {
    int n = 10;
    bool by_byte = false;
    std::string path;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-n" && i + 1 < argc) n = atoi(argv[++i]);
        else if (a == "-c" && i + 1 < argc) { n = atoi(argv[++i]); by_byte = true; }
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx head <file> [-n lines | -c bytes]\n"
                   "  -n N  前 N 行 (默认 10)\n"
                   "  -c N  前 N 字节\n");
            return 0;
        } else path = a;
    }
    if (path.empty()) { fprintf(stderr, "error: missing file\n"); return 1; }
    std::vector<uint8_t> data;
    if (!read_file(path, data)) { fprintf(stderr, "error: cannot read %s\n", path.c_str()); return 1; }

    if (by_byte) {
        size_t sz = std::min((size_t)n, data.size());
        fwrite(data.data(), 1, sz, stdout);
        if (sz < data.size()) printf("\n... (%zu more bytes)\n", data.size() - sz);
    } else {
        int lines = 0;
        for (size_t i = 0; i < data.size() && lines < n; ++i) {
            putchar(data[i]);
            if (data[i] == '\n') ++lines;
        }
        // 统计总行数
        int total = 0;
        for (uint8_t c : data) if (c == '\n') ++total;
        if (data.empty() || data.back() != '\n') putchar('\n');
        printf("... (%d lines shown, %d total)\n", lines, total);
    }
    return 0;
}

// ===================== lines =====================
// 按行号范围读取，带行号输出
int cmd_lines(int argc, char** argv) {
    size_t start = 1, end = 50;
    std::string path;
    bool no_num = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-no-num") no_num = true;
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx lines <file> [start] [end] [-no-num]\n"
                   "  输出第 start 到 end 行 (1-based, 默认 1-50)\n"
                   "  -no-num  不显示行号\n");
            return 0;
        } else if (path.empty()) path = a;
        else if (start == 1 && end == 50) { start = atoi(a.c_str()); end = start; }
        else if (end == start) end = atoi(a.c_str());
    }
    if (path.empty()) { fprintf(stderr, "error: missing file\n"); return 1; }
    if (end < start) end = start;
    std::string text;
    if (!read_text(path, text)) { fprintf(stderr, "error: cannot read %s\n", path.c_str()); return 1; }

    size_t line_no = 1, start_off = 0;
    // 找到 start 行
    while (line_no < start && start_off < text.size()) {
        if (text[start_off] == '\n') ++line_no;
        ++start_off;
    }
    if (line_no < start) { fprintf(stderr, "file has only %zu lines\n", line_no); return 0; }

    // 输出 start 到 end 行
    size_t off = start_off;
    for (size_t ln = start; ln <= end && off < text.size(); ++ln) {
        size_t e = off;
        while (e < text.size() && text[e] != '\n') ++e;
        if (!no_num) printf("%6zu| ", ln);
        fwrite(text.data() + off, 1, e - off, stdout);
        putchar('\n');
        off = (e < text.size()) ? e + 1 : e;
    }
    return 0;
}

// ===================== hex =====================
int cmd_hex(int argc, char** argv) {
    size_t bytes = 256;
    size_t offset = 0;
    std::string path;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-n" && i + 1 < argc) bytes = atoi(argv[++i]);
        else if (a == "-o" && i + 1 < argc) offset = atoi(argv[++i]);
        else if (a == "-h" || a == "--help") {
            printf("usage: tbx hex <file> [-n bytes] [-o offset]\n"
                   "  -n N  显示 N 字节 (默认 256)\n"
                   "  -o N  起始偏移 (默认 0)\n");
            return 0;
        } else path = a;
    }
    if (path.empty()) { fprintf(stderr, "error: missing file\n"); return 1; }
    std::vector<uint8_t> data;
    if (!read_file(path, data)) { fprintf(stderr, "error: cannot read %s\n", path.c_str()); return 1; }
    if (offset >= data.size()) { fprintf(stderr, "offset beyond file size\n"); return 1; }
    size_t end = std::min(offset + bytes, data.size());

    for (size_t i = offset; i < end; i += 16) {
        printf("%08zx  ", i);
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < end) printf("%02x ", data[i + j]);
            else printf("   ");
            if (j == 7) printf(" ");
        }
        printf(" |");
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < end) {
                uint8_t c = data[i + j];
                putchar((c >= 0x20 && c < 0x7F) ? c : '.');
            }
        }
        printf("|\n");
    }
    printf("\n%zu bytes shown (%zu total)\n", end - offset, data.size());
    return 0;
}

// ===================== wc =====================
int cmd_wc(int argc, char** argv) {
    std::string path;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printf("usage: tbx wc <file>\n  统计行数、单词数、字节数\n");
            return 0;
        } else path = a;
    }
    if (path.empty()) { fprintf(stderr, "error: missing file\n"); return 1; }
    std::vector<uint8_t> data;
    if (!read_file(path, data)) { fprintf(stderr, "error: cannot read %s\n", path.c_str()); return 1; }
    long long lines = 0, words = 0, bytes = (long long)data.size();
    bool in_word = false;
    for (uint8_t c : data) {
        if (c == '\n') ++lines;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') in_word = false;
        else if (!in_word) { in_word = true; ++words; }
    }
    printf("%8lld  %8lld  %8lld  %s\n", lines, words, bytes, path.c_str());
    return 0;
}

} // namespace tbx
