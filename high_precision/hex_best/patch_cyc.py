p = "div_base16_cycfix.cpp"
s = open(p).read()
old = 'bool allow_cyclic = (std::getenv("NOCYCLIC") == nullptr);'
new = (
    'bool allow_cyclic = (std::getenv("NOCYCLIC") == nullptr) && (mu_in >= 64);'
    '  // 治本修复: 自然 mu_in<64 禁用 cyclic (unwrap 对抗余数深 wrap 出错, 见 L6602-6604)'
)
assert old in s, "OLD NOT FOUND"
s = s.replace(old, new, 1)
open(p, "w").write(s)
print("CYCFIX PATCHED")
