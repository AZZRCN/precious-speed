SRC = "/home/azzr/addbench/src/add_A6c_lc.cpp"

def rd(p):
    with open(p) as f:
        return f.read()

s = rd(SRC)

# 1) 输入缓冲: 数组 -> 堆指针 + 大页尺寸常量
a1 = "    static char iBuffer[8 << 20];  // 8MB (LC 总输入 ≤ 4MB)"
b1 = ("    static constexpr size_t IBUF_SIZE = 8 << 20;  // 8MB (LC 总输入 ≤ 4MB)\n"
      "    static char* iBuffer = nullptr;  // 堆分配 + 大页, 降低 TLB miss")
assert s.count(a1) == 1, ("a1", s.count(a1))
s = s.replace(a1, b1, 1)

# 2) 文件 mmap 路径: 追加 MADV_HUGEPAGE 提示
a2 = ("                // Hint kernel: sequential access + prefetch (reduces TLB miss / page fault stall)\n"
      "                madvise(p, status.st_size, MADV_SEQUENTIAL | MADV_WILLNEED);")
b2 = (a2 + "\n                madvise(p, status.st_size, MADV_HUGEPAGE);")
assert s.count(a2) == 1, ("a2", s.count(a2))
s = s.replace(a2, b2, 1)

# 3) fread 回退路径: 先以大页堆缓冲分配, 再用 IBUF_SIZE 限制 fread
a3 = ("        size_t n = std::fread(iBuffer, 1, sizeof(iBuffer) - 1, stdin);\n"
      "        iBuffer[n] = '\\n';  // 哨兵, 确保 SWAR 能终止\n"
      "        iCursor = iBuffer;\n"
      "        iEnd = iBuffer + n;")
b3 = ("        if (iBuffer == nullptr) {\n"
      "            void *m = mmap(nullptr, IBUF_SIZE, PROT_READ | PROT_WRITE,\n"
      "                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);\n"
      "            if (m != MAP_FAILED) {\n"
      "                madvise(m, IBUF_SIZE, MADV_HUGEPAGE);\n"
      "                iBuffer = static_cast<char *>(m);\n"
      "            } else {\n"
      "                static char fb[IBUF_SIZE];\n"
      "                iBuffer = fb;\n"
      "            }\n"
      "        }\n"
      "        size_t n = std::fread(iBuffer, 1, IBUF_SIZE - 1, stdin);\n"
      "        iBuffer[n] = '\\n';  // 哨兵, 确保 SWAR 能终止\n"
      "        iCursor = iBuffer;\n"
      "        iEnd = iBuffer + n;")
assert s.count(a3) == 1, ("a3", s.count(a3))
s = s.replace(a3, b3, 1)

with open(SRC, "w") as f:
    f.write(s)
print("PATCH_LC OK")
