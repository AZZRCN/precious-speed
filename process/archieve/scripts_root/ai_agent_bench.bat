@echo off
REM ai_agent_bench.bat - Benchmark div.cpp
REM Generates LC-style test cases and measures execution time
REM Usage: ai_agent_bench.bat [size]
REM size: small (default), medium, large, max

setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set DIV_SRC=%ROOT%\div.cpp
set DIV_EXE=%ROOT%\cur_div.exe
set BENCH_PY=%ROOT%\div_dev\lc_test\bench_div.py
set CXXFLAGS=-std=c++20 -O3 -march=native -funroll-loops -I.
set LDFLAGS=-lpthread

set SIZE=%1
if "%SIZE%"=="" set SIZE=medium

echo [bench] Compiling div.cpp (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%DIV_SRC%" -o "%DIV_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC! neq 0 ( echo [bench] FAIL: compilation rc=!RC! & exit /b !RC! )
echo [bench] OK: div.exe compiled

echo [bench] Running benchmark size=%SIZE% (timeout=120s)
%TTO% 120 python "%BENCH_PY%" %SIZE%
set RC=!ERRORLEVEL!
echo [bench] rc=!RC!
exit /b !RC!
