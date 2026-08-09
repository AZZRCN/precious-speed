@echo off
REM compile_gens.bat - Compile all LC official test case generators
REM Usage: compile_gens.bat
setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set LC_ROOT=E:\library-checker-problems-master\big_integer
set COMMON=E:\library-checker-problems-master\common
set OUT=%~dp0gens

set CXXFLAGS=-O2 -std=c++17 -I "%COMMON%" -I "%LC_ROOT%\addition_of_big_integers" -I "%LC_ROOT%\multiplication_of_big_integers" -I "%LC_ROOT%\division_of_big_integers"

echo [compile_gens] Compiling generators...

REM ====== ADD generators ======
set ADD_DIR=%LC_ROOT%\addition_of_big_integers\gen
for %%G in (small medium large max_max sum_zero large_small carry_chain) do (
    echo [compile_gens] add_%%G.exe
    %TTO% 30 g++ %CXXFLAGS% "%ADD_DIR%\%%G.cpp" -o "%OUT%\add_%%G.exe"
    if !ERRORLEVEL! neq 0 ( echo [compile_gens] FAIL: add_%%G & exit /b 1 )
)

REM ====== MUL generators ======
set MUL_DIR=%LC_ROOT%\multiplication_of_big_integers\gen
for %%G in (small medium large max_max zero fft_killer large_small) do (
    echo [compile_gens] mul_%%G.exe
    %TTO% 30 g++ %CXXFLAGS% "%MUL_DIR%\%%G.cpp" -o "%OUT%\mul_%%G.exe"
    if !ERRORLEVEL! neq 0 ( echo [compile_gens] FAIL: mul_%%G & exit /b 1 )
)

REM ====== DIV generators ======
set DIV_DIR=%LC_ROOT%\division_of_big_integers\gen
for %%G in (small medium large max a_max_b_random r_nearly_zero length_ratio_integer burnikel_ziegler_bound) do (
    echo [compile_gens] div_%%G.exe
    %TTO% 30 g++ %CXXFLAGS% "%DIV_DIR%\%%G.cpp" -o "%OUT%\div_%%G.exe"
    if !ERRORLEVEL! neq 0 ( echo [compile_gens] FAIL: div_%%G & exit /b 1 )
)

echo [compile_gens] All generators compiled.
exit /b 0
