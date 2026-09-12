#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
d49_verify_local.py —— 本机（Windows/g++）版 554 正确性验证

VM 不可用期间替代 lc_bench/vm/d8_verify.py。
黄金参照 = best/div_D47.cpp（LC #391180 = 29ms，已 554 认证正确），
候选 = best/div_D49.cpp（base-1e16 打包）。全部逐字节 diff。

四段验证：
  P0  官方 26 例：D49(-DDIV_BIG16_OFF) vs D47   —— 回归护栏，证明非 big16 路径没被改坏
  P1  官方 26 例：D49                  vs D47   —— 主判据
  P2  定向形状压测（26 形状族 × B=4 缩放 + edge cases）
  P3  官方生成器扩 seed 压测（8 个 gen，共 304 例）

用法：
  python lc_bench/d49_verify_local.py            # 全部四段
  python lc_bench/d49_verify_local.py --phase 1  # 只跑某段
  python lc_bench/d49_verify_local.py --cand D50 --gold D47
  python lc_bench/d49_verify_local.py --no-build
"""
import argparse
import hashlib
import random
import subprocess
import sys
import time
from pathlib import Path

sys.set_int_max_str_digits(100_000_000)

ROOT = Path(__file__).resolve().parent.parent          # D:\precious_speed
CASES = ROOT / 'lc_bench' / 'cases' / 'div'            # 26 官方输入
GENS = ROOT / 'lc_bench' / 'gens'                      # 官方生成器 .exe
TMP = ROOT / 'lc_bench' / '_tmp' / 'v554'
LOGS = ROOT / 'lc_bench' / '_tmp' / 'v554_bad'
FLAGS = ['-O2', '-std=c++23', '-march=x86-64-v3']

GEN_SEEDS = {
    'a_max_b_random': 24,
    'length_ratio_integer': 48,
    'r_nearly_zero': 80,
    'medium': 40,
    'large': 24,
    'max': 24,
    'small': 40,
    'burnikel_ziegler_bound': 24,
}


# ---------------------------------------------------------------- utilities
def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


def build(name, out, extra=()):
    src = ROOT / 'best' / f'div_{name}.cpp'
    if not src.exists():
        print(f'  !! 源文件不存在: {src}', flush=True)
        return False
    t0 = time.time()
    r = sh(['g++', *FLAGS, *extra, '-o', str(out), str(src)], text=True)
    if r.returncode != 0:
        print(f'  BUILD FAIL {out.name}:\n{r.stderr[-4000:]}', flush=True)
        return False
    print(f'  build {out.name:16s} OK  ({time.time() - t0:.1f}s)', flush=True)
    return True


def run_bin(binp, infile, timeout=600):
    """返回 (stdout_bytes_归一化行尾, returncode)。Windows 输出带 CR，统一剥掉。"""
    with open(infile, 'rb') as f:
        p = subprocess.run([str(binp)], stdin=f, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=timeout)
    return p.stdout.replace(b'\r\n', b'\n'), p.returncode, p.stderr


def cmp_pair(gold, cand, infile, tag, keep_bad=True):
    """跑黄金与候选，逐字节比较。返回 True=一致。"""
    try:
        a, ra, ea = run_bin(gold, infile)
        b, rb, eb = run_bin(cand, infile)
    except subprocess.TimeoutExpired:
        print(f'  !! TIMEOUT {tag}', flush=True)
        return False
    if ra != 0 or rb != 0:
        print(f'  !! RC {tag}  gold={ra} cand={rb}', flush=True)
        if eb:
            print(f'     cand stderr: {eb[:400]!r}', flush=True)
        return False
    if a != b:
        ha, hb = hashlib.md5(a).hexdigest()[:12], hashlib.md5(b).hexdigest()[:12]
        # 定位首个不同行
        la, lb = a.split(b'\n'), b.split(b'\n')
        idx = next((i for i in range(min(len(la), len(lb))) if la[i] != lb[i]), -1)
        print(f'  !! MISMATCH {tag}  md5 {ha} vs {hb}  首异行#{idx}', flush=True)
        if idx >= 0:
            print(f'     gold: {la[idx][:120]!r}', flush=True)
            print(f'     cand: {lb[idx][:120]!r}', flush=True)
        if keep_bad:
            LOGS.mkdir(parents=True, exist_ok=True)
            dst = LOGS / f'bad_{tag.replace("/", "_").replace(" ", "_")}.in'
            try:
                dst.write_bytes(Path(infile).read_bytes())
                print(f'     已保存: {dst}', flush=True)
            except Exception as e:
                print(f'     (保存失败 {e})', flush=True)
        return False
    return True


# ---------------------------------------------------------------- phases
def phase_official(gold, cand, label):
    ins = sorted(CASES.glob('*.in'))
    if len(ins) != 26:
        print(f'  警告: 官方用例数 = {len(ins)}（期望 26）', flush=True)
    bad = 0
    t0 = time.time()
    for p in ins:
        ok = cmp_pair(gold, cand, p, f'{label}/{p.stem}')
        if not ok:
            bad += 1
    print(f'  [{label}] 官方 {len(ins)} 例, 不一致 {bad}  ({time.time() - t0:.1f}s)', flush=True)
    return bad == 0


def gen_case(rng, pairs):
    lines = [str(len(pairs))]
    for la, lb in pairs:
        a = rng.randrange(10 ** (la - 1), 10 ** la) if la > 0 else 0
        b = rng.randrange(10 ** (lb - 1), 10 ** lb) if lb > 0 else 1
        if b == 0:
            b = 1
        if rng.random() < 0.5:
            a = -a
        if rng.random() < 0.5:
            b = -b
        lines.append(f'{a} {b}')
    return ('\n'.join(lines) + '\n').encode()


def phase_targeted(gold, cand):
    rng = random.Random(20260803)
    shapes = []
    for l2 in [65, 70, 80, 100, 128, 129, 150, 200, 256, 257, 300, 400, 512, 600]:
        for prod in [5000, 15000, 19000, 20000, 20001, 25000, 40000, 100000]:
            shapes.append((l2, max(1, prod // l2)))
    for l2 in [200, 500, 1000, 2000, 5000, 9000, 12000]:
        for k in [1, 2, 3, 4, 6, 9, 13, 20, 33]:
            shapes.append((l2, l2 * k + rng.randrange(-3, 4)))
    for base in [1024, 2048, 4096, 8192, 16384]:
        for d in [-2, -1, 0, 1, 2]:
            shapes.append((base + d, 2 * base + d))
            shapes.append((base + d, base // 2 + d))
    shapes = [(a, b) for a, b in shapes if a >= 1 and b >= 1]

    TMP.mkdir(parents=True, exist_ok=True)
    total = bad = 0
    B = 4
    batch, pairs = 0, []
    t0 = time.time()
    for l2, qn in shapes:
        pairs.append(((l2 + qn) * B, l2 * B))
        if len(pairs) >= 12:
            batch += 1
            inf = TMP / f't49_{batch}.in'
            inf.write_bytes(gen_case(rng, pairs))
            if not cmp_pair(gold, cand, inf, f'targeted/b{batch}'):
                bad += len(pairs)
            else:
                inf.unlink(missing_ok=True)
            total += len(pairs)
            pairs = []
    # edge cases
    edge = ['8', '0 7', '7 8', '-7 8', '123456789 123456789',
            f'{10**4000} {10**2000}', f'{-(10**4000)} {10**2000}',
            f'{10**4000} {-(10**2000)}', f'{(10**2000) * (10**2000)} {10**2000}']
    inf = TMP / 't49_edge.in'
    inf.write_bytes(('\n'.join(edge) + '\n').encode())
    total += 8
    if not cmp_pair(gold, cand, inf, 'targeted/edge'):
        bad += 8
    else:
        inf.unlink(missing_ok=True)
    print(f'  定向形状压测: {total} 例, 不一致 {bad}  ({time.time() - t0:.1f}s)', flush=True)
    return bad == 0


def phase_gens(gold, cand):
    TMP.mkdir(parents=True, exist_ok=True)
    total = bad = 0
    t0 = time.time()
    for g, n in GEN_SEEDS.items():
        exe = GENS / f'div_{g}.exe'
        if not exe.exists():
            print(f'  !! 生成器缺失: {exe}', flush=True)
            continue
        gbad = 0
        gt0 = time.time()
        for seed in range(1, n + 1):
            inf = TMP / f's49_{g}_{seed}.in'
            with open(inf, 'wb') as f:
                rc = subprocess.run([str(exe), str(seed)], stdout=f).returncode
            if rc != 0:
                print(f'  !! GEN FAIL {g} seed={seed}', flush=True)
                inf.unlink(missing_ok=True)
                continue
            total += 1
            if not cmp_pair(gold, cand, inf, f'gen/{g}_{seed}'):
                bad += 1
                gbad += 1
            inf.unlink(missing_ok=True)
        print(f'  {g:26s} {n:>4d} 例  bad={gbad}  ({time.time() - gt0:.0f}s)', flush=True)
    print(f'  官方生成器压测: {total} 例, 不一致 {bad}  ({time.time() - t0:.0f}s)', flush=True)
    return bad == 0


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gold', default='D47')
    ap.add_argument('--cand', default='D49')
    ap.add_argument('--phase', default='all')
    ap.add_argument('--no-build', action='store_true')
    args = ap.parse_args()

    bindir = ROOT / 'lc_bench' / '_tmp'
    bindir.mkdir(parents=True, exist_ok=True)
    gold = bindir / f'v554_{args.gold}.exe'
    cand = bindir / f'v554_{args.cand}.exe'
    cand_off = bindir / f'v554_{args.cand}_off.exe'

    if not args.no_build:
        print('=== 编译 ===', flush=True)
        if not build(args.gold, gold):
            return 1
        if not build(args.cand, cand):
            return 1
        if not build(args.cand, cand_off, extra=['-DDIV_BIG16_OFF']):
            return 1

    want = args.phase
    res = {}
    if want in ('all', '0'):
        print('\n=== P0 回归护栏: cand(BIG16_OFF) vs gold, 官方 26 例 ===', flush=True)
        res['P0'] = phase_official(gold, cand_off, 'P0')
    if want in ('all', '1'):
        print('\n=== P1 主判据: cand vs gold, 官方 26 例 ===', flush=True)
        res['P1'] = phase_official(gold, cand, 'P1')
    if want in ('all', '2'):
        print('\n=== P2 定向形状压测 ===', flush=True)
        res['P2'] = phase_targeted(gold, cand)
    if want in ('all', '3'):
        print('\n=== P3 官方生成器扩 seed 压测 ===', flush=True)
        res['P3'] = phase_gens(gold, cand)

    print('\n=== 汇总 ===', flush=True)
    for k, v in res.items():
        print(f'  {k}: {"PASS" if v else "FAIL"}', flush=True)
    allok = all(res.values()) and res
    print(f'  总判定: {"ALL PASS" if allok else "FAIL"}', flush=True)
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
