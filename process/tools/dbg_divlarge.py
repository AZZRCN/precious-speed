import os, sys, random
sys.set_int_max_str_digits(20000000)
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh

HEX = '0123456789abcdef'
def rand_hex(rng, n):
    s = ''.join(rng.choice(HEX) for _ in range(n))
    if n >= 2:
        s = rng.choice('123456789abcdef') + s[1:]
    return s

def main():
    rng = random.Random((hash(('div','large')) & 0xffffffff) ^ 0x9E3779B9)
    T = 8
    cases = []
    for _ in range(T):
        a = rand_hex(rng, rng.randint(40000, 60000))
        b = rand_hex(rng, rng.randint(40000, 60000))
        if int(a, 16) < int(b, 16):
            a, b = b, a
        cases.append((a, b))
    local = os.path.join(os.path.dirname(__file__), "divlarge_in.txt")
    inp = f"{T}\n" + "".join(f"{a} {b}\n" for a, b in cases)
    with open(local, "wb") as f:
        f.write(inp.encode())
    vm_ssh.put(local, "/home/azzr/precious_speed/dbg/divlarge_in.txt")
    rc, out, err = vm_ssh.run(
        "cd /home/azzr/precious_speed/hex_best && ./div.bin < /home/azzr/precious_speed/dbg/divlarge_in.txt")
    got = [l.strip() for l in out.strip().split("\n")]
    nfail = 0
    for i, (a, b) in enumerate(cases):
        A = int(a, 16); B = int(b, 16)
        eq, er = A // B, A % B
        g = got[i].lower() if i < len(got) else "<none>"
        exp = f"{eq:x} {er:x}"
        if g != exp:
            nfail += 1
            gq = int(got[i].split()[0], 16) if i < len(got) and got[i] else 0
            gr = int(got[i].split()[1], 16) if i < len(got) and len(got[i].split())>1 else 0
            qd = gq - eq; rd = gr - er
            print(f"MISMATCH idx={i} na={len(a)//16} nb={len(b)//16} na>2nb? {len(a)//16 > 2*(len(b)//16)}")
            print(f"  len(q): got={gq.bit_length()} exp={eq.bit_length()}   len(r): got={gr.bit_length()} exp={er.bit_length()}")
            print(f"  qdiff_bits={qd.bit_length()} rdiff_bits={rd.bit_length()}  sign(qd)={1 if qd>0 else -1 if qd<0 else 0}")
            print(f"  gq*B+gr==A? {gq*B+gr==A}")
            print(f"  gr<B? {gr<B}  (remainder must be < B)")
            print(f"  A[:24]={a[:24]} B[:24]={b[:24]}")
            if nfail >= 3:
                break
    print("ALL PASS" if nfail==0 else f"{nfail} FAILURES")

if __name__ == "__main__":
    main()
