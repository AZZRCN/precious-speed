import os, sys
sys.set_int_max_str_digits(10000000)
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh

def main():
    p = os.path.join(os.path.dirname(__file__), "fail.txt")
    with open(p) as f:
        a = f.readline().strip(); b = f.readline().strip()
    local = os.path.join(os.path.dirname(__file__), "one_in.txt")
    with open(local, "w") as f:
        f.write(f"1\n{a} {b}\n")
    vm_ssh.put(local, "/home/azzr/precious_speed/dbg/one_in.txt")
    rc, out, err = vm_ssh.run(
        "cd /home/azzr/precious_speed/hex_best && ./div.bin < /home/azzr/precious_speed/dbg/one_in.txt")
    got = out.strip().split()
    A = int(a, 16); B = int(b, 16)
    eq, er = A // B, A % B
    gq = int(got[0], 16); gr = int(got[1], 16)
    print(f"na={len(a)//16} nb={len(b)//16}")
    print(f"Q exp (hex)={format(eq,'x')[:60]}...")
    print(f"Q got (hex)={format(gq,'x')[:60]}...")
    print(f"Q diff (int)={gq-eq}   (negative => got smaller)")
    print(f"R diff (int)={gr-er}")
    recomputed = gq * B + gr
    print(f"gq*B+gr == A ? {recomputed == A}  (diff={recomputed - A})")

if __name__ == "__main__":
    main()
