@echo off
REM bench_mul_phases.bat - Measure MUL phase timings (parse/mul/write)
setlocal enabledelayedexpansion
set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set EXE=%~dp0exe

echo [mul_phases] Compiling mul_bench.exe (BENCH_INTERNAL)...
%TTO% 30 g++ -std=c++20 -O2 -march=native -I. -DBENCH_INTERNAL "%ROOT%\mul.cpp" -o "%EXE%\mul_bench.exe" -lpthread
if !ERRORLEVEL! neq 0 ( echo FAIL & exit /b 1 )

echo [mul_phases] Running on representative cases...
echo.
echo --- max_max_00 (2x 1M digits) ---
"%EXE%\mul_bench.exe" < "%~dp0cases\mul\max_max_00.in" >nul
echo.
echo --- large_00 ---
"%EXE%\mul_bench.exe" < "%~dp0cases\mul\large_00.in" >nul
echo.
echo --- fft_killer_00 ---
"%EXE%\mul_bench.exe" < "%~dp0cases\mul\fft_killer_00.in" >nul
echo.
echo --- medium_00 ---
"%EXE%\mul_bench.exe" < "%~dp0cases\mul\medium_00.in" >nul
echo.
echo --- small_00 ---
"%EXE%\mul_bench.exe" < "%~dp0cases\mul\small_00.in" >nul
exit /b 0
