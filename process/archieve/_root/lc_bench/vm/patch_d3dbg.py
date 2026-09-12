# D3 debug: print est_blocks + use_cyclic each absDivMu invocation to stderr.
SRC = "/home/azzr/divbench/src/div_D3.cpp"
def rd(p):
    with open(p) as f: return f.read()
s = rd(SRC)
anchor = "            use_cyclic = (cyclic_m < in + len2);\n"
assert s.count(anchor) == 1, ("anchor", s.count(anchor))
ins = anchor + '            fprintf(stderr, "EB=%zu CYCLIC=%d\\n", est_blocks, (int)use_cyclic);\n'
s = s.replace(anchor, ins, 1)
with open(SRC, "w") as f: f.write(s)
print("PATCH_D3DBG OK")
