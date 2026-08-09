# -*- coding: utf-8 -*-
"""按功能桶归因 callgrind 的 SELF 成本 (Ir/D1mr/DLmr)。

正确处理 callgrind 格式的两个坑:
  1. 函数名压缩: 名字可能首次出现在 cfn=(id) name 行, 必须一并入表
  2. calls=N 之后紧跟的那一行是被调用者的 INCLUSIVE 成本, 不能计入调用者 self

回答: headline 用例里 FFT / 进制转换IO / 线性算术 各占多少?
用法(远端): python3 attrib.py <case>
"""
import re
import sys
import collections

CASE = sys.argv[1] if len(sys.argv) > 1 else "length_ratio_integer_00"
path = "/tmp/cgz_%s.out" % CASE


def bucket(fn):
    f = fn.lower()
    for k in ("difc4", "iditc4", "3stage", "butterfly", "fftcore", "smallfft",
              "radix", "::dif", "::idit", " dif<", " idit<", "fft::"):
        if k in f:
            return "FFT-kernel"
    for k in ("preparedft", "fftmul", "pointwise", "dftcache", "fft_ceil", "twiddle",
              "rootstable", "bitrev", "carryprop"):
        if k in f:
            return "FFT-support"
    for k in ("tostring", "fromstring", "parse", "read", "write", "print", "scan",
              "output", "input", "itoa", "atoi", "ostream", "istream", "streambuf",
              "putchar", "getchar", "fwrite", "fread", "memcpy", "memset", "strlen",
              "to_chars", "from_chars", "digits", "operator>>", "operator<<"):
        if k in f:
            return "IO/convert"
    for k in ("abssub", "absadd", "abscmp", "abscompare", "absmul1", "submul",
              "addmul", "count_true", "normalize", "shift"):
        if k in f:
            return "linear-arith"
    for k in ("absinv", "newton", "absdiv", "divmu", "basiccore"):
        if k in f:
            return "div-control"
    return "other"


names = {}
agg = collections.defaultdict(lambda: [0, 0, 0])
events = None
cur = None
skip_next_cost = False

with open(path) as fh:
    for line in fh:
        line = line.rstrip("\n")
        if line.startswith("events:"):
            events = line.split()[1:]
            continue

        m = re.match(r"^(c?fn)=\((\d+)\)(?:\s+(.*))?$", line)
        if m:
            kind, fid, nm = m.group(1), m.group(2), m.group(3)
            if nm:
                names[fid] = nm
            if kind == "fn":
                cur = names.get(fid, "?" + fid)
                skip_next_cost = False
            continue

        if line.startswith("calls="):
            skip_next_cost = True
            continue

        if line.startswith(("cfl=", "fl=", "fi=", "fe=", "ob=", "cob=", "jump", "jcnd")):
            continue

        if cur and line and (line[0].isdigit() or line[0] in "+-*"):
            if skip_next_cost:
                skip_next_cost = False   # 这一行是 callee inclusive, 丢弃
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                vals = [int(x) for x in parts[1:]]
            except ValueError:
                continue
            b = agg[cur]
            if vals:
                b[0] += vals[0]
            if events:
                for i, e in enumerate(events):
                    if i >= len(vals):
                        break
                    if e == "D1mr":
                        b[1] += vals[i]
                    elif e in ("DLmr", "D2mr"):
                        b[2] += vals[i]

tot = sum(v[0] for v in agg.values()) or 1
totd1 = sum(v[1] for v in agg.values())
totdl = sum(v[2] for v in agg.values())
print("CASE=%s   SELF total: Ir=%.1fM  D1mr=%.2fM  DLmr=%.3fM   est_cycles=%.1fM"
      % (CASE, tot / 1e6, totd1 / 1e6, totdl / 1e6,
         (tot + 5 * totd1 + 200 * totdl) / 1e6))
print()
print("--- TOP 20 functions by SELF Ir ---")
print("%-52s %9s %8s %10s  %s" % ("function", "Ir(M)", "%", "D1mr(K)", "bucket"))
for fn, v in sorted(agg.items(), key=lambda kv: -kv[1][0])[:20]:
    print("%-52s %9.1f %7.2f%% %10.0f  %s"
          % (fn[:52], v[0] / 1e6, 100.0 * v[0] / tot, v[1] / 1e3, bucket(fn)))

print()
print("--- BUCKETS (self) ---")
bagg = collections.defaultdict(lambda: [0, 0, 0])
for fn, v in agg.items():
    b = bagg[bucket(fn)]
    for i in range(3):
        b[i] += v[i]
for b, v in sorted(bagg.items(), key=lambda kv: -kv[1][0]):
    est = v[0] + 5 * v[1] + 200 * v[2]
    print("%-14s Ir=%8.1fM %6.2f%%  D1mr=%8.0fK  DLmr=%7.0fK  est=%8.1fM %6.2f%%"
          % (b, v[0] / 1e6, 100.0 * v[0] / tot, v[1] / 1e3, v[2] / 1e3,
             est / 1e6, 100.0 * est / (tot + 5 * totd1 + 200 * totdl)))
