import os, statistics
d = os.path.expanduser('~/lcp/big_integer/addition_of_big_integers/in')

def buckets(lens):
    import collections
    c = collections.Counter()
    for L in lens:
        if L < 19: c['<19'] += 1
        elif L < 64: c['19-63'] += 1
        elif L < 256: c['64-255'] += 1
        elif L < 1024: c['256-1023'] += 1
        else: c['>=1024'] += 1
    return dict(c)

for name in ['carry_chain_00','carry_chain_02']:
    p = os.path.join(d, name + '.in')
    toks = open(p,'rb').read().split()
    pairs = list(zip(toks[0::2], toks[1::2]))
    al = [len(a) for a,b in pairs]; bl = [len(b) for a,b in pairs]
    # both large (>=64 digits) -> full avx2 absAdd/absSub
    both_large = sum(1 for a,b in pairs if len(a)>=64 and len(b)>=64)
    # sign differ -> absSub path
    sdiff = sum(1 for a,b in pairs if (a[:1]==b'-') != (b[:1]==b'-'))
    # longest number
    all_t = toks
    longest = max(all_t, key=len)
    print(f"{name}: n_pairs={len(pairs)} both_large(>=64d)={both_large} sign_diff={sdiff}")
    print(f"   a_len buckets={buckets(al)}")
    print(f"   b_len buckets={buckets(bl)}")
    print(f"   longest num ({len(longest)} d): head={longest[:40].decode()} ... tail={longest[-40:].decode()}")
    # sample a large pair
    for a,b in pairs:
        if len(a) >= 200 and len(b) >= 200:
            print(f"   big pair: a({len(a)})={a[:30].decode()}..{a[-20:].decode()}  b({len(b)})={b[:30].decode()}..{b[-20:].decode()}")
            break
