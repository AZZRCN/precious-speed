SRC = "/home/azzr/addbench/src/add_A6d_lc.cpp"
def rd(p):
    with open(p) as f: return f.read()
s = rd(SRC)
old = """            for (; i < in1.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;"""
new = """            for (; i < in1.size; i++)
            {
                if (borrow == 0)
                {
                    break;
                }
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            if (borrow == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            }
            return borrow;"""
n = s.count(old)
assert n == 2, ("expected 2 occurrences (scalar + avx2 absSub), found", n)
s = s.replace(old, new)
with open(SRC, "w") as f: f.write(s)
print("PATCH OK, replaced", n)
