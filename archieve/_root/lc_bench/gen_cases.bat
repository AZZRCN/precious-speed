@echo off
REM gen_cases.bat - Generate all LC official test cases per info.toml
REM Usage: gen_cases.bat
setlocal enabledelayedexpansion

set GENS=%~dp0gens
set CASES=%~dp0cases
set LC_ROOT=E:\library-checker-problems-master\big_integer

echo [gen_cases] Generating test cases...

REM ====== ADD cases ======
REM example_00.in (copy from source)
copy /Y "%LC_ROOT%\addition_of_big_integers\gen\example_00.in" "%CASES%\add\example_00.in" >nul
REM small: 1
"%GENS%\add_small.exe" 0 > "%CASES%\add\small_00.in"
REM medium: 3
for /L %%S in (0,1,2) do "%GENS%\add_medium.exe" %%S > "%CASES%\add\medium_0%%S.in"
REM large: 3
for /L %%S in (0,1,2) do "%GENS%\add_large.exe" %%S > "%CASES%\add\large_0%%S.in"
REM max_max: 8
for /L %%S in (0,1,7) do (
    if %%S LSS 10 ( set TAG=0%%S ) else ( set TAG=%%S )
    "%GENS%\add_max_max.exe" %%S > "%CASES%\add\max_max_!TAG!.in"
)
REM sum_zero: 1
"%GENS%\add_sum_zero.exe" 0 > "%CASES%\add\sum_zero_00.in"
REM large_small: 1
"%GENS%\add_large_small.exe" 0 > "%CASES%\add\large_small_00.in"
REM carry_chain: 4
for /L %%S in (0,1,3) do "%GENS%\add_carry_chain.exe" %%S > "%CASES%\add\carry_chain_0%%S.in"

echo [gen_cases] ADD cases done.

REM ====== MUL cases ======
copy /Y "%LC_ROOT%\multiplication_of_big_integers\gen\example_00.in" "%CASES%\mul\example_00.in" >nul
"%GENS%\mul_small.exe" 0 > "%CASES%\mul\small_00.in"
for /L %%S in (0,1,2) do "%GENS%\mul_medium.exe" %%S > "%CASES%\mul\medium_0%%S.in"
for /L %%S in (0,1,2) do "%GENS%\mul_large.exe" %%S > "%CASES%\mul\large_0%%S.in"
for /L %%S in (0,1,7) do (
    if %%S LSS 10 ( set TAG=0%%S ) else ( set TAG=%%S )
    "%GENS%\mul_max_max.exe" %%S > "%CASES%\mul\max_max_!TAG!.in"
)
"%GENS%\mul_zero.exe" 0 > "%CASES%\mul\zero_00.in"
for /L %%S in (0,1,1) do "%GENS%\mul_fft_killer.exe" %%S > "%CASES%\mul\fft_killer_0%%S.in"
"%GENS%\mul_large_small.exe" 0 > "%CASES%\mul\large_small_00.in"

echo [gen_cases] MUL cases done.

REM ====== DIV cases ======
copy /Y "%LC_ROOT%\division_of_big_integers\gen\example_00.in" "%CASES%\div\example_00.in" >nul
"%GENS%\div_small.exe" 0 > "%CASES%\div\small_00.in"
for /L %%S in (0,1,2) do "%GENS%\div_medium.exe" %%S > "%CASES%\div\medium_0%%S.in"
for /L %%S in (0,1,1) do "%GENS%\div_large.exe" %%S > "%CASES%\div\large_0%%S.in"
for /L %%S in (0,1,2) do "%GENS%\div_max.exe" %%S > "%CASES%\div\max_0%%S.in"
for /L %%S in (0,1,2) do "%GENS%\div_a_max_b_random.exe" %%S > "%CASES%\div\a_max_b_random_0%%S.in"
for /L %%S in (0,1,2) do "%GENS%\div_r_nearly_zero.exe" %%S > "%CASES%\div\r_nearly_zero_0%%S.in"
for /L %%S in (0,1,5) do "%GENS%\div_length_ratio_integer.exe" %%S > "%CASES%\div\length_ratio_integer_0%%S.in"
for /L %%S in (0,1,3) do "%GENS%\div_burnikel_ziegler_bound.exe" %%S > "%CASES%\div\burnikel_ziegler_bound_0%%S.in"

echo [gen_cases] DIV cases done.
echo [gen_cases] All cases generated.
exit /b 0
