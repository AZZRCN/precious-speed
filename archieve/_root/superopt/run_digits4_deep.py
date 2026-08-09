"""digits4 深搜 runner —— 目标: 消除 D47 writeTo 的 40KB gather 查表。

背景 (2026-08-07):
  D47 的 Integer::writeTo 用 constexpr outTable[10000] (uint32) = 40KB 随机 gather,
  源码注释自承 "data-dependent random gather ... hardware prefetcher cannot predict",
  还得手工预取 8 个表项掩盖 L2 延迟。Zen3 L1D 仅 32KB, 该表必溢出 L1。
  TOPPROF 实测 fmt 段占比: length_ratio_integer_04 = 17.3% (4.8ms), r_nearly_zero_01 = 7.2%。

  若能搜出 <=9 条纯 ALU 指令把 x<10000 转成 4 字节 ASCII 打包, 即可整表删除。

相对 autopilot 默认跑法的两点修正:
  1) ops 补 "or" + scaled lea (lea2/lea4/lea8) —— 原 spec 只给 add/sub/mul/mulhi/shr/shl/and,
     拼字节最关键的两类指令缺席, 这是上轮 NO-WINNER 的可能真因。
  2) max_insn 6 -> 9 (上轮 6 已穷尽)。

带 ckpt: 进程被打断后重跑本脚本会从 digits4_ckpt.bin 续搜, 不重复已搜空间。
"""
import so

OPS = ["add", "sub", "mul", "mulhi", "shr", "shl", "and", "or",
       "lea2", "lea4", "lea8"]

if __name__ == "__main__":
    spec = so.make_digits4_spec(max_insn=9, K=32, tl=3600.0, ops=OPS)
    w = so.run(spec, quick_n=3000, full_n=400000, ckpt="digits4_ckpt.bin")
    print("FINAL_WINNER:", w)
