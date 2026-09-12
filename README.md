# precious_speed

Library Checker 高精度 (big_integer) 六子题冲榜工程 —— **六题现行 #1** 的结算归档。

## 目录结构

```
BEST_20260913/   六题现行最优提交原件 (国玺)
high_precision/  高精度主线: 最优前身、hex 线、DEC 家族、FFT/SoA 实验、方法论文档
process/         工作过程: 优化器、编译实验、VM 工具链、测试数据、根目录散件
```

## 现行 #1 提交编号 (yosupo Library Checker)

| 子题 | Submission | 文件 |
|---|---|---|
| addition_of_big_integers | 390082 | `BEST_20260913/ADD.CPP` |
| addition_of_hex_big_integers | 391745 | `BEST_20260913/ADD HEX.CPP` |
| multiplication_of_big_integers | 389886 | `BEST_20260913/MUL.CPP` |
| multiplication_of_hex_big_integers | 391969 | `BEST_20260913/MUL HEX.CPP` |
| division_of_big_integers | 391180 | `BEST_20260913/DIV.CPP` |
| division_of_hex_big_integers | 395390 | `BEST_20260913/DIV HEX.CPP` |

## 成绩

六十天暑期冲刺 (2026-06 中旬 ~ 2026-08 下旬)，跨六个模型协作 (GLM 5.1→5.3 / HY / DeepSeek / Claude Opus / Haiku)，
将 DEC 线加法提速至 2×、乘法约 1.4×、除法约 4~8×；HEX 线三题最优 9/17/47ms，把前任 #1 全部挑落。

方法论文档见 `high_precision/docs/` (ZEN3_CACHE_SIM_ON_INTEL_VM.md、ROADMAP_HEX_DIV_LIMIT.md 等)；
工程史与决策记录见 `process/archieve/`。

---
*2026-09-13 结算。帝国建成，剑已归鞘。*
