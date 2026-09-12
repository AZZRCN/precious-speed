def ceil2(x):
    m = 1
    while m < x:
        m <<= 1
    return m


def ladder(k0, adaptive=False, label=""):
    print(f"--- {label} k0={k0} adaptive={adaptive} ---")
    k = k0
    lvl = 0
    tot_on = 0
    tot = 0
    while k > 64:
        p2 = ceil2(k + 1)
        s = (k - 1) // 2
        s_cyc = 2 * k - p2 if 2 * k > p2 else 0
        used_s = s
        if adaptive and k >= 4096 and s > s_cyc and s_cyc >= (2 * k) // 5:
            used_s = s_cyc
        rn = k - used_s
        on = (p2 >= k + 1) and (p2 <= k + rn) and (k >= 4096)
        tot += 1
        if on:
            tot_on += 1
        print(f" L{lvl}: k={k:7d} p2={p2:7d} ratio={k/p2:.3f} s={used_s:6d} "
              f"rn={rn:6d} win=[{k+1},{k+rn}] cyclic={'ON ' if on else 'off'}")
        k = rn
        lvl += 1
        if lvl > 12:
            break
    print(f" => cyclic ON {tot_on}/{tot}\n")


if __name__ == "__main__":
    ladder(83334, False, "D4 current")
    ladder(83334, True, "adaptive s")
    ladder(65535, False, "D8b (in forced 65535)")
