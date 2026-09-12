# Correct base conversion: hex string <-> base-2^17 limbs (LSB-first limb array)
HEX = "0123456789ABCDEF"

def hex_to_limbs(hx):
    hx = hx.lower().lstrip('0') or '0'
    limbs = []
    cur = 0; curbits = 0
    for ch in reversed(hx):          # LSB -> MSB
        d = int(ch, 16)
        cur |= d << curbits
        curbits += 4
        while curbits >= 17:
            limbs.append(cur & 0x1FFFF)   # lowest 17-bit limb
            cur >>= 17
            curbits -= 17
    if curbits > 0 or not limbs:
        limbs.append(cur & 0x1FFFF)
    return limbs

def limbs_to_hex(limbs):
    if not limbs or (len(limbs) == 1 and limbs[0] == 0):
        return '0'
    out = []   # LSB-first hex digits
    acc = 0; bits = 0
    for L in limbs:                  # LSB -> MSB
        acc |= L << bits
        bits += 17
        while bits >= 4:
            out.append(HEX[acc & 0xF])
            acc >>= 4
            bits -= 4
    if bits > 0:
        out.append(HEX[acc & ((1 << bits) - 1)])
    s = ''.join(reversed(out)).lstrip('0') or '0'
    return s

for t in ['0', '1', 'ABC', 'ABCDEF', '123456789ABCDEF', 'FFFFFFFFFFFFFFFF',
          'deadbeefcafe', '1' * 40, 'a' * 68, 'ABCDEF' * 5]:
    L = hex_to_limbs(t)
    back = limbs_to_hex(L)
    ok = (back.lower() == t.lower())
    print(f"{t[:24]:26} -> n={len(L):3} -> {back[:24]:26} {'OK' if ok else 'FAIL'}")
