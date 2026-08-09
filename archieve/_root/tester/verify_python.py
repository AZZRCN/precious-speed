import sys
sys.set_int_max_str_digits(10000000)

# Read files
with open(r'd:\precious_speed\tester\failures\div_div_medium_98_0.ref.out', 'r') as f:
    ref_lines = f.read().split('\n')
with open(r'd:\precious_speed\tester\single_98.cur.out', 'r') as f:
    new_content = f.read().strip()
with open(r'd:\precious_speed\tester\single_98.in', 'r') as f:
    in_lines = f.read().split('\n')

ref_q, ref_r = ref_lines[0].split(' ')
new_q, new_r = new_content.split(' ')
a_str, b_str = in_lines[1].split(' ')

A = int(a_str)
B = int(b_str)
refQ = int(ref_q)
refR = int(ref_r)
newQ = int(new_q)
newR = int(new_r)

print(f"A digits: {len(a_str)}  B digits: {len(b_str)}")
print(f"ref Q digits: {len(ref_q)}  ref R digits: {len(ref_r)}")
print(f"new Q digits: {len(new_q)}  new R digits: {len(new_r)}")
print(f"Q match: {refQ == newQ}")
print(f"R match: {refR == newR}")
print(f"ref R < B: {refR < B}")
print(f"new R < B: {newR < B}")
print(f"ref check A == Q*B + R: {A == refQ * B + refR}")
print(f"new check A == Q*B + R: {A == newQ * B + newR}")
print(f"newR - refR = {newR - refR}")
print(f"B = {B}")
print(f"diff = newR - refR, is it 0? {newR - refR == 0}")
print(f"is newR == refR? {newR == refR}")
