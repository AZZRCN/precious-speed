#!/usr/bin/env python3
import subprocess, os, sys, glob, argparse
from concurrent.futures import ThreadPoolExecutor

HEX='/tmp/hexcmp'
BIN=os.path.join(HEX,'bin')
BASE=os.path.join(BIN,'ref_zhbase')
ZH=os.path.join(BIN,'ref_zh')
CASE_DIR=os.path.join(HEX,'cases_dir')
CASE_T2=os.path.join(HEX,'cases_t2')

def collect_cases(maxbytes, t2_only, sample_step):
    t2=sorted(glob.glob(os.path.join(CASE_T2,'*.in')))
    d=sorted(glob.glob(os.path.join(CASE_DIR,'*.in')))
    samp=[d[i] for i in range(0,len(d),sample_step)]
    cases = t2 if t2_only else (t2+samp)
    if maxbytes and maxbytes>0:
        cases=[c for c in cases if os.path.getsize(c)<=maxbytes]
    return cases

def run_correct(path):
    try:
        b=subprocess.run([BASE],stdin=open(path,'rb'),capture_output=True,timeout=1200)
        z=subprocess.run([ZH],stdin=open(path,'rb'),capture_output=True,timeout=1200)
    except subprocess.TimeoutExpired:
        return {'case':os.path.basename(path),'err':'TIMEOUT'}
    same=(b.returncode==0 and z.returncode==0 and b.stdout==z.stdout)
    return {'case':os.path.basename(path),'rc_b':b.returncode,'rc_z':z.returncode,'same':same,
            'len_b':len(b.stdout),'len_z':len(z.stdout)}

def cg_ir(path,which):
    binpath=BASE if which=='base' else ZH
    outf='/tmp/cg_%s_%s.txt'%(which,os.path.basename(path))
    try:
        subprocess.run(['valgrind','--tool=callgrind','--callgrind-out-file='+outf,'--quiet',binpath],
                       stdin=open(path,'rb'),capture_output=True,timeout=2400)
    except subprocess.TimeoutExpired:
        return None
    try:
        with open(outf) as f:
            for line in f:
                if line.startswith('totals:'):
                    return int(line.split()[1])
    except Exception:
        pass
    return None

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--mode',choices=['correct','cg','both'],default='both')
    ap.add_argument('--workers',type=int,default=8)
    ap.add_argument('--correct-maxbytes',type=int,default=10**9)
    ap.add_argument('--t2-only',action='store_true')
    ap.add_argument('--sample-step',type=int,default=5)
    ap.add_argument('--cg-maxbytes',type=int,default=200000)
    args=ap.parse_args()
    cases=collect_cases(args.correct_maxbytes,args.t2_only,args.sample_step)
    print('CASES total=%d (t2_only=%s maxbytes<=%d)'%(len(cases),args.t2_only,args.correct_maxbytes),flush=True)
    if args.mode in ('correct','both'):
        fails=0
        with ThreadPoolExecutor(max_workers=args.workers) as ex:
            futs={ex.submit(run_correct,c):c for c in cases}
            for fu in futs:
                r=fu.result()
                if 'err' in r:
                    print('ERR %s %s'%(r['case'],r['err']),flush=True); fails+=1
                elif not r['same']:
                    print('DIFF %s rc_b=%s rc_z=%s len_b=%d len_z=%d'%(r['case'],r['rc_b'],r['rc_z'],r['len_b'],r['len_z']),flush=True); fails+=1
        print('CORRECT tot=%d fails=%d'%(len(cases),fails),flush=True)
    if args.mode in ('cg','both'):
        cg_cases=[c for c in cases if os.path.getsize(c)<=args.cg_maxbytes]
        print('CG cases=%d (<= %d bytes)'%(len(cg_cases),args.cg_maxbytes),flush=True)
        with ThreadPoolExecutor(max_workers=min(args.workers,4)) as ex:
            tasks={}
            for c in cg_cases:
                tasks[ex.submit(cg_ir,c,'base')]=('base',c)
                tasks[ex.submit(cg_ir,c,'zh')]=('zh',c)
            res={}
            for fu in tasks:
                kind,c=tasks[fu]
                res[(kind,c)]=fu.result()
        tot_b=tot_z=0
        for c in cg_cases:
            ib=res[('base',c)]; iz=res[('zh',c)]
            if ib and iz:
                d=ib-iz
                tot_b+=ib; tot_z+=iz
                print('CG %-30s base=%d zh=%d delta=%d (%.3f%%)'%(os.path.basename(c),ib,iz,d,100.0*d/ib),flush=True)
        if tot_b:
            print('CG TOTAL base=%d zh=%d delta=%d (%.3f%%)'%(tot_b,tot_z,tot_b-tot_z,100.0*(tot_b-tot_z)/tot_b),flush=True)

if __name__=='__main__':
    main()
