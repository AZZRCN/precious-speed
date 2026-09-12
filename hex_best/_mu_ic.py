import subprocess, random, os

EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"

random.seed(20260813)
def gen_case(nb, na_ratio):
    B = random.getrandbits(64 * nb)
    if B < (1 << (64 * (nb - 1))): B |= (1 << (64 * (nb - 1)))
    na = max(3 * nb, int(nb * na_ratio))
    A = random.getrandbits(64 * na)
    if A < (1 << (64 * (na - 1))): A |= (1 << (64 * (na - 1)))
    if A < B: A += B
    return A, B

cases = [(200,3.0),(200,5.0),(256,3.0),(300,8.0),(512,3.0),(1000,5.0)]
for (nb, r) in cases:
    A, B = gen_case(nb, r)
    inp = "1\n" + format(A,'X') + " " + format(B,'X') + "\n"
    env = {**os.environ, "MU":"1"}
    proc = subprocess.run([EXE], input=inp, capture_output=True, text=True, env=env)
    parts = proc.stdout.strip().split()
    qgot = int(parts[0],16); rgot = int(parts[1],16)
    qexp = A//B; rexp = A%B
    ic = (qgot*B + rgot == A)
    print(f"nb={nb} r={r}: qok={qgot==qexp} rok={rgot==rexp} q*B+r==A={ic} "
          f"qgot_top_limb_nonzero={qgot>= (1<<(64*( (A.bit_length()//64+1) - nb -1)))}")
