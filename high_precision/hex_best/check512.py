import subprocess
out = subprocess.run(["./genbin/burnikel_ziegler_bound", "2"], capture_output=True, text=True).stdout
lines = out.split("\n")
A, B = lines[513].split()
a = int(A, 16); b = int(B, 16)
eq, er = a // b, a % b
inp = "1\n" + A + " " + B + "\n"
r = subprocess.run(["./div_base16_zn"], input=inp, capture_output=True, text=True)
q, rr = r.stdout.split()
print("oracle_q_head", hex(eq)[:50])
print("bin_q_head   ", q[:50])
print("oracle_r_head", hex(er)[:50])
print("bin_r_head   ", rr[:50])
print("Q_MATCH", int(q, 16) == eq, "R_MATCH", int(rr, 16) == er)
