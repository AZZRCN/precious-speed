import os
d = r'd:\precious_speed\lc_bench\cases\mul'
print('case                          queries  max_len  file_size')
for f in sorted(os.listdir(d)):
    if not f.endswith('.in'):
        continue
    p = os.path.join(d, f)
    sz = os.path.getsize(p)
    with open(p, 'rb') as fh:
        data = fh.read()
    nl = data.find(b'\n')
    if nl < 0:
        print(f'{f:30s} {"-":>8s} {"-":>8s} {sz:>10d}')
        continue
    try:
        q = int(data[:nl].decode())
    except Exception:
        q = -1
    lines = data[nl+1:].split(b'\n')
    max_len = 0
    for ln in lines:
        s = ln.strip()
        if not s:
            continue
        parts = s.split()
        for x in parts:
            if x and (x[0:1].isdigit() or x[0:1] == b'-'):
                v = x.lstrip(b'-')
                if v.isdigit():
                    if len(v) > max_len:
                        max_len = len(v)
    print(f'{f:30s} {q:>8d} {max_len:>8d} {sz:>10d}')
