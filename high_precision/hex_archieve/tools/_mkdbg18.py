import io, os
D = r"D:\hex_precious_speed\work\mul"
s = io.open(os.path.join(D, "v18.cpp"), encoding="utf-8").read()

pairs = [
    ("    static void dif3StageR(cpx* b0, u32 m) {",
     "    static void dif3StageR(cpx* b0, u32 m) {\n        fprintf(stderr, \"D3 %u\\n\", m);"),
    ("    static void idit3StageR(cpx* b0, u32 m) {",
     "    static void idit3StageR(cpx* b0, u32 m) {\n        fprintf(stderr, \"I3 %u\\n\", m);"),
    ("    static void dif5StageR(cpx* b0, u32 m) {",
     "    static void dif5StageR(cpx* b0, u32 m) {\n        fprintf(stderr, \"D5 %u\\n\", m);"),
    ("    static void idit5StageR(cpx* b0, u32 m) {",
     "    static void idit5StageR(cpx* b0, u32 m) {\n        fprintf(stderr, \"I5 %u\\n\", m);"),
]
for a, b in pairs:
    assert a in s, a
    s = s.replace(a, b, 1)
if "#include <cstdio>" not in s:
    s = s.replace("#include <vector>", "#include <vector>\n#include <cstdio>", 1)
io.open(os.path.join(D, "v18dbg.cpp"), "w", encoding="utf-8", newline="\n").write(s)
print("ok")
