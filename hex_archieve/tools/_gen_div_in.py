import random
# 生成触发 FFT 的大除法输入：nb >= 160 limbs (>= 2560 hex digits)
random.seed(12345)
def rh(n_digits):
    # 首位非零
    s = ''.join(random.choice('0123456789abcdef') for _ in range(n_digits))
    s = 'f' + s[1:]
    return s
lines = ['3']
# case1: a 长 b 长 -> 都走 FFT
lines.append(rh(5000) + ' ' + rh(3000))
# case2: 近似等长
lines.append(rh(3000) + ' ' + rh(2600))
# case3: 大 a / 中 b
lines.append(rh(8000) + ' ' + rh(2700))
open('tools/_d9input.txt','w').write('\n'.join(lines) + '\n')
print("generated", sum(len(l) for l in lines), "chars")
