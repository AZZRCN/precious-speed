#!/bin/bash
# bytecmp 单例闸门: ref_zhbase vs ref_zh
f="$1"
/tmp/hexcmp/bin/ref_zhbase < "$f" > /tmp/ob_$$.txt 2>/dev/null; rc1=$?
/tmp/hexcmp/bin/ref_zh      < "$f" > /tmp/oz_$$.txt 2>/dev/null; rc2=$?
if [ $rc1 -ne 0 ] || [ $rc2 -ne 0 ]; then echo "CRASH $(basename "$f") b=$rc1 z=$rc2"; rm -f /tmp/ob_$$.txt /tmp/oz_$$.txt; exit 1; fi
if ! cmp -s /tmp/ob_$$.txt /tmp/oz_$$.txt; then echo "DIFF $(basename "$f")"; rm -f /tmp/ob_$$.txt /tmp/oz_$$.txt; exit 1; fi
rm -f /tmp/ob_$$.txt /tmp/oz_$$.txt; echo "OK $(basename "$f")"; exit 0
