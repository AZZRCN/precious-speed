import io, sys

DIV = r'D:/hex_precious_speed/submit_ready/div.cpp'
V7  = r'D:/hex_precious_speed/work/mul/v7_leaf10.cpp'

receipt_div = '''/*
Submission #392043
ID	Date	Problem	Lang	User	Status	Time	Memory
392043	2026/8/12 01:01:41	
Division of Hex Big Integers
	C++23	(Anonymous)	AC	52 ms	29.01 Mib
	Name	Status	Time	Memory
	example_00	AC	1 ms	0.79 Mib
	small_00	AC	13 ms	10.54 Mib
	medium_00	AC	19 ms	8.55 Mib
	medium_01	AC	4 ms	6.55 Mib
	medium_02	AC	4 ms	6.28 Mib
	large_00	AC	4 ms	5.79 Mib
	large_01	AC	4 ms	5.01 Mib
	max_00	AC	5 ms	6.64 Mib
	max_01	AC	3 ms	6.04 Mib
	max_02	AC	4 ms	7.82 Mib
	a_max_b_random_00	AC	23 ms	11.76 Mib
	a_max_b_random_01	AC	41 ms	25.53 Mib
	a_max_b_random_02	AC	40 ms	29.01 Mib
	power_00	AC	4 ms	4.79 Mib
	r_nearly_zero_00	AC	23 ms	7.60 Mib
	r_nearly_zero_01	AC	3 ms	4.32 Mib
	r_nearly_zero_02	AC	3 ms	4.01 Mib
	length_ratio_integer_00	AC	52 ms	26.30 Mib
	length_ratio_integer_01	AC	42 ms	20.27 Mib
	length_ratio_integer_02	AC	48 ms	18.52 Mib
	length_ratio_integer_03	AC	44 ms	15.77 Mib
	length_ratio_integer_04	AC	40 ms	14.04 Mib
	length_ratio_integer_05	AC	27 ms	12.75 Mib
	burnikel_ziegler_bound_00	AC	12 ms	7.79 Mib
	burnikel_ziegler_bound_01	AC	19 ms	8.04 Mib
	burnikel_ziegler_bound_02	AC	9 ms	6.02 Mib
	burnikel_ziegler_bound_03	AC	17 ms	7.97 Mib
*/'''

receipt_v7 = '''/*
Submission #392049
ID	Date	Problem	Lang	User	Status	Time	Memory
392049	2026/8/12 01:09:39	
Multiplication of Hex Big Integers
	C++23	(Anonymous)	AC	27 ms	29.08 Mib
	Name	Status	Time	Memory
	example_00	AC	9 ms	0.52 Mib
	small_00	AC	10 ms	8.77 Mib
	medium_00	AC	10 ms	9.67 Mib
	medium_01	AC	9 ms	10.04 Mib
	medium_02	AC	8 ms	9.02 Mib
	large_00	AC	25 ms	9.51 Mib
	large_01	AC	15 ms	10.30 Mib
	large_02	AC	15 ms	9.30 Mib
	max_max_00	AC	27 ms	29.08 Mib
	max_max_01	AC	25 ms	29.04 Mib
	max_max_02	AC	25 ms	29.01 Mib
	max_max_03	AC	26 ms	29.04 Mib
	max_max_04	AC	26 ms	29.01 Mib
	max_max_05	AC	25 ms	28.80 Mib
	max_max_06	AC	25 ms	29.04 Mib
	max_max_07	AC	26 ms	29.01 Mib
	zero_00	AC	3 ms	3.80 Mib
	power_00	AC	8 ms	10.04 Mib
	power_01	AC	9 ms	10.01 Mib
	fft_killer_00	AC	25 ms	28.95 Mib
	fft_killer_01	AC	25 ms	28.77 Mib
	large_small_00	AC	21 ms	20.01 Mib
*/'''

def swap(path, receipt):
    s = open(path, encoding='utf-8').read()
    mi = s.find('// ============================ 状态')
    assert mi != -1, f'no status block in {path}'
    p = s.find('// =', mi + 5)          # the terminator ==== line
    assert p != -1, f'no terminator in {path}'
    end = s.find('\n', p) + 1
    pre = s[:mi].rstrip('\n')           # '// AZZRCN\n// https://...'
    new = receipt.rstrip('\n') + '\n' + pre + '\n' + s[end:]
    open(path, 'w', encoding='utf-8', newline='').write(new)
    print('SWAPPED', path, '-> bytes', len(new))

swap(DIV, receipt_div)
swap(V7, receipt_v7)
print('DONE')
