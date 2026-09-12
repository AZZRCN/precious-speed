#!/usr/bin/env python3
"""
在 Ubuntu 上用 Library Checker 官方 generator 生成测试用例，并用 hash.json 校验。

为什么必须在 Linux 上做：
  Windows 下 g++ 编译的 generator 用 printf("\\n") 输出，stdout 文本模式会把
  \\n 翻译成 \\r\\n。生成的 .in 文件是 CRLF，而 LC 判题机（Ubuntu）是 LF。
  被测程序解析时 \\r 会变成"幽灵 token"，走的根本不是判题机上的代码路径。

hash.json 是官方随仓库分发的黄金标准（每个 .in / .out 的 sha256）。
校验通过 == 本地用例与判题机逐字节一致。

用法:
  python3 gen_official.py                 # 生成三题全部用例并校验
  python3 gen_official.py mul add         # 只做指定题目
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path('/home/azzr/lcp')
COMMON = ROOT / 'common'
BIGINT = ROOT / 'big_integer'

# 与 generate.py 的 cxxflags_default 完全一致
CXXFLAGS = ['-O2', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-result']

PROBLEMS = {
    'add': 'addition_of_big_integers',
    'mul': 'multiplication_of_big_integers',
    'div': 'division_of_big_integers',
}


def parse_info(path: Path):
    """极简 info.toml 解析器，只抽 [[tests]] 段的 name / number。"""
    tests = []
    cur = None
    for line in path.read_text().splitlines():
        s = line.strip()
        if s == '[[tests]]':
            cur = {}
            tests.append(cur)
            continue
        if s.startswith('[') and s != '[[tests]]':
            cur = None
            continue
        if cur is None:
            continue
        m = re.match(r"""name\s*=\s*['"](.+?)['"]""", s)
        if m:
            cur['name'] = m.group(1)
            continue
        m = re.match(r"number\s*=\s*(\d+)", s)
        if m:
            cur['number'] = int(m.group(1))
    return [t for t in tests if 'name' in t and 'number' in t]


def build_gens(pdir: Path, names):
    """按官方命令编译 generator。"""
    gendir = pdir / 'gen'
    built = []
    for nm in names:
        if not nm.endswith('.cpp'):
            continue
        src = gendir / nm
        out = gendir / Path(nm).stem
        if out.exists() and out.stat().st_mtime >= src.stat().st_mtime:
            continue
        cmd = ['g++'] + CXXFLAGS + ['-I', str(COMMON), '-o', str(out), str(src)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f'  [BUILD FAIL] {nm}\n{r.stderr[:800]}')
            sys.exit(1)
        built.append(Path(nm).stem)
    return built


def gen_cases(pdir: Path):
    tests = parse_info(pdir / 'info.toml')
    indir = pdir / 'in'
    indir.mkdir(exist_ok=True)
    built = build_gens(pdir, [t['name'] for t in tests])
    if built:
        print(f'  编译 generator: {" ".join(built)}')
    total = 0
    for t in tests:
        nm, num = t['name'], t['number']
        stem = Path(nm).stem
        for i in range(num):
            case = f'{stem}_{i:02d}'
            dst = indir / (case + '.in')
            if nm.endswith('.in'):
                # 仓库自带的静态样例，直接复制
                shutil.copyfile(pdir / 'gen' / (case + '.in'), dst)
            else:
                with open(dst, 'wb') as f:
                    subprocess.run([str(pdir / 'gen' / stem), str(i)],
                                   stdout=f, check=True)
            total += 1
    return total


def verify(pdir: Path):
    """用 hash.json 校验 .in 的 sha256。"""
    hashes = json.loads((pdir / 'hash.json').read_text())
    indir = pdir / 'in'
    ok, bad, missing = 0, 0, 0
    problems = []
    for name in sorted(hashes):
        if not name.endswith('.in'):
            continue
        f = indir / name
        if not f.exists():
            missing += 1
            problems.append((name, 'MISSING'))
            continue
        data = f.read_bytes()
        got = hashlib.sha256(data).hexdigest()
        if got == hashes[name]:
            ok += 1
        else:
            bad += 1
            crlf = data.count(b'\r\n')
            problems.append((name, f'sha mismatch (crlf={crlf})'))
    return ok, bad, missing, problems


def main():
    keys = sys.argv[1:] or list(PROBLEMS)
    grand_ok = grand_bad = grand_missing = 0
    for k in keys:
        if k not in PROBLEMS:
            print(f'未知题目: {k}')
            continue
        pdir = BIGINT / PROBLEMS[k]
        print(f'=== {k.upper()}  ({PROBLEMS[k]}) ===')
        n = gen_cases(pdir)
        ok, bad, missing, problems = verify(pdir)
        print(f'  生成 {n} 例 -> {pdir / "in"}')
        print(f'  hash 校验: OK={ok}  MISMATCH={bad}  MISSING={missing}')
        for name, why in problems[:8]:
            print(f'    ! {name}: {why}')
        grand_ok += ok
        grand_bad += bad
        grand_missing += missing
    print(f'\n总计: OK={grand_ok}  MISMATCH={grand_bad}  MISSING={grand_missing}')
    print('OFFICIAL_CASES_VERIFIED' if grand_bad == 0 and grand_missing == 0
          else 'VERIFY_FAILED')


if __name__ == '__main__':
    main()
