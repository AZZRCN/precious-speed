@echo off
REM ai_agent_pure_bench.bat - Measure pure division time (no I/O)
setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set DIV_SRC=%ROOT%\div.cpp
set DIV_EXE=%ROOT%\cur_div_pure.exe
set CXXFLAGS=-std=c++20 -O3 -march=native -funroll-loops -I. -DBENCH_DIV_PURE
set LDFLAGS=-lpthread

echo [pure] Compiling with BENCH_DIV_PURE (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%DIV_SRC%" -o "%DIV_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC! neq 0 ( echo [pure] FAIL rc=!RC! & exit /b !RC! )
echo [pure] OK compiled

echo [pure] Running (3 cases b_len=1000 q_len=1000, timeout=60s)
%TTO% 60 python "%ROOT%\gen_prof_in.py"
%TTO% 60 "%DIV_EXE%" < "%ROOT%\prof_in.txt" > "%ROOT%\prof_out.txt" 2>&1
set RC=!ERRORLEVEL!
echo [pure] rc=!RC!
type "%ROOT%\prof_out.txt"
exit /b 0
