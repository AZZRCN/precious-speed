import paramiko
c=paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.1.55',username='azzr',password='1234',timeout=20)
cases = ["length_ratio_integer_02","length_ratio_integer_00","a_max_b_random_02",
         "r_nearly_zero_01","burnikel_ziegler_bound_02","medium_02"]
indir = "~/lcp/big_integer/division_of_big_integers/in"
for f in cases:
    cmd = "wc -c < " + indir + "/" + f + ".in"
    _,o,e = c.exec_command(cmd)
    b = int(o.read().decode().strip())
    # decimal digits ~ bytes (minus whitespace); limbs ~ digits/4; len2 ~ limbs/2 for divisor
    print(f, "bytes=%d  ~digits=%d  ~total_limbs=%d" % (b, b//2, b//8))
# peek format
_,o,e = c.exec_command("head -c 100 " + indir + "/length_ratio_integer_02.in")
print("FORMAT:", repr(o.read().decode().strip()[:100]))
c.close()
