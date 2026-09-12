@echo off
REM ai_agent_o2cmp.bat - O2 speed comparison: best/div.cpp VS cur div.cpp
REM Compiles both with -O2 (no -march=native, no -funroll-loops) and benchmarks.
REM Usage: ai_agent_o2cmp.bat [size]
REM   size: medium (default) | large | xlarge | small | all

setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set BEST_SRC=%ROOT%\best\div.cpp
set BEST_EXE=%ROOT%\best_div_o2.exe
set CUR_SRC=%ROOT%\div.cpp
set CUR_EXE=%ROOT%\cur_div_o2.exe
set BENCH_PY=%ROOT%\div_dev\lc_test\bench_div.py

REM O2 only — no -march=native (LC ubuntu env compatibility, user constraint)
REM Keep -std=c++20 -I. for hint headers; -lpthread for thread_local
set O2FLAGS=-std=c++20 -O2 -I.
set LDFLAGS=-lpthread

set SIZE=%1
if "%SIZE%"=="" set SIZE=medium

echo ============================================================
echo [o2cmp] O2 speed comparison: best VS cur  (size=%SIZE%)
echo ============================================================

echo.
echo [o2cmp] Step 1: Compile best/div.cpp (O2, timeout=60s)
%TTO% 60 g++ %O2FLAGS% "%BEST_SRC%" -o "%BEST_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [o2cmp] TIMEOUT: best compile killed & exit /b 124 )
if !RC! neq 0 ( echo [o2cmp] FAIL: best compile rc=!RC! & exit /b !RC! )
echo [o2cmp] OK: best_div_o2.exe compiled

echo.
echo [o2cmp] Step 2: Compile cur div.cpp (O2, timeout=60s)
%TTO% 60 g++ %O2FLAGS% "%CUR_SRC%" -o "%CUR_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [o2cmp] TIMEOUT: cur compile killed & exit /b 124 )
if !RC! neq 0 ( echo [o2cmp] FAIL: cur compile rc=!RC! & exit /b !RC! )
echo [o2cmp] OK: cur_div_o2.exe compiled

echo.
echo ============================================================
echo [o2cmp] Step 3: Benchmark BEST div (O2)
echo ============================================================
%TTO% 180 python "%BENCH_PY%" %SIZE% "%BEST_EXE%"
set RC_BEST=!ERRORLEVEL!

echo.
echo ============================================================
echo [o2cmp] Step 4: Benchmark CUR div (O2)
echo ============================================================
%TTO% 180 python "%BENCH_PY%" %SIZE% "%CUR_EXE%"
set RC_CUR=!ERRORLEVEL!

echo.
echo ============================================================
echo [o2cmp] Summary
echo ============================================================
echo   best rc=!RC_BEST!  cur rc=!RC_CUR!
echo   best binary: %BEST_EXE%
echo   cur  binary: %CUR_EXE%
exit /b 0
