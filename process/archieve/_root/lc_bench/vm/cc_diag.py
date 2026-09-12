import os
d = os.path.expanduser('~/lcp/big_integer/addition_of_big_integers/in')
for name in ['carry_chain_00','carry_chain_01','carry_chain_02','carry_chain_03','medium_00','large_00']:
    p = os.path.join(d, name + '.in')
    data = open(p, 'rb').read()
    toks = data.split()
    npairs = len(toks) // 2
    lens = [len(t) for t in toks]
    # all-9s numbers (carry-chain operands)
    all9 = sum(1 for t in toks if t and t == b'9' * len(t))
    # numbers whose last 20 digits are all 9 (long carry tail)
    tail20 = sum(1 for t in toks if len(t) >= 20 and t[-20:] == b'9' * 20)
    # numbers whose first 20 digits are all 9 (long carry head)
    head20 = sum(1 for t in toks if len(t) >= 20 and t[:20] == b'9' * 20)
    import statistics
    med = statistics.median(lens) if lens else 0
    print(f"{name}: sz={len(data)} toks={len(toks)} pairs={npairs} maxlen={max(lens)} medlen={med} all9={all9} tail20_9={tail20} head20_9={head20}")
