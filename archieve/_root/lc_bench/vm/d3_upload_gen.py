import sys, os
sys.path.insert(0, r'D:\precious_speed\lc_bench\vm')
from vmctl import run, put

SRC = r'E:\library-checker-problems-master'
DST = '/home/azzr/lcgen'

files = [
    (f'{SRC}/common/random.h', f'{DST}/common/random.h'),
    (f'{SRC}/big_integer/division_of_big_integers/params.h', f'{DST}/division_of_big_integers/params.h'),
    (f'{SRC}/big_integer/division_of_big_integers/base.hpp', f'{DST}/division_of_big_integers/base.hpp'),
]
gen_dir = f'{SRC}/big_integer/division_of_big_integers/gen'
for fn in sorted(os.listdir(gen_dir)):
    if fn.endswith('.cpp'):
        files.append((os.path.join(gen_dir, fn), f'{DST}/division_of_big_integers/gen/{fn}'))

for local, remote in files:
    parent = os.path.dirname(remote)
    run(f'mkdir -p {parent}')
    put(local, remote)
    print('uploaded', remote)
print('DONE_UPLOAD')
