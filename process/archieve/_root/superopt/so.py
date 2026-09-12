#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
so.py — 超优化器驱动 (自定义 spec 格式)

流程:
  spec (Python dict)
    -> 生成 K 个对抗性测试输入 + 目标值向量
    -> spec.json
    -> so_core (C++ 穷举 + 5 类剪枝)
    -> 候选表达式
    -> 大规模验证 (随机 + 边界穷举)   <- 这一步旧版完全没有
    -> 输出可直接粘进 C++ 源码的表达式

spec 字段:
  name        标识
  inputs      [(名字, 位宽上界表达式或取值生成器)]
  goals       {名字: ref 函数}
  consts      常量列表 (可用 auto_consts 自动推导)
  ops         允许的操作 (越少越快)
  max_insn    最大指令数
  verify_n    验证样本数

用法:
  python so.py divmod        # 搜 (q,r)=divmod(s,10000), s<2^45
  python so.py list
"""
import json, os, random, subprocess, sys, re, time

HERE = os.path.dirname(os.path.abspath(__file__))
M64 = (1 << 64) - 1

# ---------------------------------------------------------------- 语义 (与 so_core 严格一致)
def _add(a, b):   return (a + b) & M64
def _sub(a, b):   return (a - b) & M64
def _mul(a, b):   return (a * b) & M64
def _mulhi(a, b): return ((a * b) >> 64) & M64
def _and(a, b):   return a & b
def _or(a, b):    return a | b
def _xor(a, b):   return a ^ b
def _shl(a, b):   return (a << b) & M64 if b < 64 else None
def _shr(a, b):   return (a >> b) if b < 64 else None
def _sar(a, b):
    if b >= 64: return None
    s = a - (1 << 64) if a >> 63 else a
    return (s >> b) & M64
def _not(a):      return (~a) & M64
def _neg(a):      return (-a) & M64
def _lea2(a, b):  return (a + 2 * b) & M64
def _lea4(a, b):  return (a + 4 * b) & M64
def _lea8(a, b):  return (a + 8 * b) & M64
def _cmplt(a, b): return 1 if a < b else 0
def _minu(a, b):  return min(a, b)
def _maxu(a, b):  return max(a, b)
def _rol(a, b):
    s = b & 63
    return a if s == 0 else (((a << s) | (a >> (64 - s))) & M64)
def _bzhi(a, b):  return a if b >= 64 else (a & ((1 << b) - 1))

SEM = dict(add=_add, sub=_sub, mul=_mul, mulhi=_mulhi, and_=_and, or_=_or,
           xor=_xor, shl=_shl, shr=_shr, sar=_sar, not_=_not, neg=_neg,
           lea2=_lea2, lea4=_lea4, lea8=_lea8, cmplt=_cmplt, minu=_minu,
           maxu=_maxu, rol=_rol, bzhi=_bzhi, andn=lambda a, b: (~a & b) & M64)

# ---- 本轮补齐的指令 (BMI1 / BMI2 / ABM) —— 语义必须与 so_core.cpp eval1 逐位一致
def _ror(a, b):
    s = b & 63
    return a if s == 0 else (((a >> s) | (a << (64 - s))) & M64)
def _bextr(a, b):
    st, ln = b & 0xFF, (b >> 8) & 0xFF
    if st >= 64: return 0
    t = a >> st
    return t if ln >= 64 else (t & ((1 << ln) - 1))
def _blsi(a):   return (a & (-a)) & M64
def _blsr(a):   return (a & ((a - 1) & M64)) & M64
def _blsmsk(a): return (a ^ ((a - 1) & M64)) & M64
def _tzcnt(a):
    if a == 0: return 64
    n = 0
    while not (a >> n) & 1: n += 1
    return n
def _lzcnt(a):
    if a == 0: return 64
    return 64 - a.bit_length()
def _popcnt(a): return bin(a & M64).count("1")
def _pdep(a, b):
    r, m, s = 0, b, a
    while m:
        lo = m & (-m)
        if s & 1: r |= lo
        s >>= 1; m ^= lo
    return r & M64
def _pext(a, b):
    r, m, k = 0, b, 0
    while m:
        lo = m & (-m)
        if a & lo: r |= (1 << k)
        k += 1; m ^= lo
    return r & M64
SEM.update(ror=_ror, bextr=_bextr, blsi=_blsi, blsr=_blsr, blsmsk=_blsmsk,
           tzcnt=_tzcnt, lzcnt=_lzcnt, popcnt=_popcnt, pdep=_pdep, pext=_pext,
           cmpeq=lambda a, b: 1 if a == b else 0,
           cmpgt=lambda a, b: 1 if a > b else 0,
           sbbmask=lambda a, b: M64 if a < b else 0)
# so_core 打印的名字用的是保留字裸名, eval 时需改写
_RENAME = {"and": "and_", "or": "or_", "not": "not_"}

UOPS = dict(add=1, sub=1, mul=1, mulhi=2, and_=1, or_=1, xor=1, shl=1, shr=1,
            sar=1, not_=1, neg=1, lea2=1, lea4=1, lea8=1, cmplt=2, minu=2,
            maxu=2, rol=1, bzhi=1)


def replay(expr, env):
    """在给定输入环境下重放 so_core 输出的表达式"""
    e = expr
    for k, v in _RENAME.items():
        e = re.sub(r'\b%s\(' % k, v + '(', e)
    return eval(e, {"__builtins__": {}}, dict(SEM, **env))


# ---------------------------------------------------------------- 常量自动推导
def auto_consts(d, nmax):
    """针对 '除以常数 d, 被除数 < nmax' 自动给出可能有用的 *数值* 常量

    注意: 移位量走独立池 (auto_shifts), 不要混进来 —— so_core 的 P7 剪枝
    依赖 poolKind 区分 '数值常量' 与 '移位量', 混用会让搜索空间白白翻倍。
    """
    cs = {d, d - 1}
    # Barrett / Granlund-Montgomery 系列
    for s in (64, 63, 62, 61, 60, 58, 56, 52, 48, 45, 44, 32):
        m_ceil = -(-(1 << s) // d)
        m_flr = (1 << s) // d
        if m_ceil <= M64: cs.add(m_ceil)
        if m_flr <= M64:  cs.add(m_flr)
    # Lemire fastmod 的 M = floor(2^64/d) + 1
    cs.add((M64 // d) + 1)
    cs.discard(0)
    return sorted(c for c in cs if c <= M64)


def auto_shifts(extra=()):
    """移位量候选 (poolKind=2). 只放真正可能出现的档位, 越少越好。"""
    ss = {1, 2, 3, 4, 8, 13, 16, 19, 32}
    ss.update(extra)
    return sorted(s for s in ss if 0 < s < 64)


# ---------------------------------------------------------------- SPEC 库
def make_divmod_spec(d=10000, bits=45, max_insn=4, only_q=False, only_r=False,
                     ops=None, consts=None, shifts=None, K=32, tl=120.0):
    nmax = 1 << bits
    rnd = random.Random(20260805)
    vals = [0, 1, d - 1, d, d + 1, 2 * d, nmax - 1, nmax - 2,
            nmax - nmax % d, (nmax - 1) // d * d, d * d, d * d - 1]
    while len(vals) < K:
        r = rnd.randrange(nmax)
        # 一半样本强制落在 d 的倍数边界附近 (Barrett 最易出错处)
        if rnd.random() < 0.4:
            r = (r // d) * d + rnd.choice([0, 1, d - 1])
            r %= nmax
        vals.append(r)
    vals = vals[:K]

    goals = {}
    if not only_r: goals["q"] = lambda s: s // d
    if not only_q: goals["r"] = lambda s: s % d
    return {
        "name": f"divmod_{d}_lt2p{bits}",
        "K": K,
        "inputs": {"s": vals},
        "goals": goals,
        "consts": consts if consts is not None else auto_consts(d, nmax),
        "shifts": shifts if shifts is not None else auto_shifts((bits, bits + 1, 45, 51, 52)),
        "ops": ops or ["add", "sub", "mul", "mulhi", "shr", "shl", "and", "lea2", "lea4", "lea8"],
        "max_insn": max_insn,
        "time_limit": tl,
        "verify": {"kind": "range", "bits": bits, "d": d},
    }


def make_carrystep_spec(d=10000, vbits=45, cbits=32, max_insn=5, K=32, tl=180.0,
                        ops=None, consts=None, shifts=None):
    """carryPropSeg 一步:  s=v+c;  q=s/d;  r=s%d   (两个输入)"""
    rnd = random.Random(777)
    vmax, cmax = 1 << vbits, 1 << cbits
    vv, cc = [], []
    seeds = [(0, 0), (d - 1, 1), (d, 0), (vmax - 1, cmax - 1), (d - 1, d - 1),
             (vmax - 1, 0), (0, cmax - 1), (d * d, d)]
    for a, b in seeds: vv.append(a); cc.append(b)
    while len(vv) < K:
        a = rnd.randrange(vmax); b = rnd.randrange(cmax)
        if rnd.random() < 0.4: a = (a // d) * d + rnd.choice([0, d - 1])
        vv.append(a % vmax); cc.append(b)
    vv, cc = vv[:K], cc[:K]
    return {
        "name": f"carrystep_{d}",
        "K": K,
        "inputs": {"v": vv, "c": cc},
        "goals": {"q": lambda v, c: (v + c) // d, "r": lambda v, c: (v + c) % d},
        "consts": consts if consts is not None else auto_consts(d, 1 << (vbits + 1)),
        "shifts": shifts if shifts is not None else auto_shifts((vbits, vbits + 1)),
        "ops": ops or ["add", "sub", "mul", "mulhi", "shr", "and"],
        "max_insn": max_insn,
        "time_limit": tl,
        "verify": {"kind": "carry", "vbits": vbits, "cbits": cbits, "d": d},
    }


# ---------------------------------------------------------------- 运行
def _dec(b):
    """中文 Windows 下 MSVC/cmd 输出是 GBK, 直接 text=True 会 UnicodeDecodeError。"""
    if not b:
        return ""
    if isinstance(b, str):
        return b
    for enc in ("utf-8", "gbk", "mbcs"):
        try:
            return b.decode(enc)
        except Exception:
            pass
    return b.decode("utf-8", "replace")


def _find_vcvars():
    for c in (r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
              r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
              r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
              r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"):
        if os.path.exists(c):
            return c
    return None


def _build_msvc(src, exe):
    """MSVC 最高优化: /O2 (速度) + /GL+/LTCG (全程序优化=LTO) + /arch:AVX2 (本机指令集)。
    仅优化 SO 二进制自身速度; 搜索目标微架构仍固定 Zen3 (源码内 op 集合/端口表)。"""
    vc = _find_vcvars()
    if not vc:
        return False, "no vcvars64.bat"
    bat = os.path.join(HERE, "_build_msvc.bat")
    with open(bat, "w") as f:
        f.write("@echo off\r\n")
        f.write('call "%s" >nul\r\n' % vc)
        f.write('cd /d "%s"\r\n' % HERE)
        f.write('cl /nologo /O2 /GL /Oi /arch:AVX2 /std:c++17 /EHsc /DNDEBUG '
                '/Fe:"%s" "%s" /link /LTCG\r\n' % (exe, src))
        f.write('exit /b %ERRORLEVEL%\r\n')
    r = subprocess.run(["cmd", "/c", bat], capture_output=True)   # 不用 text: 输出可能是 GBK
    # 注意: 不能在此删除 _build_msvc.bat —— cmd 是逐行读取批处理的,
    # 运行中删除会让 cmd 提前中止并返回非 0, 造成「编译其实成功却判为失败」。
    p = os.path.join(HERE, "so_core.obj")
    try:
        if os.path.exists(p): os.remove(p)
    except OSError:
        pass
    if r.returncode != 0:
        return False, (_dec(r.stdout) + _dec(r.stderr))
    return True, "MSVC /O2 /GL /LTCG /arch:AVX2"


def _build_gpp(src, exe):
    r = subprocess.run(["g++", "-O3", "-flto", "-std=c++17", "-march=native", "-o", exe, src],
                       capture_output=True)
    if r.returncode != 0:
        return False, _dec(r.stderr)
    return True, "g++ -O3 -flto -march=native"


def build_core(force=False, prefer=None):
    """构建 SO 二进制。Windows 首选 MSVC (LTO), 失败自动回退 g++; 其它平台用 g++。
    prefer: 'msvc' | 'gpp' | None(自动)。可用环境变量 SO_CC 覆盖。"""
    src = os.path.join(HERE, "so_core.cpp")
    exe = os.path.join(HERE, "so_core.exe" if os.name == "nt" else "so_core")
    if (not force) and os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(src):
        return exe
    prefer = prefer or os.environ.get("SO_CC") or ("msvc" if os.name == "nt" else "gpp")
    order = ["msvc", "gpp"] if prefer == "msvc" else ["gpp", "msvc"]
    errs = []
    for how in order:
        print(f"[build] compiling so_core via {how} ...")
        ok, msg = (_build_msvc if how == "msvc" else _build_gpp)(src, exe)
        if ok:
            print(f"[build] OK -> {msg}")
            return exe
        errs.append(f"--- {how} FAILED ---\n{(msg or '')[:1500]}")
    print("[build] 所有编译器均失败:\n" + "\n".join(errs))
    sys.exit(1)


def write_spec_json(spec, path):
    inp = spec["inputs"]
    names = list(inp.keys())
    K = spec["K"]
    obj = {
        "K": K, "max_insn": spec["max_insn"], "time_limit": spec.get("time_limit", 60.0),
        "verbose": 0, "find_all": 0,
        "max_cands": spec.get("max_cands", 200),
        "slack": spec.get("slack", 2),
        # 0=吞吐优先(热点各次迭代可并行)  1=延迟优先(热点在串行依赖链上)  2=取大
        "cost_mode": {"tput": 0, "lat": 1, "blend": 2}.get(spec.get("cost_mode", "blend"), 2),
        "ops": spec["ops"], "consts": spec["consts"],
        "shifts": spec.get("shifts", []),
        "input_names": names,
        # 1 = 该输入来自串行依赖链(如上一步的进位); 0 = 可预取/预计算(如数组元素)
        "input_crit": [1 if n in spec.get("crit_inputs", names) else 0 for n in names],
        "goal_names": list(spec["goals"].keys()),
        # 1 = 该 goal 在跨迭代串行依赖链上(计入关键路径); 0 = 只吃吞吐
        "goal_crit": [1 if g in spec.get("crit_goals", spec["goals"].keys()) else 0
                      for g in spec["goals"].keys()],
    }
    for i, n in enumerate(names):
        obj[f"input_{i}"] = inp[n][:K]
    cols = [inp[n][:K] for n in names]
    for gi, (gn, fn) in enumerate(spec["goals"].items()):
        # goal 可以是 lambda(输入...) 也可以是预先算好的值向量
        if callable(fn):
            obj[f"goal_{gi}"] = [fn(*[c[t] for c in cols]) for t in range(K)]
        else:
            obj[f"goal_{gi}"] = list(fn)[:K]
    with open(path, "w") as f:
        json.dump(obj, f)
    return obj


def parse_output(txt):
    """解析 so_core 输出的 *全部* 候选 (已按 uops,lat 排序)。

    两阶段策略: 搜索期只用少量测试向量快筛并收集候选, 这里把它们全部取回,
    再由 verify_all 逐个做大规模独立验证 —— 单个候选可能是小 K 下的假阳性。
    """
    cands, cur = [], None
    for line in txt.splitlines():
        m = re.match(r"FOUND cyc=([\d.]+) cp=(\d+) res=([\d.]+) uops=(\d+) insns=(\d+)", line)
        if m:
            cur = {"cyc": float(m.group(1)), "cp": int(m.group(2)),
                   "res": float(m.group(3)), "uops": int(m.group(4)),
                   "insns": int(m.group(5)), "exprs": {}}
            cands.append(cur)
            continue
        m = re.match(r"\s+GOAL (\S+) = (.+)$", line)
        if m and cur is not None:
            cur["exprs"][m.group(1)] = m.group(2)
    return cands


def verify(spec, cand, n=400000, quiet=False):
    """大规模独立验证 —— 旧版完全没有这一步"""
    v = spec["verify"]
    rnd = random.Random(0xC0FFEE)
    bad = 0; checked = 0
    def note(msg):
        if not quiet and bad < 3: print(msg)
    if v["kind"] == "digits4":
        # x<10000 全域穷举 —— 无需抽样, 直接证明
        # 快筛阶段 (n 较小) 用等距抽样, 尽早撞上错误
        step = 1 if n >= 10000 else max(1, 10000 // max(n, 1))
        for x in range(0, 10000, step):
            checked += 1
            exp = ((x // 1000) | ((x // 100 % 10) << 8) |
                   ((x // 10 % 10) << 16) | ((x % 10) << 24))
            got = replay(cand["exprs"]["packed"], {"x": x})
            if got != exp:
                note(f"  !! x={x}: got {got:#x} want {exp:#x}")
                bad += 1
                if quiet: break
        return checked, bad
    if v["kind"] == "swar":
        nb = v["nbits"]
        mask = (1 << nb) - 1
        def ref(G, P, cin0):
            c = cin0 & 1; out = 0
            for j in range(nb):
                c = ((G >> j) & 1) | (((P >> j) & 1) & c)
                out |= c << j
            return out
        cases = [(0, 0, 0), (0, 0, 1), (0, mask, 1), (0, mask, 0),
                 (mask, 0, 0), (mask, 0, 1), (1, mask ^ 1, 0), (1, mask ^ 1, 1),
                 (0, mask ^ 1, 1), (1 << (nb - 1), 0, 1)]
        for _ in range(n):
            # G 与 P 必须互斥 (t>=BASE 与 t==BASE-1 不可能同时成立)
            g = rnd.randrange(1 << nb)
            p = rnd.randrange(1 << nb) & ~g & mask
            cases.append((g, p, rnd.randrange(2)))
        for _ in range(n // 4):
            # 长传播链: P 几乎全 1, G 稀疏 —— 最能暴露借位链错误
            g = (1 << rnd.randrange(nb)) if rnd.randrange(4) else 0
            p = mask & ~g
            cases.append((g, p, rnd.randrange(2)))
        for G, P, c0 in cases:
            checked += 1
            got = replay(cand["exprs"]["cout"], {"G": G, "P": P, "cin": c0}) & mask
            exp = ref(G, P, c0)
            if got != exp:
                note(f"  !! G={G:#x} P={P:#x} cin={c0}: got {got:#x} want {exp:#x}")
                bad += 1
                if quiet: break
        return checked, bad
    # ---- 2026-08-07 新增: 输入域 <= 10000 的 kind, 一律**穷举证明** ----
    # (快筛阶段 n 小则等距抽样, 全量阶段 n>=域大小 => 完全穷举, 强于随机抽样)
    if v["kind"] in ("digits2", "digits2x2", "digits2x2_core", "split100",
                     "str4toi"):
        kind = v["kind"]
        if kind == "digits2x2_core":
            # 只到 u = pk(a) | pk(b)<<32 (未做跨 lane 合并), 全域 100x100 穷举
            dom = [({"p": a | (b << 32)},
                    ((a // 10) | ((a % 10) << 8)) |
                    (((b // 10) | ((b % 10) << 8)) << 32))
                   for a in range(100) for b in range(100)]
            gname = "u"
        elif kind == "digits2":
            dom = [({"y": y}, (y // 10) | ((y % 10) << 8)) for y in range(100)]
            gname = "packed"
        elif kind == "split100":
            dom = [({"x": x}, (x // 100) | ((x % 100) << 32)) for x in range(10000)]
            gname = "p"
        elif kind == "digits2x2":
            dom = [({"p": a | (b << 32)},
                    (a // 10) | ((a % 10) << 8) | ((b // 10) << 16) | ((b % 10) << 24))
                   for a in range(100) for b in range(100)]
            gname = "packed"
        else:  # str4toi
            dom = []
            for x in range(10000):
                dg = [x // 1000, x // 100 % 10, x // 10 % 10, x % 10]
                u = ((0x30 + dg[0]) | ((0x30 + dg[1]) << 8) |
                     ((0x30 + dg[2]) << 16) | ((0x30 + dg[3]) << 24))
                dom.append(({"u": u}, x))
            gname = "v"
        step = 1 if n >= len(dom) else max(1, len(dom) // max(n, 1))
        expr = cand["exprs"][gname]
        for env, exp in dom[::step]:
            checked += 1
            got = replay(expr, env)
            if got != exp:
                note(f"  !! {env}: got {got:#x} want {exp:#x}")
                bad += 1
                if quiet: break
        return checked, bad

    d = v["d"]
    if v["kind"] == "range":
        nmax = 1 << v["bits"]
        cases = [0, 1, d - 1, d, nmax - 1, nmax - 2]
        cases += [(nmax - 1) // d * d + o for o in (-1, 0, 1)]
        cases += [rnd.randrange(nmax) for _ in range(n)]
        cases += [(rnd.randrange(nmax // d)) * d + rnd.choice([0, 1, d - 1]) for _ in range(n)]
        for s in cases:
            if not (0 <= s < nmax): continue
            checked += 1
            env = {"s": s}
            for gn, expr in cand["exprs"].items():
                got = replay(expr, env)
                exp = (s // d) if gn == "q" else (s % d)
                if got != exp:
                    note(f"  !! s={s} {gn}: got {got} want {exp}")
                    bad += 1
            if bad and quiet: break
    else:
        vmax, cmax = 1 << v["vbits"], 1 << v["cbits"]
        cases = [(0, 0), (vmax - 1, cmax - 1), (d - 1, 1), (vmax - 1, 0)]
        cases += [(rnd.randrange(vmax), rnd.randrange(cmax)) for _ in range(n)]
        cases += [((rnd.randrange(vmax // d)) * d + rnd.choice([0, d - 1]),
                   rnd.randrange(cmax)) for _ in range(n)]
        for a, b in cases:
            checked += 1
            env = {"v": a, "c": b}
            s = a + b
            for gn, expr in cand["exprs"].items():
                got = replay(expr, env)
                exp = (s // d) if gn == "q" else (s % d)
                if got != exp:
                    note(f"  !! v={a} c={b} {gn}: got {got} want {exp}")
                    bad += 1
            if bad and quiet: break
    return checked, bad


def verify_all(spec, cands, quick_n=3000, full_n=400000, want=1):
    """两级验证: 先小样本快筛淘汰假阳性, 幸存者再做全量验证。

    候选按综合周期 cyc 升序, 第一个通过全量验证的即为真最优解。
    """
    survivors = []
    print(f"\n-- 阶段1 快筛 ({len(cands)} 个候选 x {quick_n} 样本) --")
    for idx, c in enumerate(cands):
        _, bad = verify(spec, c, n=quick_n, quiet=True)
        tag = "pass" if bad == 0 else f"reject({bad})"
        if bad == 0:
            survivors.append(c)
        if idx < 12 or bad == 0:
            print(f"   [{idx:>3}] cyc={c['cyc']:.2f} cp={c['cp']} uops={c['uops']} -> {tag}")
    print(f"   幸存 {len(survivors)}/{len(cands)}")
    if not survivors:
        return None, survivors

    print(f"\n-- 阶段2 全量验证 (每个 ~{full_n*2} 样本) --")
    winner = None
    for c in survivors[:max(want, 3)]:
        checked, bad = verify(spec, c, n=full_n)
        print(f"   cyc={c['cyc']:.2f} cp={c['cp']} res={c['res']:.2f} uops={c['uops']}  checked={checked}  "
              f"mismatches={bad}  {'OK' if bad == 0 else 'FAIL'}")
        for gn, e in c["exprs"].items():
            print(f"        {gn} = {e}")
        if bad == 0 and winner is None:
            winner = c
            if want == 1:
                break
    return winner, survivors


# ---------------------------------------------------------------- 多进程协调器 (原子记账)
# 并行由「多进程分片」提供 (so_core --shard k n): 每进程独立地址空间 => 零数据竞争,
# 彻底避开 in-process std::thread 的堆损坏。协调器(本 py)= 独立的原子记账程序:
#   - 候选池   cand_pool/<name>.json      : 各分片候选去重合并, 原子落盘 (临时文件+rename)
#   - 记账表   search_accounting.json     : 每 spec/分片 的状态/节点数/候选数/时间戳
# 白天只搜集候选(预设输入-输出快筛), 不做 40 万全量校验; 夜间 verify_pool 统一校验。
CAND_DIR = os.path.join(HERE, "cand_pool")
ACCT_PATH = os.path.join(HERE, "search_accounting.json")


def _atomic_write_json(path, obj):
    """原子写 JSON。

    [2026-08-07 修] Windows 上 os.replace 会被杀软/索引器/残留句柄短暂锁住 ->
    PermissionError(13)。night4 的 E1 digits2x2_core 就因此整条 stage 崩掉,
    还留下 .json.tmp 残骸, 1800s 搜索白烧。
    改为: 唯一 tmp 名(带 pid) + 指数退避重试 + 最终退回直写。
    原子性让位于「搜索成果绝不能因一次写盘失败而丢失」。
    """
    tmp = "%s.%d.tmp" % (path, os.getpid())
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    last = None
    for i in range(8):             # 0.01+0.02+...+1.28 ≈ 2.5s 重试窗口
        try:
            os.replace(tmp, path)  # 原子替换 (记账不会写到一半被读到)
            return
        except PermissionError as e:
            last = e
            time.sleep(0.01 * (2 ** i))
    try:
        with open(path, "w") as f:
            json.dump(obj, f, indent=2)
        sys.stderr.write("[warn] atomic replace failed (%s), fell back to direct write: %s\n"
                         % (last, path))
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass


def _load_json(path, default):
    if os.path.exists(path):
        try:
            return json.load(open(path))
        except Exception:
            return default
    return default


def _acct_update(name, patch):
    acct = _load_json(ACCT_PATH, {})
    rec = acct.get(name, {})
    rec.update(patch)
    acct[name] = rec
    _atomic_write_json(ACCT_PATH, acct)


def _cand_key(c):
    return tuple(sorted(c["exprs"].items()))


def search_parallel(spec, nproc=7, save=True, merge=True):
    """白天: 多进程分片搜索, 收集所有通过「预设输入/目标」快筛的候选并原子落盘,
    *不做* 大规模校验 (留待夜间 verify_pool)。返回去重后的候选列表。"""
    exe = build_core()
    nproc = int(os.environ.get("SO_NPROC", nproc))
    if nproc < 1:
        nproc = 1
    sj = os.path.join(HERE, "_spec_%s.json" % spec["name"])
    write_spec_json(spec, sj)
    print(f"=== search {spec['name']}  (nproc={nproc}, 多进程分片) ===")
    print(f"  ops={spec['ops']}")
    print(f"  consts({len(spec['consts'])})={spec['consts'][:8]}{' ...' if len(spec['consts'])>8 else ''}")
    print(f"  shifts={spec.get('shifts', [])}  K={spec['K']}  max_insn={spec['max_insn']}"
          f"  tl={spec.get('time_limit')}s")
    _acct_update(spec["name"], {"status": "running", "nproc": nproc,
                                "start": time.strftime("%Y-%m-%d %H:%M:%S")})
    t0 = time.time()
    procs = []
    for k in range(nproc):
        p = subprocess.Popen([exe, "--shard", str(k), str(nproc), sj],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        procs.append(p)
    all_cands, per_shard, tot_nodes = [], [], 0
    for k, p in enumerate(procs):
        out, err = p.communicate()
        cs = parse_output(out)
        mm = re.findall(r"nodes=(\d+)", out)      # 取最后一次(最终 stats), 非首次(depth<=1)
        nn = int(mm[-1]) if mm else 0
        to = "(TIMEOUT)" in out
        tot_nodes += nn
        per_shard.append({"shard": k, "cands": len(cs), "nodes": nn,
                          "rc": p.returncode, "timeout": to})
        all_cands.extend(cs)
        if err and err.strip():
            print(f"  [shard{k} stderr] {err.strip()[:200]}")
    # 合并已有候选池 (增量累积: 多次搜索/加深指令数, 候选只增不减)
    if merge:
        old = _load_json(os.path.join(CAND_DIR, "%s.json" % spec["name"]), {})
        all_cands = (old.get("cands", []) if isinstance(old, dict) else []) + all_cands
    seen, uniq = set(), []
    for c in all_cands:
        key = _cand_key(c)
        if key in seen:
            continue
        seen.add(key)
        uniq.append(c)
    uniq.sort(key=lambda c: (c["cyc"], c["cp"], c["uops"], c["insns"]))
    if save:
        os.makedirs(CAND_DIR, exist_ok=True)
        _atomic_write_json(os.path.join(CAND_DIR, "%s.json" % spec["name"]), {
            "name": spec["name"], "saved": time.strftime("%Y-%m-%d %H:%M:%S"),
            "verify": spec.get("verify"), "n_cands": len(uniq), "cands": uniq})
    _acct_update(spec["name"], {"status": "searched", "nodes": tot_nodes,
                                "n_cands": len(uniq), "shards": per_shard,
                                "elapsed": round(time.time() - t0, 1),
                                "end": time.strftime("%Y-%m-%d %H:%M:%S")})
    to_any = any(s["timeout"] for s in per_shard)
    print(f"  -> {len(uniq)} 候选 (nodes={tot_nodes:,}, {time.time()-t0:.1f}s"
          f"{', 有分片超时' if to_any else ''}) 已存候选池, 待夜间校验")
    return uniq


def verify_pool(name=None, quick_n=3000, full_n=400000, want=1):
    """夜间: 对候选池逐一大规模独立校验, 选真赢家, 写 superopt_results.json。"""
    if not os.path.isdir(CAND_DIR):
        print("[verify] 无候选池目录"); return {}
    names = ([name] if name else
             sorted(f[:-5] for f in os.listdir(CAND_DIR) if f.endswith(".json")))
    done = _load_json(os.path.join(HERE, "superopt_results.json"), {})
    for nm in names:
        pool = _load_json(os.path.join(CAND_DIR, "%s.json" % nm), None)
        if not pool or not pool.get("cands"):
            print(f"[verify] 跳过 {nm} (无候选)"); continue
        spec = {"verify": pool["verify"]}
        cands = pool["cands"]
        print(f"\n[verify] === {nm}  ({len(cands)} 候选) ===")
        winner, _surv = verify_all(spec, cands, quick_n=quick_n, full_n=full_n, want=want)
        rec = {"verified": winner is not None, "time": time.strftime("%Y-%m-%d %H:%M"),
               "n_cands": len(cands)}
        if winner:
            rec.update({k: winner[k] for k in ("cyc", "cp", "res", "uops", "insns")})
            rec["exprs"] = winner["exprs"]
            print(f"   VERIFIED uops={winner['uops']} cyc={winner['cyc']:.2f}")
            for gn, e in winner["exprs"].items():
                print(f"     {gn} = {e}")
        else:
            print(f"   无赢家 ({len(cands)} 个候选全是小 K 假阳性)")
        done[nm] = rec
        _atomic_write_json(os.path.join(HERE, "superopt_results.json"), done)
    print(f"\n[verify] 完成 -> superopt_results.json")
    return done


def run(spec, nproc=7, quick_n=3000, full_n=400000):
    """交互式单 spec 全流程: 多进程搜索 + 立即两级校验 (开发/验证用)。"""
    cands = search_parallel(spec, nproc=nproc, save=True)
    if not cands:
        print("no candidate."); return None
    t0 = time.time()
    winner, _ = verify_all(spec, cands, quick_n=quick_n, full_n=full_n)
    print(f"\n-- 结论 --")
    if winner:
        print(f"   VERIFIED  cyc={winner['cyc']:.2f}  cp={winner['cp']}  "
              f"res={winner['res']:.2f}  uops={winner['uops']}  insns={winner['insns']}")
        for gn, e in winner["exprs"].items():
            print(f"   {gn} = {e}")
    else:
        print(f"   所有 {len(cands)} 个候选均未通过验证 (小 K 假阳性)")
    print(f"   verify {time.time()-t0:.1f}s")
    return winner


def make_digits4_spec(max_insn=6, K=32, tl=600.0, ops=None, consts=None, shifts=None):
    r"""print 热点: x<10000 -> 4 个十进制数字打包成 uint32
        packed = d0 | d1<<8 | d2<<16 | d3<<24   (d0 是最高位)
        再 + 0x30303030 即得 4 个 ASCII 字节, 一次 32 位 store。
    """
    rnd = random.Random(4444)
    vals = [0, 1, 9, 10, 99, 100, 999, 1000, 9999, 5000, 1234, 9090]
    while len(vals) < K: vals.append(rnd.randrange(10000))
    vals = vals[:K]

    def pack(x):
        return ((x // 1000) | ((x // 100 % 10) << 8) |
                ((x // 10 % 10) << 16) | ((x % 10) << 24))
    # 数值常量: 十进制权重 / 掩码 / 各种 "乘倒数" 魔数
    cs = {10, 100, 1000, 10000, 0xFF, 0xFFFF, 0xFF00FF, 0x00FF00FF,
          5243, 429497, 42949673, 6554, 656, 103, 1311, 0x1999999A,
          0xD1B71759, 0x51EB851F, 0xCCCCCCCD, 281475, 274878, 3518437209,
          0x346DC5D6, 0x28F5C29, 0x10624DD3, 0x431BDE83}
    # 移位量: 字节通道 + 乘倒数常见右移档
    sh = {4, 8, 11, 13, 16, 17, 19, 20, 24, 26, 27, 32, 45, 52}
    return {
        "name": "digits4", "K": K,
        "inputs": {"x": vals},
        "goals": {"packed": pack},
        "consts": consts if consts is not None else sorted(cs),
        "shifts": shifts if shifts is not None else sorted(sh),
        "ops": ops or ["add", "sub", "mul", "mulhi", "shr", "shl", "and"],
        "max_insn": max_insn, "time_limit": tl,
        "verify": {"kind": "digits4"},
    }


def make_divmod_pow2_spec(bits_d=16, bits=45, max_insn=4, K=32, tl=60.0):
    """对照组: 若 base 换成 2^k (C 路线) 的 divmod 代价下界"""
    d = 1 << bits_d
    s = make_divmod_spec(d=d, bits=bits, max_insn=max_insn, K=K, tl=tl)
    s["name"] = f"divmod_2p{bits_d}_lt2p{bits}"
    return s


def make_swar_spec(nbits=16, max_insn=5, K=12, tl=90.0, ops=None):
    """D11 SWAR 进位/借位链: 给定 G(生成) / P(传播) / cin(进位入), 求 cout。

    递推  C[j] = G[j] | (P[j] & C[j-1]),  C[-1] = cin,  cout 的 bit j = C[j]。
    G 与 P 互斥 —— 这是 div.cpp 里 (t>=BASE) 与 (t==BASE-1) 的天然性质,
    也是能把 6 条压到更短的关键前提, 必须体现在测试向量里。

    当前 div.cpp 实现 (absAdd / absSub / absMul1 三处共用):
        B = G|P; s = G+B+cin; t = s^G^B; cout = t>>1        -> 6 ops
    """
    mask = (1 << nbits) - 1
    rnd = random.Random(0x5A15)
    Gs, Ps, Cs, Os = [], [], [], []
    seeds = [(0, 0, 0), (0, mask, 1), (mask, 0, 0), (1, mask ^ 1, 0),
             (1, mask ^ 1, 1), (0, mask ^ 1, 1), (0, 0, 1), (mask >> 1, 0, 1)]
    while len(Gs) < K:
        if len(Gs) < len(seeds):
            g, p, c0 = seeds[len(Gs)]
        else:
            g = rnd.randrange(1 << nbits)
            p = rnd.randrange(1 << nbits) & ~g & mask
            c0 = rnd.randrange(2)
        c = c0; o = 0
        for j in range(nbits):
            c = ((g >> j) & 1) | (((p >> j) & 1) & c)
            o |= c << j
        Gs.append(g); Ps.append(p); Cs.append(c0); Os.append(o)
    return {
        "name": f"swar_carry{nbits}", "K": K,
        "inputs": {"G": Gs, "P": Ps, "cin": Cs},
        "goals": {"cout": Os},
        "consts": [1, mask, mask + 1, (mask << 1) | 1],
        "shifts": [1, 2, nbits, nbits - 1],
        "ops": ops or ["add", "sub", "or", "and", "xor", "not", "shr", "shl",
                       "lea2", "lea4", "andn"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "swar", "nbits": nbits},
    }


# ================================================================
# 2026-08-07 新一批: 直捣 D47 两张随机 gather 查表
#   parseTable  64KB (uint8_t[0x10000])  输入路径 str4toi
#   outTable    40KB (uint32_t[10000])   输出路径 writeTo
# 这两张表都是 data-dependent 随机 gather, 硬件预取器无法预测 (D47 里已被迫
# 手写 HINT_PREFETCH 兜 L2 延迟)。若能搜出纯 ALU 短式, 直接消灭 cache 压力。
# 全部输入域 <= 10000 种 => 夜间校验可**穷举证明**, 强于随机抽样。
# ================================================================

def make_str4toi_spec(max_insn=8, K=32, tl=1800.0, ops=None, consts=None, shifts=None):
    r"""输入热点 str4toi: 4 个 ASCII 数字 (小端 32 位 load) -> 数值。

        u = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24,  s[i] = '0'+d[i]
        v = d0*1000 + d1*100 + d2*10 + d3          (d0 是最高位)

    现状: D47 用 64KB parseTable 查两次 + 乘 100 相加。
    已知可达式 (Lemire 双乘, 7 条):
        t = u - 0x30303030
        t = ((t * 2561) >> 8) & 0x00FF00FF      # 两个 16 位 lane = [10d0+d1, 10d2+d3]
        v = ((t * 6553601) >> 16) & 0xFFFF
    给 max_insn=8 留一条余量, 看能否搜出更短的。
    """
    rnd = random.Random(0xA5C11)
    vals = [0, 1, 9, 10, 99, 100, 999, 1000, 9999, 5000, 1234, 9090, 1010, 101]
    while len(vals) < K:
        vals.append(rnd.randrange(10000))
    vals = vals[:K]

    def enc(x):
        """数值 -> 4 字节 ASCII 的小端 32 位整数"""
        d = [x // 1000, x // 100 % 10, x // 10 % 10, x % 10]
        return ((0x30 + d[0]) | ((0x30 + d[1]) << 8) |
                ((0x30 + d[2]) << 16) | ((0x30 + d[3]) << 24))

    us = [enc(x) for x in vals]
    # 数值 <-> ASCII 的桥梁常量 + 十进制权重 + lane 掩码
    cs = [10, 100, 1000, 0xFF, 0xFFFF, 0x0F0F0F0F, 0x00FF00FF, 0x000F000F,
          0xFF00FF00, 0x30303030, 2561, 6553601]
    sh = [4, 8, 10, 11, 16, 24, 32]
    return {
        "name": "str4toi", "K": K,
        "inputs": {"u": us},
        # goals 用「值列表」形式 (与 swar 同), 避免 lambda 需要反解 ASCII
        "goals": {"v": vals},
        "consts": consts if consts is not None else sorted(set(cs)),
        "shifts": shifts if shifts is not None else sh,
        "ops": ops or ["add", "sub", "mul", "shr", "shl", "and", "or",
                       "lea2", "lea4", "lea8"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "str4toi"},
    }


def make_digits2_spec(max_insn=6, K=24, tl=300.0, ops=None, consts=None, shifts=None):
    r"""outTable 的一半: y<100 -> 两个十进制数字打包
            packed = (y/10) | (y%10)<<8
    很小, 必出解; 用来标定 digits 系列的代价下界。
    """
    rnd = random.Random(0xD162)
    vals = [0, 1, 9, 10, 11, 19, 20, 50, 89, 90, 98, 99]
    while len(vals) < K:
        vals.append(rnd.randrange(100))
    vals = vals[:K]
    return {
        "name": "digits2", "K": K,
        "inputs": {"y": vals},
        "goals": {"packed": lambda y: (y // 10) | ((y % 10) << 8)},
        # 2559 = 10*256-1 是把 q|(r<<8) 融合成 (y<<8)-2559q 的关键常数
        # (已 Python 全域验证: q=(y*103)>>10; packed=(y<<8)-2559q, 100/100 通过)
        "consts": consts if consts is not None else
                  [10, 100, 256, 0xFF, 0xFFFF, 103, 205, 2559, 2560, 6554],
        "shifts": shifts if shifts is not None else [3, 5, 8, 10, 11, 16],
        "ops": ops or ["add", "sub", "mul", "shr", "shl", "and", "or", "lea2"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "digits2"},
    }


def make_digits2x2_spec(max_insn=8, K=32, tl=2400.0, ops=None):
    r"""outTable 的 SWAR 后半段: 两个 <100 的值放在 32 位 lane 里, 一次出 4 个数字。

        p = a | (b<<32)          a = x/100 (高两位), b = x%100 (低两位)
        packed = (a/10) | ((a%10)<<8) | ((b/10)<<16) | ((b%10)<<24)

    与 split100 串联即得完整 digits4, 但每段深度只要 ~5, 远比单体 digits4
    (深度 12+, 已烧 653 亿节点无解) 可搜。
    """
    rnd = random.Random(0x2222)
    pairs = [(0, 0), (99, 99), (0, 99), (99, 0), (10, 1), (1, 10),
             (50, 50), (9, 90), (90, 9), (12, 34)]
    while len(pairs) < K:
        pairs.append((rnd.randrange(100), rnd.randrange(100)))
    pairs = pairs[:K]
    ps = [a | (b << 32) for a, b in pairs]

    def goal(p):
        a = p & 0xFFFFFFFF
        b = (p >> 32) & 0xFFFFFFFF
        return ((a // 10) | ((a % 10) << 8) |
                ((b // 10) << 16) | ((b % 10) << 24))

    return {
        "name": "digits2x2", "K": K,
        "inputs": {"p": ps},
        "goals": {"packed": goal},
        "consts": [10, 100, 0xFF, 0xFFFF, 0xFFFFFFFF, 0x00FF00FF, 0xFF00FF,
                   103, 205, 6554, 26215, 0x000000FF000000FF, 0x0000FFFF0000FFFF],
        "shifts": [3, 8, 10, 11, 16, 22, 24, 32],
        "ops": ops or ["add", "sub", "mul", "shr", "shl", "and", "or",
                       "lea2", "lea4", "lea8"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "digits2x2"},
    }


def make_split100_spec(max_insn=6, K=32, tl=900.0, ops=None):
    r"""digits4 前半段: x<10000 -> 两个 32 位 lane
            p = (x/100) | ((x%100)<<32)
    """
    rnd = random.Random(0x5911)
    vals = [0, 1, 99, 100, 101, 999, 1000, 9999, 5000, 1234, 9900, 9901]
    while len(vals) < K:
        vals.append(rnd.randrange(10000))
    vals = vals[:K]
    return {
        "name": "split100", "K": K,
        "inputs": {"x": vals},
        "goals": {"p": lambda x: (x // 100) | ((x % 100) << 32)},
        "consts": [100, 10000, 0xFF, 0xFFFF, 0xFFFFFFFF, 5243, 429497,
                   42949673, 0x51EB851F, 0x28F5C29],
        "shifts": [8, 16, 19, 32, 37, 39, 45, 52],
        "ops": ops or ["add", "sub", "mul", "mulhi", "shr", "shl", "and", "or",
                       "lea2", "lea4", "lea8"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "split100"},
    }


def make_digits4_lean_spec(max_insn=12, K=32, tl=3000.0):
    r"""digits4 单体重搜, 但**精简 op/常数/移位集** —— 上一轮 653 亿节点无解的
    真因是分支因子太大 (11 op x 23 常数 x 14 移位), 深度 10 都没展完。
    这里砍到 8 op x 9 常数 x 7 移位, 换取能真正下探到深度 12。
    """
    s = make_digits4_spec(
        max_insn=max_insn, K=K, tl=tl,
        ops=["add", "sub", "mul", "shr", "shl", "and", "or", "lea2"],
        consts=[10, 100, 1000, 0xFF, 0xFFFF, 0x00FF00FF, 5243, 103, 6554],
        shifts=[8, 10, 11, 16, 19, 24, 32],
    )
    s["name"] = "digits4_lean"
    return s


def make_str4toi_lean_spec(max_insn=7, tl=600.0):
    r"""str4toi 精简搜索空间 —— 只保留「已知 7 条解」用到的 op/常数/移位。

    上一轮 digits4 653 亿节点无解的教训: 真正的敌人是**分支因子**不是深度。
    4 op x 5 常数 x 2 移位 的子空间小到能真正搜穿深度 7, 因此:
      - 若搜出 <=6 条 => 净赚 (比已知 Lemire 式更短)
      - 若只搜出 7 条  => 证明该子空间内 Lemire 式最优, 且**验证 spec 编码正确**
    这是先窄后宽策略的第一步 (窄空间给出可信基线, 再逐步放宽)。
    """
    s = make_str4toi_spec(
        max_insn=max_insn, tl=tl,
        ops=["sub", "mul", "shr", "and"],
        consts=[0x30303030, 2561, 0x00FF00FF, 6553601, 0xFFFF],
        shifts=[8, 16],
    )
    s["name"] = "str4toi_lean"
    return s


def make_str4toi_mid_spec(max_insn=7, tl=2400.0):
    """str4toi 中等空间: 在 lean 基础上放宽 op 与常数, 找 lean 空间外的更短式。"""
    s = make_str4toi_spec(
        max_insn=max_insn, tl=tl,
        ops=["add", "sub", "mul", "shr", "shl", "and", "or", "lea2"],
        consts=[0x30303030, 2561, 6553601, 0x00FF00FF, 0x0F0F0F0F,
                0xFFFF, 0xFF, 10, 100, 1000],
        shifts=[8, 16, 24, 32],
    )
    s["name"] = "str4toi_mid"
    return s


def make_digits2_lean_spec(max_insn=5, tl=600.0):
    """digits2 精简空间: 含已知 5 条解 (y<<8)-2559*((y*103)>>10) 所需的全部元素。"""
    s = make_digits2_spec(
        max_insn=max_insn, tl=tl,
        ops=["sub", "mul", "shr", "shl", "or", "and"],
        consts=[103, 2559, 10, 0xFF],
        shifts=[8, 10],
    )
    s["name"] = "digits2_lean"
    return s


def make_digits2_leanest_spec(max_insn=5, tl=300.0):
    """**端到端验收专用**: 只留已知 5 条解所需的最小元素集, 空间 <2e7 秒级搜完。

        packed = sub( shl(y,8), mul(2559, shr(mul(y,103),10)) )

    该式已 Python 全域 100/100 验证。若这里仍搜不到, 则 so_core 除 P6 之外
    还有漏解剪枝 —— 这是判定「修复是否真的通了」的黄金实验, 不许跳过。
    """
    s = make_digits2_spec(
        max_insn=max_insn, tl=tl,
        ops=["sub", "mul", "shr", "shl"],
        consts=[103, 2559],
        shifts=[8, 10],
    )
    s["name"] = "digits2_leanest"
    return s


def make_digits2x2_core_spec(max_insn=6, tl=3600.0, ops=None, consts=None,
                             shifts=None):
    r"""digits2x2 的**可搜内核**: 只到双 lane 各自打包, 不含跨 lane 合并。

        u = sub( shl(p,8), mul( and(shr(mul(p,103),10), 0xFF000000FF), 2559) )
        u 的低 lane = pk(a), 高 lane = pk(b),  pk(v) = (v/10)|((v%10)<<8)

    已 Python 全域 10000/10000 验证 (6 条)。
    为什么拆: 完整 digits2x2 已知解 9 条, 深度 9 x 分支 48 现实中打不穿;
    而末尾 3 条合并 (or(u, shr(u,16)) 再 and 0xFFFFFFFF) 是平凡恒等, 无搜索价值。
    把预算全压在真正非平凡的 6 条内核上。
    """
    rnd = random.Random(0x2222)
    pairs = [(0, 0), (99, 99), (0, 99), (99, 0), (10, 1), (1, 10),
             (50, 50), (9, 90), (90, 9), (12, 34)]
    K = 32
    while len(pairs) < K:
        pairs.append((rnd.randrange(100), rnd.randrange(100)))
    pairs = pairs[:K]

    def pk(v):
        return (v // 10) | ((v % 10) << 8)

    return {
        "name": "digits2x2_core", "K": K,
        "inputs": {"p": [a | (b << 32) for a, b in pairs]},
        "goals": {"u": lambda p: pk(p & 0xFFFFFFFF) | (pk((p >> 32) & 0xFFFFFFFF) << 32)},
        # 默认 = **最窄**: 只含已知 6 条解真正用到的元素。
        # 2026-08-07 实测: 加上 or/10/0x0F0000000F 后分支因子 48, 深度 6
        # 烧 30.9 亿节点 420s 仍「有分片超时」没展完 -> 验收关必须窄到能搜穿,
        # 放宽留给后面的 _w1 片。
        "consts": consts if consts is not None else
                  [103, 2559, 0x000000FF000000FF],
        "shifts": shifts if shifts is not None else [8, 10],
        "ops": ops or ["mul", "shr", "shl", "sub", "and"],
        "max_insn": max_insn, "time_limit": tl,
        "max_cands": 400,
        "verify": {"kind": "digits2x2_core"},
    }


def make_split100_leanest_spec(max_insn=6, tl=600.0):
    r"""**验收/切片专用**: 只留已知 6 条解所需的最小元素集。

        p = or( shr(mul(x,5243),19), shl( sub(x, mul(q,100)), 32) )

    已 Python 全域 10000/10000 验证 (5243>>19 与 42949673>>32 两条都通)。
    2026-08-07 教训: 完整 split100 (11 op x 10 常数 x 8 移位) 94.9 亿节点
    20min 都没展完 —— 大空间必须先用本 spec 拿可信基线, 再按常数分片放宽。
    """
    s = make_split100_spec(max_insn=max_insn, tl=tl,
                           ops=["mul", "shr", "shl", "sub", "or"])
    s["consts"] = [100, 5243]
    s["shifts"] = [19, 32]
    s["name"] = "split100_leanest"
    return s


def make_digits2x2_leanest_spec(max_insn=9, tl=3600.0):
    r"""**验收/切片专用**: 含已知 9 条 SWAR 解的最小空间。

        t = shr(mul(p,103),10) ; Q = and(t, 0x000000FF000000FF)
        u = sub( shl(p,8), mul(Q,2559) )
        packed = and( or(u, shr(u,16)), 0xFFFFFFFF )

    已 Python 全域 10000/10000 验证 (a,b 各 0..99)。

    !! 2026-08-07 发现的重大空间缺陷 !!
        原 make_digits2x2_spec 的 consts **既没有 2559 也没有 mask 常数**,
        且 max_insn=8 < 9 —— 已知解根本不在空间内, 搜多久都必然 0 候选。
        这与 P6 剪枝 bug 是同型陷阱 (结论「无解」其实是「空间里没有」)。
        任何新 spec 上线前必须先做本函数这样的「含已知解最小空间」验收。
    """
    s = make_digits2x2_spec(max_insn=max_insn, tl=tl,
                            ops=["mul", "shr", "shl", "sub", "or", "and"])
    s["consts"] = [103, 2559, 0x000000FF000000FF, 0xFFFFFFFF]
    s["shifts"] = [8, 10, 16]
    s["name"] = "digits2x2_leanest"
    return s


SPECS = {
    "divmod":     lambda: make_divmod_spec(),
    "divmod_q":   lambda: make_divmod_spec(only_q=True, max_insn=3),
    "divmod_r":   lambda: make_divmod_spec(only_r=True, max_insn=3),
    "divmod_p2":  lambda: make_divmod_pow2_spec(),
    "carry":      lambda: make_carrystep_spec(),
    "digits4":    lambda: make_digits4_spec(),
    "swar":       lambda: make_swar_spec(),
    "swar4":      lambda: make_swar_spec(max_insn=4),
    "swar3":      lambda: make_swar_spec(max_insn=3),
    # --- 2026-08-07 新增: 查表消灭战 ---
    "str4toi":       lambda: make_str4toi_spec(),
    "str4toi_lean":  lambda: make_str4toi_lean_spec(),
    "str4toi_lean6": lambda: make_str4toi_lean_spec(max_insn=6, tl=1200.0),
    "str4toi_mid":   lambda: make_str4toi_mid_spec(),
    "digits2_lean":  lambda: make_digits2_lean_spec(),
    "digits2_leanest": lambda: make_digits2_leanest_spec(),
    "digits2":     lambda: make_digits2_spec(),
    "digits2x2":   lambda: make_digits2x2_spec(),
    "digits2x2_leanest": lambda: make_digits2x2_leanest_spec(),
    "digits2x2_core":    lambda: make_digits2x2_core_spec(),
    "split100":    lambda: make_split100_spec(),
    "split100_leanest":  lambda: make_split100_leanest_spec(),
    "digits4_lean": lambda: make_digits4_lean_spec(),
}

# ---------------------------------------------------------------- SEM 一致性自测
# so_core --test-sem 打印每条指令在结构化+随机输入下的 eval1 输出; 这里用 Python
# SEM 重算对照, 堵住「SEM 与 eval1 同时对某条指令写错成一样」的漏网风险。
UNARY = {"not", "neg", "blsi", "blsr", "blsmsk", "tzcnt", "lzcnt", "popcnt"}


def test_sem():
    exe = build_core()
    r = subprocess.run([exe, "--test-sem"], capture_output=True, text=True)
    if r.returncode != 0:
        print("[test_sem] so_core 失败:"); print(r.stderr); return False
    bad, n = 0, 0
    for line in r.stdout.splitlines():
        m = re.match(r"SEMCHECK (\S+) (\d+) (\d+) (\d+)", line)
        if not m:
            continue
        name, a, b, out = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
        sem_name = name if name in SEM else name + "_"
        try:
            exp = SEM[sem_name](a) if name in UNARY else SEM[sem_name](a, b)
        except Exception as e:
            bad += 1
            if bad <= 10:
                print(f"  EXC {name} a={a} b={b}: {e}")
            continue
        n += 1
        if exp != out:
            bad += 1
            if bad <= 10:
                print(f"  MISMATCH {name} a={a} b={b}: eval1={out} SEM={exp}")
    print(f"[test_sem] checked={n} mismatches={bad} -> {'OK' if bad == 0 else 'FAIL'}")
    return bad == 0


# ---------------------------------------------------------------- 自驱动 autopilot
# 遍历注册的 spec, 逐个以断点续搜模式运行, 收集 VERIFIED 赢家, 写 superopt_results.json。
# 已 VERIFIED 的 spec 自动跳过 -> 中断后再跑只续搜未完成的部分, 无需监督。
def autopilot(spec_names=None, nproc=7):
    """白天自驱动: 遍历注册 spec, 逐个多进程分片搜索并把候选原子落盘候选池;
    *不做* 大规模校验 (留待夜间 `verify` 统一处理)。无人值守, 只吃算力。"""
    names = [n for n in (spec_names or list(SPECS)) if n in SPECS]
    print(f"[autopilot] 白天搜索模式: {len(names)} 个 spec, nproc={nproc}, 只搜集候选")
    for nm in names:
        print(f"\n[autopilot] === {nm} ===")
        try:
            uniq = search_parallel(SPECS[nm](), nproc=nproc, save=True)
        except Exception as e:
            print(f"[autopilot] {nm} 出错: {e}")
            _acct_update(nm, {"status": "error", "err": str(e)[:200]})
            continue
        print(f"[autopilot] {nm} -> 候选池已存 {len(uniq)} 条")
    print(f"\n[autopilot] 全部完成. 候选池 -> {CAND_DIR}\\  记账 -> {ACCT_PATH}")
    print(f"[autopilot] 夜间校验:  python so.py verify   (对全部候选池做 40 万样本校验)")
    return True


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] == "list":
        print("specs:", ", ".join(SPECS)); sys.exit(0)
    cmd = sys.argv[1]
    if cmd == "autopilot":
        autopilot(sys.argv[2:] if len(sys.argv) > 2 else None)
    elif cmd == "search":
        # 白天: 多进程搜索, 只存候选池, 不校验。 用法: so.py search [spec...]
        specs = [s for s in sys.argv[2:] if s in SPECS] or list(SPECS)
        for nm in specs:
            search_parallel(SPECS[nm]())
    elif cmd == "verify":
        # 夜间: 对候选池做大规模校验。 用法: so.py verify [name]
        verify_pool(sys.argv[2] if len(sys.argv) > 2 else None)
    elif cmd == "test_sem":
        ok = test_sem()
        sys.exit(0 if ok else 1)
    elif cmd == "hotspots":
        # 热点检测 (callgrind+objdump, 仅 VM) 已归档于 lc_bench/vm/_deprecated/superopt_pipeline.py
        print("热点检测 (callgrind+objdump, 仅 VM 可跑) 见: lc_bench/vm/_deprecated/superopt_pipeline.py")
        print("autopilot 已覆盖的已知热点核:")
        for nm in SPECS:
            print("   -", nm)
    elif cmd in SPECS:
        run(SPECS[cmd]())
    else:
        print("unknown:", cmd); sys.exit(1)
