import os
d = os.path.expanduser('~/lcp/big_integer/addition_of_big_integers/in')

def toks_of(name):
    p = os.path.join(d, name + '.in')
    return open(p,'rb').read().split()

def stat_neg(name):
    toks = toks_of(name)
    n = len(toks)
    neg = sum(1 for t in toks if t[:1] == b'-')
    pairs = list(zip(toks[0::2], toks[1::2]))
    # b negative -> operator+= triggers subtraction (a.add(b,true))
    bneg = sum(1 for a,b in pairs if b[:1] == b'-')
    aneg = sum(1 for a,b in pairs if a[:1] == b'-')
    print(f"{name}: toks={n} neg_tok={neg} ({100*neg/n:.1f}%) pairs={len(pairs)} a_neg={aneg} b_neg={bneg} (b_neg->sub path)")

for name in ['carry_chain_00','carry_chain_01','carry_chain_02','carry_chain_03','medium_00','large_00']:
    stat_neg(name)

# dump samples of carry_chain_02 first 3 pairs (with sign)
t = toks_of('carry_chain_02')
pairs = list(zip(t[0::2], t[1::2]))
print("--- carry_chain_02 samples ---")
for i in range(3):
    a,b = pairs[i]
    print(f"  pair{i}: a({len(a)})={a[:50].decode()}  b({len(b)})={b[:50].decode()}")
