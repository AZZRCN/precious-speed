#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 VM 上跑 _prof_cluster.py, 把表格通过 cat 回传, 落本地 _prof_out.txt。
用法: python _runprof.py <case1> [case2 ...]
stdout 在本环境被吞, 所有结果靠文件: _runlog.txt (运行日志) + _prof_out.txt (画像表)。
"""
import vmctl, sys

LOCAL = r'D:\precious_speed\lc_bench\vm'
cases = sys.argv[1:] or ['length_ratio_integer_02']
cmd = ('cd /home/azzr/divbench && python3 _prof_cluster.py d31 %s '
       '> _prof_out.txt 2>&1; echo DONE' % ' '.join(cases))

log = open(LOCAL + r'\_runlog.txt', 'w')
try:
    rc, o, e = vmctl.run(cmd, timeout=3000)
    log.write('RUN rc=%s\nERR=%r\n' % (rc, e))
except Exception as ex:
    log.write('RUN exception %r\n' % (ex,))
    o = ''

if 'DONE' in o:
    try:
        rc2, o2, e2 = vmctl.run('cat /home/azzr/divbench/_prof_out.txt', timeout=120)
        open(LOCAL + r'\_prof_out.txt', 'w').write(o2)
        log.write('CAT len=%d\n' % len(o2))
    except Exception as ex:
        log.write('CAT exception %r\n' % (ex,))
else:
    log.write('PROFILE NOT DONE; OUT=%r\n' % (o,))
log.close()
