import os, sys, random
sys.path.insert(0, os.path.dirname(__file__))
import vm_ssh

HEX = '0123456789abcdef'
def rand_hex(rng, n):
    s = ''.join(rng.choice(HEX) for _ in range(n))
    if n >= 2:
        s = rng.choice('123456789abcdef') + s[1:]
    return s

def py_div(a, b):
    q, r = divmod(int(a, 16), int(b, 16))
    return format(q, 'x'), format(r, 'x')

def main():
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    rng = random.Random(20260813)
    cases = []
    for _ in range(N):
        nb = rng.randint(64, 4000)
        na = rng.randint(nb, 2 * nb)
        a = rand_hex(rng, na * 16)
        b = rand_hex(rng, nb * 16)
        if int(a, 16) < int(b, 16):
            a, b = b, a
        cases.append((a, b))
    d = os.path.dirname(__file__)
    in_dir = os.path.join(d, "cases_seg")
    os.makedirs(in_dir, exist_ok=True)
    for i, (a, b) in enumerate(cases):
        with open(os.path.join(in_dir, f"c{i}.in"), "w") as f:
            f.write(f"1\n{a} {b}\n")
    # upload all
    vm_ssh.run("rm -rf /home/azzr/precious_speed/dbg/seg && mkdir -p /home/azzr/precious_speed/dbg/seg")
    vm_ssh.putdir(in_dir, "/home/azzr/precious_speed/dbg/seg")
    # run each on VM, capture rc + first output line; if segfault, rc!=0
    rc, out, err = vm_ssh.run(
        "cd /home/azzr/precious_speed/dbg/seg && "
        "for f in c*.in; do "
        "  ./../../hex_best/div.bin < \"$f\" > \"${f%.in}.out\" 2>/dev/null; "
        "  echo \"$f rc=$?\"; "
        "done")
    # fetch outputs
    vm_ssh.run("cd /home/azzr/precious_speed/dbg/seg && tar czf seg.tar.gz c*.out")
    vm_ssh.get("/home/azzr/precious_speed/dbg/seg/seg.tar.gz", os.path.join(d, "seg.tar.gz"))
    import tarfile
    got = {}
    with tarfile.open(os.path.join(d, "seg.tar.gz")) as tf:
        for m in tf.getmembers():
            if m.name.endswith(".out"):
                got[m.name] = tf.extractfile(m).read().decode().strip().lower()
    for i, (a, b) in enumerate(cases):
        fn = f"c{i}.out"
        eq, er = py_div(a, b)
        g = got.get(fn, "")
        if g != f"{eq} {er}":
            print(f"PROBLEM idx={i} na={len(a)//16} nb={len(b)//16} -> got='{g[:40]}'")
            print(f"A={a[:60]}...")
            print(f"B={b[:60]}...")
            with open(os.path.join(d, "fail.txt"), "w") as f:
                f.write(f"{a}\n{b}\n")
            return
    print(f"ALL {N} OK")

if __name__ == "__main__":
    main()
