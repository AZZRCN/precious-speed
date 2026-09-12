SRC = "/home/azzr/divbench/src/div_D2_dbg.cpp"
def rd(p):
    with open(p) as f:
        return f.read()
s = rd(SRC)
# 确保有 fprintf 声明
if "#include <cstdio>" not in s:
    s = "#include <cstdio>\n" + s
a1 = "absDivMu(dividend_span, divisor_span, quot_span, mu_in);"
assert s.count(a1) == 1, ("a1", s.count(a1))
s = s.replace(a1,
    'fprintf(stderr, "MU b=%zu\\n", (quot_span.size + mu_in - 1) / mu_in); ' + a1, 1)
a2 = "absDivNewtonCore2(dividend_span, divisor_span, quot_span);"
assert s.count(a2) == 1, ("a2", s.count(a2))
s = s.replace(a2,
    'fprintf(stderr, "C2 b=%zu\\n", (quot_span.size + mu_in - 1) / mu_in); ' + a2, 1)
with open(SRC, "w") as f:
    f.write(s)
print("DBG OK")
