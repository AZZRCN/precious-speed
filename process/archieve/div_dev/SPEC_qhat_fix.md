# SPEC: qhat 溢出修复 + 红蓝对抗流程

## 背景

absDivNewtonWithInvFast 中 `dividend.size = k` 截断 BUG 修复后，引入了 qhat 溢出 carry 传播代码。
carry 传播上界 `quotient.size + k + 1` 无数学依据，可能越界。需以 GMP 源码为金标准重新设计。

## 金标准

E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\mu_divappr_q.c
E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\invertappr.c

## 修复流程（一步一记录）

### Step 1: 阅读 GMP 源码
- [ ] mu_divappr_q.c: qhat 修正逻辑 (ceil 逆 vs floor 逆)
- [ ] invertappr.c: Newton 逆的 floor/ceil 保证
- [ ] 记录 GMP 如何避免 qhat 溢出

### Step 2: 对照 moptm
- [ ] absInvNewton 产生的逆是 floor 还是 ceil?
- [ ] absDivNewtonWithInvFast 的两个 while 循环对应 GMP 哪段?
- [ ] qhat 溢出的数学条件是什么?

### Step 3: 设计修复
- [ ] 方案A: 改用 ceil 逆 (inv 偏大), 只需第一个 while (减divisor)
- [ ] 方案B: 保持 floor 逆, 安全处理溢出 (需要调用方提供 carry 目标)
- [ ] 方案C: 预处理保证每块商 < B^k
- [ ] 记录选型理由

### Step 4: 实施修复
- [ ] 修改代码
- [ ] 编译验证 (ai.bat compile_mod)

### Step 5: 代码审查 (重复直到完美)
- [ ] 第1轮审查: 重读修复代码, 记录问题
- [ ] 第2轮审查: ...
- [ ] 第N轮审查: 无吐槽 → 进入测试

### Step 6: 红蓝对抗测试 (5轮起)
- [ ] 红队生成: 针对 qhat 溢出边界、块边界进位、余数高位
- [ ] 蓝队验证: cur_mod.exe
- [ ] 记录每轮结果
- [ ] 最后一轮无失败 → 通过

### Step 7: 全路径独立性验证
- [ ] absDivBasicCore (小规模)
- [ ] absDivNewtonCore1 (len1 < 2*len2)
- [ ] absDivNewtonCore2 (多块)
- [ ] absDivMu (GMP mu 风格)
- [ ] 每条路径独立正确

## 记录区

### Step 1 记录: GMP 源码分析

**GMP mu_div_qr.c 关键设计**:
- L257-L261 预处理: `qh = mpn_cmp(np, dp, dn) >= 0; if (qh) sub_n(rp, np, dp, dn)` → 保证 rp < dp
- L278-L280 qhat 计算: `mpn_mul_n(tp, rp+dn-in, ip, in); cy = mpn_add_n(qp, tp+in, rp+dn-in, in); ASSERT_ALWAYS(cy == 0)`
  - **GMP assert qhat 不溢出!** 因为 rp < dp → rp_high < dp_high → qhat < B^in
- L325-L334 修正: `while(r != 0) { mpn_incr_u(qp, 1); sub_n(rp, rp, dp, dn); r -= cy; }` — 只加不减
- L335-L341 最终检查: `if (mpn_cmp(rp, dp, dn) >= 0) { mpn_incr_u(qp, 1); sub_n(rp, rp, dp, dn); }`
- **没有 qhat 溢出处理!** GMP 数学保证 qhat < B^in

**mu_divappr_q.c 区别**: 最后 `mpn_add_1(qp, qp, qn, 3)` — 产生近似商(偏大), 不是精确商

### Step 2 记录: moptm 数学分析

**absDivRem 预处理** (L4237-L4246):
```
high = dividend[quot_len-1 .. len1-1]  // 最高 len2 位
if (high >= divisor) { quotient[quot_len-1] = 1; high -= divisor; }
```
→ 保证 high < divisor

**Core2 第一个块**:
- len1 = len2*blocks + r (0 < r < len2)
- dividend = dividend[len1_rem-len2 .. len1-1], size = len2+r
- divid_high = dividend[len2-1 .. len2+r-1], size = r+1
- high = dividend[len1-len2 .. len1-1] = dividend[len2*(blocks-1)+r .. len2*blocks+r-1]
- divid_high 最低位 = len2*blocks-1, high 最低位 = len2*blocks-len2+r
- 当 r <= len2-1: divid_high ⊂ high 或 divid_high = high → **divid_high < divisor**

**qhat 不溢出证明**:
- qhat ≈ divid_high * B^(k-1) / divisor (k=len2)
- divid_high < divisor → qhat < B^(k-1) = B^r = B^quotient.size
- **qhat 不会溢出!**

**循环块**:
- 前一块保证 remainder < divisor
- 下一块 dividend = [remainder, new_chunk]
- divid_high = [remainder 最高 1 位, new_chunk], size = k+1
- dividend = remainder * B^k + new_chunk < divisor * B^k + B^k = (divisor+1)*B^k
- floor(dividend/divisor) < B^k + B^k/divisor <= B^k + B
- 但 remainder < divisor → dividend < B^k * divisor → **floor < B^k, qhat 不溢出!**

### Step 3 记录: 修复方案

**结论: qhat 溢出处理代码是多余的!** 数学证明 divid_high < divisor → qhat < B^k.

**修复**: 删除草台班子 carry 传播代码, 恢复 assert:
```cpp
while (absCompare(dividend, divisor) >= 0) { ... }  // 修正 qhat 偏小
dividend.size = k;
assert(qhat_span[qhat_span.size - 1] == 0);  // 数学保证不溢出
qhat_span.size--;
std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
```

同时保留截断 BUG 修复 (先 while 再截断).

## 命令

所有命令通过 ai.bat 或衍生脚本执行:
- ai.bat compile_mod     编译 div_modular.cpp
- ai.bat test_mod        测试 cur_mod.exe
- ai_agent_redblue.bat   红蓝对抗测试
