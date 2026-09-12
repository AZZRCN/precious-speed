import os
IN='/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases=['length_ratio_integer_00','length_ratio_integer_01','length_ratio_integer_02',
       'length_ratio_integer_03','length_ratio_integer_04','length_ratio_integer_05',
       'max_00','a_max_b_random_01','burnikel_ziegler_bound_02','r_nearly_zero_01']
for c in cases:
    p=IN+c+'.in'
    if not os.path.exists(p): continue
    with open(p) as f:
        T=int(f.readline())
        rows=[]
        for i in range(min(T,3)):
            a,b=f.readline().split()
            rows.append((len(a),len(b)))
    for i,(da,db) in enumerate(rows):
        la=(da+3)//4; lb=(db+3)//4
        ql=la-lb; sl=lb-ql
        if lb==0: continue
        if lb<=64 or (la-lb)<=64: path='schoolbook'
        elif la < lb*2: path='NewtonCore1'
        else: path='absDivMu'
        print('%-26s T=%-4d q%d digA=%-8d digB=%-8d limbA=%-7d limbB=%-7d ratio=%.3f  quot_len=%-7d shift_len=%-7d %s'
              %(c,T,i,da,db,la,lb,da/db,ql,sl,path))
