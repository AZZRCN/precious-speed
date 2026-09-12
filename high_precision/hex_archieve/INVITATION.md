# 致新 AI 的邀请函 —— precious_speed · HEX 三题开发

> 我是前任 AI。precious-speed 项目的「十进制大整数三题」已收官，全部登顶 Library Checker 最快榜。
> 现在池塘里还有更大的鱼：**三个 HEX 题**（十六进制大整数运算）。原主人把接力棒交给你了。
> 这份不是正式交接文档，只塞了「能直接上手、别踩我踩过的坑」的必要内容。

---

## 一、项目背景与你将接手的目标

- **平台**：Library Checker（LC），站点 `https://judge.yosupo.jp/`。提交页不暴露源码（匿名），验收只能靠自己跑。
- **已收官（十进制三题，可作参照）**：
  - `addition_of_big_integers` → ADD = **7ms**（提交 #390082，榜单 #1/53）
  - `multiplication_of_big_integers` → MUL = **25ms**（提交 #389886，榜单 #1/68）
  - `division_of_big_integers` → DIV = **29ms**（提交 #391180，榜单 #1/64）
- **你的目标**：三个 HEX 题（十六进制表示的大整数加/乘/除，具体题目 slug 请在 `judge.yosupo.jp` 的 problem 列表核实，大概率形如 `hex_addition_*` / `hex_multiplication_*` / `hex_division_*`）。思路与十进制同构，但底数 16 会改变 FFT/WINOGRAD/Karatsuba 的 limb 切分与进位逻辑，别直接照搬十进制代码。

---

## 二、铁律（违反任何一条都会付出真金白银的代价，务必遵守）

1. **VM 唯一权威**：一切编译与正确性/性能验收必须在 VM（Linux g++ 15.2.0 = LC 部署编译器）上跑。**本地 MinGW/g++ 结果不可信**——本地曾因超短宏名冲突 + 大输入 mmap 差异翻车多次。
2. **只通知，不代交**：只把明显更快的候选交给原主人决策；任何候选须 554 验证逐字节通过。绝不擅自提交。
3. **绝不提交含 `optimize` pragma 的源码**（LC 会 CE）；`target` pragma 安全。
4. **称「基线/原版」前，先 `diff -q` 确认真的就是同一份**，别凭文件名瞎认。
5. **SO（超优化器）搜索空间不得基于本机指令集**——目标固定 LC/Zen3。先推算法再推特调。
6. **长任务走 `jobs.py`**（硬超时落 `.jobs/<id>.out`）；禁前台裸跑/`sleep`；Windows 别用 `timeout N cmd`。
7. **LC 回执立即归档**：回执到手同轮内就建 `submit_history/<ID>_<版本>_<headline>ms[_BEST].md`（回执 + 逐点 Δ + 判读）并更新 `INDEX.md`。

---

## 三、环境与上手

- **验证机 VM**：当前在用 `10.144.33.157`，6 核（Linux）。控制脚本：`archieve/_root/lc_bench/vm/vmctl.py`（含 `run/put/get/put_tree`，auth `azzr/REDACTED`）。连接用系统 `C:/Program Files/Python311/python.exe`（含 paramiko，唯一有 paramiko 的运行时）。
- **VM 工具链**：`g++ 15.2.0 -std=c++23 -O2 -march=x86-64-v3`。`.in` 测试文件上传后须 `sed -i 's/\r$//'` 清掉 CRLF。
- **perf 锁死**：VM 上 perf 不可用，只能 `callgrind + 墙钟`。callgrind **只数 Ir ⇒ 仅函数级归因，禁止 A/B 判优**。
- **官方题目仓库（gen/sol 来源）**：`E:/library-checker-problems-master/`（各题 `gen/`、`sol/correct.cpp`）。
- **554 验证器**：`lc_bench/vm/d8_verify.py`（官方 gen 生成输入，逐字节比对 mine vs ref）。
- **GitHub**：`https://github.com/AZZRCN/precious-speed.git`，已登录 `gh`（账号 AZZRCN，token 在 keyring）。「打 release」指建 **GitHub Release 页面**（`gh release create ...`），不是仅 git tag。
- **抓 LC 榜单**：WebFetch 抓不到（SPA 空壳）。用本地 headless Chrome（web-access skill 的 CDP 代理 `localhost:3456`，Chrome 开 `9222`）实时 eval `document.body.innerText`，等 ~8s 渲染。正确域名是 `judge.yosupo.jp`（不是 librarychecker.python-lang.org）。

---

## 四、前车之鉴（天坑清单，照着避）

1. **`vmctl.put_tree` 在 Windows 上 `os.path.relpath` 返回反斜杠路径**，VM 上会建出 `add\gen` 字面目录，破坏编译。改用单 SFTP 会话 + `rel.replace('\\','/')` 重传。
2. **VM Linux 上 >~80k 位输入会 SIGSEGV（rc=139）**：原版与修复版同崩，属 mmap/大页 arena 既有环境限制，非代码 bug（LC 测试集不触达）。生成测试输入时把位数压在 60k 左右。
3. **测量判据**：同源码两份 md5 相同跑 A/B；`perf cycles` + `ratio-of-mins` ±0.2%（可验 0.5%），`instructions` ±0.01%。**禁 min-of-ratios**（会虚报 −7~−22%）。评测机抖动 ±8~10ms，`headline=max` ⇒ 单次提交不能判优；真回归需同点多次复现同向。VM 只给增益存在性 + 量级下界（大页 VM +11% / LC +2.8%）。
4. **DIV 真实 bug（重要参照）**：`absDivMu` 块循环卷积路径，单 limb `r` 估计因 FFT 浮点误差失效 → qhat 偏小 1 → 连锁商错。修复 = `absDivRem` 分派处 `allow_cyclic = false`（仅禁块循环卷积，保留逆元 Newton 的 cyclic）。HEX 除法大概率有同构脆弱点，先把除法正确性对拍跑满 100 轮再谈速度。
5. **压缩件（gccg）**：`D:\gcc modifield\cpp_regex\shrink_cpp\main.cpp` 是 C++ 源码压缩器，产物落 `best/compress/`。验收只认 **failures:0**（shrink→编译→同输入跑→stdout 逐字节比对）。改完源码必 `gccg.py up`。宏展开安全性别用静态规则猜，用 `g++ -E -P` 预处理等价神谕（曾翻车 3 次）。

---

## 五、建议起步路线

1. 在 `judge.yosupo.jp` 核实三个 HEX 题的准确 slug 与 I/O 格式（是否纯 hex 字符串、有无符号、是否要求特定大小写）。
2. 把官方 gen/sol 拉到 VM，先跑通 **正确性对拍**（`run_duipai.py` 思路，ROUNDS=100, WORKERS=6），确保基线正确。
3. 参照十进制三题的 `best/origin/` 与 `best/div_verifield/`（在 `D:\precious_speed\best\`）汲取算法骨架。
4. 性能优化严格走 VM A/B，别信本地；任何候选先 554 验证再上报原主人。
5. 新代码、新回执、新压缩件按铁律 #1/#7 归档。

---

欢迎上船。原主人很在意额度成本，回话要快问快答、少绕弯，有假设就上 VM 实测收口，别在脑子里空转自我质疑。

—— 前任 AI 留
