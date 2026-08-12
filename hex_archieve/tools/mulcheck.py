#!/usr/bin/env python3
# MUL 正确性闸门 oracle: int(a,16)*int(b,16), 大小写不敏感, 支持符号。
# 用法: python3 mulcheck.py run <bin> <case.in> <case.exp>
import sys, subprocess

def run_case(binpath, inf):
    data = open(inf).read().split()
    if not data:
        return ''
    T = int(data[0])
    cases = data[1:]
    inp = (str(T) + '\n' + '\n'.join(' '.join(cases[i*2:i*2+2]) for i in range(T)) + '\n').encode()
    return subprocess.run([binpath], input=inp, capture_output=True).stdout.decode(errors='replace')

def check_case(binpath, inf, expf):
    out = run_case(binpath, inf).split()
    exp = open(expf).read().split()
    if len(out) != len(exp):
        return False, 'len %d vs %d' % (len(out), len(exp))
    bad = 0
    ex = []
    for i, (o, e) in enumerate(zip(out, exp)):
        if o.upper() != e.upper():
            bad += 1
            if bad <= 3:
                ex.append((i, e, o))
    return (bad == 0), ('bad=%d %s' % (bad, ex))

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'run':
        ok, msg = check_case(sys.argv[2], sys.argv[3], sys.argv[4])
        print(('OK' if ok else 'BAD'), msg)
        sys.exit(0 if ok else 1)
    else:
        print('usage: mulcheck.py run <bin> <in> <exp>')
        sys.exit(2)
