import random, sys
sys.set_int_max_str_digits(800000)
random.seed(7)
# test product sizes relevant to failures: (na_an, n) and symmetric (n,n)
sizes = [(3661,2525),(2525,2525),(3660,2525),(3515,3514),(6186,1),(1136,2525),
         (3000,2525),(4000,2525),(2525,4000)]
tests = []
for na,nb in sizes:
    a = random.getrandbits(na*64)
    b = random.getrandbits(nb*64)
    tests.append((na,nb,format(a,'x'),format(b,'x')))
with open(r'D:\precious_speed\tools\mul_sweep_in.txt','w') as f:
    f.write(str(len(tests))+'\n')
    for na,nb,ah,bh in tests:
        f.write(ah+' '+bh+'\n')
print("wrote", len(tests))
