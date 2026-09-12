#!/usr/bin/env python3
"""本地驱动: 在 VM 上复现 Library Checker 官方测试数据 (三道 HEX 题).

流程: 解析 info.toml -> 生成 params.h -> 打 tar 上传 -> VM 编译 gen -> 按 seed 跑出 .in
"""
import io, os, re, sys, tarfile, posixpath
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

REPO = 'E:/library-checker-problems-master'
WS = '/home/azzr/hexbench'
PROBS = {
    'add': 'addition_of_hex_big_integers',
    'mul': 'multiplication_of_hex_big_integers',
    'div': 'division_of_hex_big_integers',
}

def parse_info(path):
    txt = open(path, encoding='utf-8').read()
    tests = []
    for m in re.finditer(r'\[\[tests\]\]\s*\n\s*name\s*=\s*[\'"]([^\'"]+)[\'"]\s*\n\s*number\s*=\s*(\d+)', txt):
        tests.append((m.group(1), int(m.group(2))))
    params = {}
    pm = re.search(r'\[params\]\s*\n((?:\s*\w+\s*=.*\n?)+)', txt)
    if pm:
        for line in pm.group(1).strip().split('\n'):
            k, v = line.split('=', 1)
            params[k.strip()] = v.strip()
    return tests, params

def params_h(params):
    out = []
    for k, v in params.items():
        if re.fullmatch(r'-?\d+', v):
            out.append(f'#define {k} (long long){v}')
        else:
            out.append(f'#define {k} {v}')
    return '\n'.join(out) + '\n'

def main():
    buf = io.BytesIO()
    tf = tarfile.open(fileobj=buf, mode='w:gz')

    def add_bytes(arc, data: bytes):
        ti = tarfile.TarInfo(arc); ti.size = len(data); ti.mode = 0o644
        tf.addfile(ti, io.BytesIO(data))

    tf.add(f'{REPO}/common/random.h', arcname='common/random.h')
    plan = {}
    for short, full in PROBS.items():
        base = f'{REPO}/big_integer/{full}'
        tests, params = parse_info(f'{base}/info.toml')
        plan[short] = tests
        add_bytes(f'{short}/params.h', params_h(params).encode())
        for aux in ('base.hpp',):
            if os.path.exists(f'{base}/{aux}'):
                tf.add(f'{base}/{aux}', arcname=f'{short}/{aux}')
        for name, num in tests:
            if name.endswith('.in'):
                stem = name[:-3]
                for i in range(num):
                    fn = f'{stem}_{i:02d}.in'
                    src = f'{base}/gen/{fn}'
                    if os.path.exists(src):
                        tf.add(src, arcname=f'{short}/gen/{fn}')
                    else:
                        print(f'  !! missing {src}')
                continue
            src = f'{base}/gen/{name}'
            if os.path.exists(src):
                tf.add(src, arcname=f'{short}/gen/{name}')
            else:
                print(f'  !! missing {src}')
    tf.close()
    data = buf.getvalue()
    print(f'tarball {len(data)} bytes')

    local_tar = 'D:/hex_precious_speed/work/_gendata.tgz'
    os.makedirs(os.path.dirname(local_tar), exist_ok=True)
    open(local_tar, 'wb').write(data)
    vmctl.put(local_tar, f'{WS}/gendata.tgz')
    vmctl.run(f'cd {WS} && rm -rf lcgen && mkdir -p lcgen && tar xzf gendata.tgz -C lcgen && ls lcgen && echo EXTRACT_OK')

    # compile all gens + run seeds
    script = ['set -e']
    for short, tests in plan.items():
        script.append(f'mkdir -p {WS}/data/{short}')
        for name, num in tests:
            stem = name.rsplit('.', 1)[0]
            if name.endswith('.in'):
                for i in range(num):
                    fn = f'{stem}_{i:02d}.in'
                    script.append(f'cp {WS}/lcgen/{short}/gen/{fn} {WS}/data/{short}/{fn}')
                continue
            script.append(
                f'g++ -O2 -std=c++20 -I {WS}/lcgen/common -o {WS}/lcgen/{short}/gen/{stem} '
                f'{WS}/lcgen/{short}/gen/{name}')
            for i in range(num):
                script.append(
                    f'{WS}/lcgen/{short}/gen/{stem} {i} > {WS}/data/{short}/{stem}_{i:02d}.in')
    script.append('echo GEN_ALL_DONE')
    body = ' && '.join(script)
    vmctl.run(f'bash -c {shq(body)}', timeout=1800)
    vmctl.run(f'for p in add mul div; do echo "== $p =="; ls -la {WS}/data/$p | head -50; done')

def shq(s):
    return "'" + s.replace("'", "'\\''") + "'"

if __name__ == '__main__':
    main()
