import os, sys, random, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh

HEX = '0123456789abcdef'
def rand_hex(rng, nhex):
    s = ''.join(rng.choice(HEX) for _ in range(nhex))
    s = rng.choice('123456789abcdef') + s[1:]
    return s

def run_case(ws, binname, a, b):
    local = "/tmp/mulg_in.txt"
    with open(local,'wb') as f:
        f.write(f"{a}\n{b}\n".encode())
    vm_ssh.put(local, f"{ws}/mulg_in.txt")
    rc, out, err = vm_ssh.run(f"cd {ws} && ./{binname} < mulg_in.txt")
    return out.strip()

def main():
    ws = "/home/azzr/precious_speed/dbg"
    cases = [(3292,3010),(6250,6250),(3750,2500),(282,3010),(150,150),(5000,5000),(100,3000)]
    rng = random.Random(12345)
    allok = True
    for (na, nb) in cases:
        a = rand_hex(rng, 16*na)
        b = rand_hex(rng, 16*nb)
        exp = int(a,16)*int(b,16)
        try:
            gd = int(run_case(ws, "mulgtest.bin", a, b), 16)
        except ValueError:
            gd = None
        try:
            gm = int(run_case(ws, "mulgtest_mul.bin", a, b), 16)
        except ValueError:
            gm = None
        od = (gd == exp)
        om = (gm == exp)
        allok = allok and od and om
        print(f"na={na} nb={nb}  div={'OK' if od else 'BAD'}  mul={'OK' if om else 'BAD'}")
    print("ALL OK" if allok else "SOME BAD")


if __name__ == "__main__":
    main()
