import subprocess, os

GEN_DIR = '/home/azzr/lcgen/division_of_big_integers/gen'
COMMON = '/home/azzr/lcgen/common'
SRC = '/home/azzr/divbench/src/div_D3.cpp'
DBGSRC = '/home/azzr/divbench/src/div_D3_dbg.cpp'
DBGBIN = '/home/azzr/divbench/bin/div_D3_dbg'

# debug source = copy of D3 + fprintf (compute est_blocks inline since D3 removed the var)
s = open(SRC).read()
anchor = "            use_cyclic = (cyclic_m < in + len2);\n"
assert s.count(anchor) == 1, ("anchor", s.count(anchor))
ins = anchor + ('            { size_t dbg_eb = (quotient.size + in - 1) / in;'
                ' fprintf(stderr, "EB=%zu CYCLIC=%d\\n", dbg_eb, (int)use_cyclic); }\n')
open(DBGSRC, 'w').write(s.replace(anchor, ins, 1))

# compile generators
gens = ['a_max_b_random', 'r_nearly_zero', 'length_ratio_integer', 'medium',
        'large', 'max', 'small', 'burnikel_ziegler_bound']
gb = {}
for g in gens:
    cpp = f'{GEN_DIR}/{g}.cpp'; out = f'/tmp/gen_{g}'
    r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON, cpp, '-o', out],
                       capture_output=True, text=True)
    if r.returncode == 0:
        gb[g] = out
    else:
        print('GEN_COMPILE_FAIL', g, r.stderr[:300], flush=True)

# compile debug D3
r = subprocess.run(['g++', '-O2', '-std=c++23', '-march=x86-64-v3', '-I', COMMON,
                    DBGSRC, '-o', DBGBIN], capture_output=True, text=True)
if r.returncode != 0:
    print('D3DBG_COMPILE_FAIL', r.stderr[:600], flush=True); raise SystemExit(1)
print('debug D3 compiled', flush=True)

gens_n = {'a_max_b_random': 24, 'r_nearly_zero': 80, 'length_ratio_integer': 48,
          'medium': 40, 'large': 24, 'max': 24, 'small': 40, 'burnikel_ziegler_bound': 24}
max_eb = 0; cyc = 0; tot = 0; max_eb_cyclic = 0
for g, n in gens_n.items():
    if g not in gb:
        continue
    for seed in range(1, n + 1):
        inf = '/tmp/case.in'
        subprocess.run([gb[g], str(seed)], stdout=open(inf, 'wb'))
        p = subprocess.run([DBGBIN], stdin=open(inf, 'rb'), capture_output=True)
        os.remove(inf)
        for line in p.stderr.decode(errors='replace').splitlines():
            if line.startswith('EB='):
                eb = int(line.split()[0].split('=')[1])
                c = line.split('CYCLIC=')[1] == '1'
                max_eb = max(max_eb, eb)
                tot += 1
                if c:
                    cyc += 1
                    max_eb_cyclic = max(max_eb_cyclic, eb)
    print(f'-- {g} done (max_eb={max_eb} cyc={cyc}/{tot})', flush=True)

print(f'=== calls={tot} max_est_blocks={max_eb} cyclic_true={cyc} '
      f'max_est_blocks_under_cyclic={max_eb_cyclic} ===', flush=True)
