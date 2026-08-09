SRC = "/home/azzr/divbench/src/div_D2.cpp"

def rd(p):
    with open(p) as f:
        return f.read()

s = rd(SRC)

# D2: 放宽 absDivRem 分发限制。
# 原: bool ab_safe = (mu_in < len2) || ((quot_span.size + mu_in - 1) / mu_in <= 10);
# 多块场景(est_blocks>10)下, absDivMu 内部已把 cyclic_m 放大到 int_ceil2(len2+in+1),
# 使循环卷积退化为线性(精确), use_cyclic 自动关闭 -> 不存在 unwrap 累积误差。
# 故放开 est_blocks<=10 限制, 让更多大数除法走 absDivMu(复用 divisor DFT),
# 避免回退到 Core2 重复计算 FFT。单/少块行为不变。纯算法层面改动。
old = 'bool ab_safe = (mu_in < len2) || ((quot_span.size + mu_in - 1) / mu_in <= 10);'
new = ('bool ab_safe = true;  // D2: 多块(est_blocks>10)时 absDivMu 内部已把 cyclic_m 放大到 '
       'int_ceil2(len2+in+1) 使循环卷积退化为线性(精确), use_cyclic 自动关闭, '
       '故放开 est_blocks<=10 限制, 更多大数除法走 absDivMu(复用 divisor DFT)')

assert s.count(old) == 1, ("ab_safe anchor count", s.count(old))
s = s.replace(old, new, 1)

with open(SRC, "w") as f:
    f.write(s)
print("PATCH D2 OK")
