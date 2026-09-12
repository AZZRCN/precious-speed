#!/usr/bin/env python3
import vmctl
rc, o, e = vmctl.run('ls /home/azzr/lcp/big_integer/division_of_big_integers/in/')
open(r'D:\precious_speed\lc_bench\vm\_cases.txt', 'w').write(o)
