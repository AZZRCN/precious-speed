def region(path, start_marker, end_marker, end_inc=True):
    lines = open(path, encoding='utf-8').read().splitlines()
    s = next(i for i,l in enumerate(lines) if start_marker in l)
    e = next(i for i,l in enumerate(lines) if end_marker in l)
    return lines[s:e+1] if end_inc else lines[s:e]

# mul.cpp: fft namespace (155) -> end of mul_fft merge (line 968 'merge_b2(c, FB, u, k);')
m = region('mul.cpp', 'namespace fft {', 'merge_b2(c, FB, u, k);')
# div.cpp: fft namespace (196) -> before mulg marker (1013)
d = region('div.cpp', 'namespace fft {', '// c[0..na+nb) = a*b')
open('div_region.txt','w').write('\n'.join(d))
open('mul_region.txt','w').write('\n'.join(m))
print("mul.cpp region:", len(m), "lines")
print("div.cpp region:", len(d), "lines")
