import io, os
D = r"D:\hex_precious_speed\work\mul"
s = io.open(os.path.join(D, "v18.cpp"), encoding="utf-8").read()
NI = "__attribute__((noinline)) "
for f in ("dif3StageR", "idit3StageR", "dif5StageR", "idit5StageR"):
    a = f"    static void {f}(cpx* b0, u32 m) {{"
    b = f"    {NI}static void {f}(cpx* b0, u32 m) {{"
    assert a in s, a
    s = s.replace(a, b, 1)
io.open(os.path.join(D, "v19.cpp"), "w", encoding="utf-8", newline="\n").write(s)
print("v19.cpp written")
