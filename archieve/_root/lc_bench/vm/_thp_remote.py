# -*- coding: utf-8 -*-
"""远程探测 THP / 大页 支持情况，并测量某二进制实际获得的 AnonHugePages。"""
import os
import subprocess
import sys

PATHS = [
    '/sys/kernel/mm/transparent_hugepage/enabled',
    '/sys/kernel/mm/transparent_hugepage/defrag',
    '/sys/kernel/mm/redhat_transparent_hugepage/enabled',
]


def sh(c):
    p = subprocess.run(['bash', '-c', c], capture_output=True, text=True)
    return p.stdout.strip(), p.stderr.strip()


print('==== THP sysfs ====')
for p in PATHS:
    if os.path.exists(p):
        print('%-58s = %s' % (p, open(p).read().strip()))
    else:
        print('%-58s = <MISSING>' % p)

print()
print('==== /proc/meminfo huge ====')
o, _ = sh("grep -i -E 'huge|Anon' /proc/meminfo")
print(o)

print()
print('==== kernel config ====')
o, _ = sh("uname -r; (zgrep -i TRANSPARENT_HUGEPAGE /proc/config.gz 2>/dev/null "
          "|| grep -i TRANSPARENT_HUGEPAGE /boot/config-$(uname -r) 2>/dev/null "
          "|| echo '<no kernel config available>')")
print(o)

print()
print('==== madvise(MADV_HUGEPAGE) live test ====')
# 直接跑一个 C 小程序: mmap 64MB + MADV_HUGEPAGE, 触碰后读 smaps_rollup
src = r'''
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char**argv){
    size_t N = (size_t)64<<20;
    int use = atoi(argv[1]);
    void* m = mmap(0,N+ (2<<20),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(m==MAP_FAILED){printf("mmap fail\n");return 1;}
    char* b = (char*)(((unsigned long)m + ((2<<20)-1)) & ~(unsigned long)((2<<20)-1));
    if(use) {
        int r = madvise(b,N,MADV_HUGEPAGE);
        printf("madvise rc=%d\n", r);
    }
    memset(b,1,N);
    FILE*f=fopen("/proc/self/smaps_rollup","r");
    char line[512];
    while(f && fgets(line,sizeof line,f)){
        if(strstr(line,"AnonHugePages")||strstr(line,"Rss:")) printf("  %s", line);
    }
    if(f) fclose(f);
    return 0;
}
'''
open('/tmp/_thptest.c', 'w').write(src)
o, e = sh('gcc -O1 -o /tmp/_thptest /tmp/_thptest.c 2>&1')
if e or 'error' in o:
    print('build:', o, e)
print('-- WITHOUT madvise --')
o, e = sh('/tmp/_thptest 0')
print(o or e)
print('-- WITH MADV_HUGEPAGE --')
o, e = sh('/tmp/_thptest 1')
print(o or e)
