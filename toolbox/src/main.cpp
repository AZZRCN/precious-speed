// main.cpp - tbx (toolbox) 统一入口
// 子命令分发: split | sam | regex | glob | es | tree | head | lines | hex | wc
#include "common.h"

namespace tbx {
int cmd_split(int argc, char** argv);
int cmd_sam(int argc, char** argv);
int cmd_regex(int argc, char** argv);
int cmd_glob(int argc, char** argv);
int cmd_es(int argc, char** argv);
int cmd_tree(int argc, char** argv);
int cmd_head(int argc, char** argv);
int cmd_lines(int argc, char** argv);
int cmd_hex(int argc, char** argv);
int cmd_wc(int argc, char** argv);
}

static void print_usage() {
    printf(
        "tbx - AI 工具箱 (纯 C/C++ 实现)\n"
        "\n"
        "USAGE: tbx <command> [options]\n"
        "\n"
        "COMMANDS:\n"
        "  split   文本拆分分析器 (按字符类分类统计/token 化)\n"
        "  sam     SAM 后缀自动机搜索 (出现次数/位置, 通配符 * ?)\n"
        "  regex   正则表达式搜索 (行/字节模式, std::regex)\n"
        "  glob    通配符文件名搜索 (递归目录, * ? [abc])\n"
        "  es      Everything HTTP 搜索 (localhost:21465)\n"
        "  tree    目录树打印 (递归, 深度限制, 文件大小)\n"
        "  head    文件头部采样 (前 N 行/字节)\n"
        "  lines   按行号范围读取 (带行号)\n"
        "  hex     十六进制查看器\n"
        "  wc      统计行数/单词数/字节数\n"
        "\n"
        "GLOBAL OPTIONS:\n"
        "  -h, --help  显示帮助\n"
        "  -json       JSON 输出 (split/sam/regex/glob/es 支持)\n"
        "\n"
        "Run 'tbx <command> -h' for command-specific help.\n"
    );
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { print_usage(); return 0; }

    if (cmd == "split") return tbx::cmd_split(argc, argv);
    if (cmd == "sam")   return tbx::cmd_sam(argc, argv);
    if (cmd == "regex") return tbx::cmd_regex(argc, argv);
    if (cmd == "glob")  return tbx::cmd_glob(argc, argv);
    if (cmd == "es")    return tbx::cmd_es(argc, argv);
    if (cmd == "tree")  return tbx::cmd_tree(argc, argv);
    if (cmd == "head")  return tbx::cmd_head(argc, argv);
    if (cmd == "lines") return tbx::cmd_lines(argc, argv);
    if (cmd == "hex")   return tbx::cmd_hex(argc, argv);
    if (cmd == "wc")    return tbx::cmd_wc(argc, argv);

    fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    print_usage();
    return 1;
}
