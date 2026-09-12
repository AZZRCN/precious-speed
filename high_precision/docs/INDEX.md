# 393027_opt.cpp 函数索引（导航用，验证时勿读此文件）

> 定位契约：所有行号基于本文件当前版本。改代码用本索引找位置；**验证（oracle 字节相等 + perf instructions:u）只跑真实二进制，不回读本索引**。
> 当前最优交付：cyclic 版（CYC=1 默认开），oracle 26+46 全 0-diff，~40ms 级，已到算法地板。

## 0. 头部 / 常量 / 全局（行 1–215）

- `L1–37` 注释块：LC 回执 + 头部声明（含 `// 喵喵喵~` 必留行）。
- `L50` `#pragma GCC target("avx2,fma,bmi,bmi2,popcnt,lzcnt")` —— 仅 target，无 optimize。
- `L64` `HP = 1u<<21`（2MiB 巨页）；`L66` `hugify()` 显式 madvise 大页，消 minor fault。
- `L99–107` 巨缓冲（全部 `alignas(HP)`）：`inbuf_/outbuf/A/B/Qout/Rout/AN/BN/AS/BS/VB/WORK`。
- `L111` `FB/LMMAX GB/LMMAX` FFT 双缓冲（double）。
- `L197–215` 复数工具：`cmul/cmulconj/cmulspec/cscale`、`cpx` 类型。`twbase[L202]` 旋转因子表（16KiB）。
- 关键常数（可 `-D` 覆盖，**纯旋钮零正确性风险**）：
  - `MULBF_MAX`（L≈90 附近）schoolbook 阈值，仅 <48 limb 小乘积生效。
  - `FFT_LEAF_LOG`（L≈192）FFT 递归叶大小 → **#0 主旋钮，已实测 LL=8 最优**。
  - `INV_BASE`（L1387 `#ifndef`）invertappr 基例，已实测 32–128 全平。
  - `BZ_MIN` / `KD_QMAX` / `BZ_CUTOFF` / `BARRETT_NMIN` 分发阈值。

## 1. 解析 / I/O（行 123–204）

- `L129 hexfull` / `L135 hexpart` / `L142 hex16_store`：十六进制 limb 读写。
- `L153 tok_len`：单 token 长度；`L166 parse_limbs`：字符串→u64 limb 数组。
- `L175 put_u64` / `L180 put_big`：limb 数组→十六进制输出。

## 2. FFT 内核（行 216–610）—— split-radix 重写的核心战场

### 2.1 twiddle 表与蝶形
- `L216 resize(n)`：按复数点数 n 重建 `twbase`（两级表，halfSize=√n）。**改 FFT 大小时必须同步重建**。
- `L230 twg(i)`：两级查表取 W_i；`L234 twlo`/`L235 twhi`：循环外提高位分量。
- `L237 mulI(z)`：`[re,im]→[-im,re]`（radix-2^2 顶层用）。
- `L241 bfPlain` / `L255 bfFwd` / `L273 bfInv`：radix-2 单蝶（标量尾 + AVX2 主循环）。
- `L294–310` radix-2^2 融合蝶形助手：`cmul4/cmulv/cmulconj4/cmulconjv/mulI4`（中间结果留寄存器）。
- `L316 bf2Fwd` / `L345 bf2FwdOne` / `L373 bf2Inv` / `L400 bf2InvOne`：radix-2^2 顶层蝶（w0==w1==1 时走 *One 省旋转因子）。

### 2.2 递归 radix-4（**pointwise 强依赖其输出排列**）
- `L426 difFlat` / `L460 ditFlat`：iterative radix-4 蝶（叶级，含 halfSize 跨边界判定）。
- `L486 difRec(d,n,bb)`：**递归 radix-4 DIF**，叶 `n<=2^FFT_LEAF_LOG` 落 difFlat；输出为 **base-4 位反转序**，位置 `bb` 编码 radix-4 反转位。
- `L530 ditRec(d,n,bb)`：递归 radix-4 DIT，对称结构。
- `L500 difZeroHiTop` / `L522 difRecZeroHi`：zero-hi 优化——实数缓冲上半为零时顶层只读下半（省 1/2 读流量）。**仅在纯 2 幂路径、且 `path==2 && ts>2^FFT_LEAF_LOG` 时启用**（见 mul_fft L949）。**split-radix 若算 full DFT 会丢掉此优化**。

### 2.3 pointwise（**合同核心**）
- `L541 pointwise(F,G,n)`：复数卷积点乘。**强依赖 difRec/ditRec 的 base-4 反转序**：用 `twg(f>>1)` 与共轭配对 `(f, b=2*bs-1-f)` 实现 RIRI 对称，只算半谱。位置 `F[0]`、`F[1]` 有特例（DC/Nyquist 用 `cmulspec`）。
  → **任何 FFT 内核替换（split-radix/4-step）必须逐位等于原 base-4 反转序，否则 pointwise 错**。
- `L584 pointwiseSq`：自卷积点乘（same 操作数），对称简化。
- `L789 pointwise_blk` / `L810 pointwise_cross` / `L829 pointwise_mixed`：混合 radix（3/5 路径）点乘，复用 conjugate-symmetry 归一化。

### 2.4 混合 radix 顶层（行 603–841）
- `L619 MR_Tw::build` / `ensure3` / `ensure5`：3/5 路径两级旋转因子表（O(√m)）。
- `L660 dif3StageR` / `idit3StageR` / `dif5StageR` / `idit5StageR`：3/5 混合基顶层（移植自前人实数 FFT，复数 DFT 约定）。
- `L775 get(m)` 取混合基 twiddle。
- **注意**：混合路径顶层未做半区跳过 → zero-hi 仅纯 2 幂路径可用。

## 3. 乘法层（行 845–1207）

- `L845 split_b2(src,g,n,k,zlim)`：u64 limb → k-bit 数字 double 序列；向量 gather + 标量尾；仅清 `[total,zlim)`。返回非零 digit 数 total。
- `L882 merge_b2(f,g,n,k)`：FFT 输出 double → 进位合并回 u64 limb。
- `L896 fft_ceil_tiers(c)` / `L912 fft_len_for(u,k)` / `L916 pick_k(u)`：FFT 长度选择（2^k / 3·2^(k-2) / 5·2^(k-3) 混合档，恒 ≤ next_pow2，精度预算 2^(2k)·lm ≤ 2^48 天然不破）。**已近最优，再拧有精度风险**。
- `L926 mul_bf`：schoolbook 乘法（nb≤MULBF_MAX）。
- `L938 mul_fft(a,na,b,nb,c)`：**主 FFT 乘法**。流程：`u=na+nb` → `pick_k` → `fft_len_for` → `path = (lm%3?3:(lm%5?5:2))` → split_b2 两操作数 → 按 path 调 difRec/ditRec + pointwise → merge_b2。**零-hi 判定 L949**：`deep=(path==2)&&ts>2^FFT_LEAF_LOG`。
- `L1014 mulg(a,na,b,nb,c)`：入口分派，`nb<=MULBF_MAX`→mul_bf 否则 mul_fft；na<nb 交换。
- `L1026–1096 FixedFFT`（fm_prep/fm_mul）：**固定乘数 FFT 复用**（Barrett 块循环里 q2/BS 正变换只做一次）。`g_fmt`（L1029，FMT 开关）切混合档/旧纯 2 幂。
- `L1066 fm_mul`：调用方已 split 好 a，复用预变换 G。
- `L1106–1207 CycFFT`（pick_cyclic/cyc_prep/cyc_mul_fixed）：**环形固定乘数 mod B^mc−1**（DEC 相对 HEX 的核心杠杆，半个线性长度）。`g_cyc`（L1108，CYC 开关，默认 1）。`cyc_mul_fixed` 内含 `B^mc≡1` 回绕进位。

## 4. 约简 / limb 运算（行 1211–1250）

- `L1211 reduce_mod_bm1(Z,len,mc,zc)`：`Z mod (2^(64mc)−1)` 折半（环约简），全 1 代表 0。`g_cyc` 路径用。
- `L1236 cmpn` / `L1241 addn` / `L1246 subn`：limb 比较/加减（_addcarry_u64）。

## 5. 除法算法（行 1253–1725）

- `L1253 knuthD(U,mn,V,n,Qo,Ro)`：Knuth Algorithm D（schoolbook 长除），小商/极短商走这里。
- `L1330 div_2n_1n` / `L1351 div_3n_2n`：BZ 递归基例（A:2n/B:n 与 A:3n/B:2n），内部 `mulg` 走 FFT。
- `L1389 invertappr(d,n,v)`：**Newton 倒数**，~2.5·M(n)，远低于 div_2n_1n 的 ~9·M(n)。
  - `n<=INV_BASE`：knuthD 求 1/d。
  - 递归：`xh=invertappr(d_hi,h)` → `W=d*(B^n+xh*B^l)` → `E=B^{2n}−W` → `v=xh*B^l+Ehi+floor(Ehi*xh/B^h)`。
  - **关键**：L1412 `mulg(d,n,xh,h,T)` 与 L1435 `mulg(Ehi,ne,xh,h,P)` 两路乘积——`_P[h+i]` 仅取 high-half（L1438–1448 累加 `P[h+i]` + `Ehi[i]`）。前者 `d*xh` 是 full product，后者 high-half 可截断机会在 `P[h+i]`。
- `L1456 bz_divide(Ai,na,Bi,nb,Q,R)`：**顶层 Burnikel–Ziegler**。块长 n=j·m；归一化 shift sigma；分块迭代。
  - `t>=3 || n>=BARRETT_NMIN`（L1507）：Barrett 路径——`invertappr(BS)` 求倒数 → `fm_prep` 两固定乘数(q2,BS) + `cyc_prep` 环形 BS → 每块 2 次 n·n 乘法（环形 or 线性，由 `use_cyc=CY2.ok && CY2.lm<FF2.lm` 选）。
  - 否则（L1572）：走 `div_2n_1n` 递归叶。
  - **cyclic 优化受益点**：a_max_b_random、burnikel_*、部分 length_ratio（#1–5）走此路径。
- `L1611 mag_cmp` / `L1617 divrem_1`：比较、单 limb 除法（nb==1）。
- `L1633 newton_divide(a,na,d,nb,q,r)`：**Newton 主除法**（绑定 LC 的 MAX 点 `length_ratio_integer#0` 走此）。
  - 归一化 → `invertappr(dn)` → `L=an*v`（**full product，M(3n)，高低半都用，无短积空间**）→ `qe=floor(an/β^n)+floor(an*v/β^{2n})` → `QD=qe*dn`（**full product，M(2n)**）→ `Rt=an−QD` → 至多 32 轮校正。
  - **成本主导**：`an*v` 占 newton 总成本 ~40%，是卡 MAX 地板根因；三大乘积全 full product 确认无早停空间。

## 6. main / 分发（行 1727–1833）

- `L1727 main`：hugify 所有巨缓冲 → 读 getenv 开关（FMT/BZALL/CYC）→ 读 stdin → 逐 case：
  - `L1767 la,lb<=16` → u64 除；`L1776 la<=32,lb<=16` → u128/u64；`L1802 nb==1` → divrem_1；`L1796 A<B` → 商 0 余 A。
  - `L1811` 分发：`nb<BZ_MIN || na−nb+1<=KD_QMAX` → **knuthD**；`na<=2*nb && !g_bzall` → **newton_divide**；否则 → **bz_divide**。
  - `g_bzall`（L1741，BZALL 开关，默认 0）：把 `na<=2nb` 也路由 BZ；实测对 #0 na≈2nb **指令等价（零增益）**。
- 输出 `Qout R` 十六进制。

## 7. split-radix 改动的精确触及面（若执行）

1. FFT 内核 `L486 difRec` / `L530 ditRec` + 叶 `L426 difFlat`/`L460 ditFlat` + zero-hi `L500/L522`。
2. **必须同步改 pointwise `L541`**（或保证新内核输出 = 原 base-4 反转序），否则卷积错。
3. **必须保留 zero-hi**：split-radix 若算 full DFT 会丢 `difRecZeroHi` 的半流量省（M(3n) 主导乘积回归）。
4. 混合 radix 路径（path==3/5）独立，split-radix 仅需重写纯 2 幂 `path==2` 分支（mul_fft L998、fm_mul L1089、cyc_mul_fixed L1190）。
5. 守口：SR=1 env 门控默认关；每一步 oracle 字节相等 + perf instructions:u 实测，回归立即回退。

## 8. 已实测结论（VM perf instructions:u，非推断）

- 绑定 MAX 点 = `length_ratio_integer#0` = 395.4M 指令（newton_divide，算法地板）。
- `FFT_LEAF_LOG`：LL=8 最优（7→+2.4%、9→+0.27%、10→+0.5%）。
- `INV_BASE`：32–128 全平（±0.003% 噪声）。
- `BZALL=1` 路由 #0→BZ：oracle 仍 0-diff，但 #0 指令 395.43M≈不变（na≈2nb 等价）。
- 4-step cache-aware：数学+集成双死路（EPYC 32MB L3 已容下 16MB 工作集，cache 收益存疑）。
- 全部安全旋钮零增益 → cyclic 版即当前最稳 #1 交付。
