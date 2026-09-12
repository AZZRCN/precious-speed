#!/bin/bash
# run_cg.sh <infile> <out_ir_log>
# Runs opt_src via callgrind on a single LC case file, captures total I refs.
BIN=/tmp/opt_src
IN="$1"
LOG="$2"
ir=$(valgrind --tool=callgrind --cache-sim=no --collect-jumps=no --callgrind-out-file=/tmp/cg_single.out "$BIN" < "$IN" > /tmp/cg_out.txt 2>/tmp/cg_err.txt | grep -oE "I +refs: +[0-9,]+"; valgrind --tool=callgrind --cache-sim=no --collect-jumps=no "$BIN" < "$IN" >/dev/null 2>&1; grep -oE "I +refs: +[0-9,]+" /tmp/cg_single.out | grep -oE "[0-9,]+")
echo "$IN Ir=$ir" >> "$LOG"
echo "$IN -> $ir"
