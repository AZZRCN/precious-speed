#!/usr/bin/env python3
# 边界用例：1/4/5/8/9/16/17/25/32/64/65 位 + 负数
# ADD: a + 0 = a        (正/负 a，覆盖 writeTo 全 limb 边界)
# MUL: a * 1 = a        (正/负 a)
# DIV: a / 1 = a rem 0  (正 a；absDivRem 取绝对值，负 a 同正)
import subprocess
import sys

WORK = "/tmp/o2compare"
BINS = {"ADD": f"{WORK}/mf_ADD", "MUL": f"{WORK}/mf_MUL", "DIV": f"{WORK}/mf_DIV"}
DIGITS = [1, 4, 5, 8, 9, 16, 17, 25, 32, 64, 65]


def make_num(digits, neg=False):
    s = "1" * digits
    return ("-" + s) if neg else s


def run(bin_path, input_str):
    p = subprocess.run([bin_path], input=input_str,
                       capture_output=True, text=True, timeout=60)
    return p.stdout


def case_add():
    cases = []
    expected = []
    for n in DIGITS:
        for neg in [False, True]:
            a = make_num(n, neg)
            cases.append((a, "0"))
            expected.append(str(int(a) + 0))
    inp = f"{len(cases)}\n" + "\n".join(f"{a} {b}" for a, b in cases) + "\n"
    exp = "\n".join(expected) + "\n"
    return inp, exp


def case_mul():
    cases = []
    expected = []
    for n in DIGITS:
        for neg in [False, True]:
            a = make_num(n, neg)
            cases.append((a, "1"))
            expected.append(str(int(a) * 1))
    inp = f"{len(cases)}\n" + "\n".join(f"{a} {b}" for a, b in cases) + "\n"
    exp = "\n".join(expected) + "\n"
    return inp, exp


def case_div():
    # absDivRem 的符号约定较特殊（|a|==|b| 时 q=+1 特例；余数取绝对值），
    # 且本任务未修改 absDivRem。为避免既有的符号约定干扰 writeTo 验证，
    # DIV 仅测正 a：b=1 => q=a, r=0；b=3 => q=a//3, r=a%3（正数截断==向下取整）
    cases = []
    expected = []
    for n in DIGITS:
        a = make_num(n, False)
        cases.append((a, "1"))
        ai = int(a)
        expected.append(f"{ai // 1} {ai % 1}")
    # 非平凡除数 b=3，正 a
    for n in [5, 17, 65]:
        a = make_num(n, False)
        cases.append((a, "3"))
        ai = int(a)
        expected.append(f"{ai // 3} {ai % 3}")
    inp = f"{len(cases)}\n" + "\n".join(f"{a} {b}" for a, b in cases) + "\n"
    exp = "\n".join(expected) + "\n"
    return inp, exp


def diff_lines(exp, got, label):
    e = exp.split("\n")
    g = got.split("\n")
    if len(e) != len(g):
        print(f"  [{label}] line count mismatch: exp={len(e)} got={len(g)}")
        return False
    ok = True
    for i, (a, b) in enumerate(zip(e, g)):
        if a != b:
            print(f"  [{label}] case {i}: exp={a[:80]} got={b[:80]}")
            ok = False
    return ok


def main():
    all_ok = True
    for label, gen in (("ADD", case_add), ("MUL", case_mul), ("DIV", case_div)):
        inp, exp = gen()
        got = run(BINS[label], inp)
        ok = diff_lines(exp, got, label)
        print(f"{label}: {'OK' if ok else 'FAIL'} ({len(exp.split(chr(10)))-1} cases)")
        if not ok:
            all_ok = False
    print("ALL_PASS" if all_ok else "SOME_FAIL")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
