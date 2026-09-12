import io, os
D = r"D:\hex_precious_speed\work\mul"

OLD_Q = """        std::vector<u32> q;
        u32 cached = 0;
        const u32* get(u32 m) {
            if (cached == m && !q.empty()) return q.data();
            const int bits = 31 - __builtin_clz(m);
            std::vector<u32> rv(m);
            for (u32 p = 0; p < m; ++p) {
                u32 r = 0;
                for (int i = 0; i < bits; ++i) if ((p >> i) & 1u) r |= 1u << (bits - 1 - i);
                rv[p] = r;
            }
            q.assign(m, 0);
            for (u32 p = 0; p < m; ++p) q[p] = rv[(1u + m - rv[p]) & (m - 1)];
"""

NEW_Q = """        std::vector<u32> q;
        std::vector<u32> rv;
        u32 cached = 0;
        const u32* get(u32 m) {
            if (cached == m && !q.empty()) return q.data();
            const u32 bits = (u32)(31 - __builtin_clz(m));
            rv.resize(m); q.resize(m);
            rv[0] = 0;
            for (u32 p = 1; p < m; ++p) rv[p] = (rv[p >> 1] >> 1) | ((p & 1u) << (bits - 1));
            for (u32 p = 0; p < m; ++p) q[p] = rv[(1u + m - rv[p]) & (m - 1)];
"""

def rd(p):
    return io.open(os.path.join(D, p), encoding="utf-8").read()
def wr(p, s):
    io.open(os.path.join(D, p), "w", encoding="utf-8", newline="\n").write(s)

v16 = rd("v16.cpp")
v18 = rd("v18.cpp")

assert OLD_Q in v16, "OLD_Q not in v16"
assert NEW_Q in v18, "NEW_Q not in v18"

# v18q = v16 + only MR_Q fix
wr("v18q.cpp", v16.replace(OLD_Q, NEW_Q))
# v18t = v18 - MR_Q fix (only MR_Tw two-level tables)
wr("v18t.cpp", v18.replace(NEW_Q, OLD_Q))
print("written v18q.cpp / v18t.cpp")
