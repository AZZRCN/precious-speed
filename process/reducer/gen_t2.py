import sys, random, os

# Generate a T=2 (or T=N) file of LARGE hex big-integer divisions that exercise
# the FFT/Barrett path (na,nb large), to reproduce the v27 multi-query segfault.
# Format consumed by main(): first token = T, then T pairs of hex strings (A B).

def rand_hex(nlimbs, rng):
    # produce nlimbs*16 hex chars, top limb nonzero
    chars = '0123456789abcdef'
    s = []
    for _ in range(nlimbs):
        chunk = ''.join(rng.choice(chars) for _ in range(16))
        s.append(chunk)
    s[0] = rng.choice('123456789abcdef') + s[0][1:]  # force nonzero top
    return ''.join(s)

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 12345
    T = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    na = int(sys.argv[3]) if len(sys.argv) > 3 else 100000
    nb = int(sys.argv[4]) if len(sys.argv) > 4 else 50000
    out = sys.argv[5] if len(sys.argv) > 5 else 't2.txt'
    rng = random.Random(seed)
    lines = [str(T)]
    for t in range(T):
        # A >= B so it goes through newton/bz; both large
        A = rand_hex(na, rng)
        # B slightly smaller, top nonzero
        B = rand_hex(nb, rng)
        lines.append(A)
        lines.append(B)
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('wrote', out, 'T=%d na=%d nb=%d size=%.1fMB' % (T, na, nb, os.path.getsize(out)/1e6))

if __name__ == '__main__':
    main()
