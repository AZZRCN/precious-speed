@echo off
cd /d d:\precious_speed

set LC_ROOT=E:\library-checker-problems-master
set COMMON=%LC_ROOT%\common
set BI=%LC_ROOT%\big_integer

echo === Compiling official correct.cpp reference solutions ===

del tester\ref_add.exe 2>NUL
del tester\ref_mul.exe 2>NUL
del tester\ref_div.exe 2>NUL

g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o tester\ref_add.exe "%BI%\addition_of_big_integers\sol\correct.cpp" && echo [OK] ref_add || echo [FAIL] ref_add
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o tester\ref_mul.exe "%BI%\multiplication_of_big_integers\sol\correct.cpp" && echo [OK] ref_mul || echo [FAIL] ref_mul
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o tester\ref_div.exe "%BI%\division_of_big_integers\sol\correct.cpp" && echo [OK] ref_div || echo [FAIL] ref_div

echo.
echo === Quick test: ref_mul on fft_killer seed=0 ===
tester\gen\mul_fft_killer.exe 0 > tester\smoke_fft.in
tester\ref_mul.exe < tester\smoke_fft.in > tester\smoke_fft.ref.out 2>&1
echo ref_mul exit=%errorlevel%
for %%A in (tester\smoke_fft.ref.out) do echo output size: %%~zA bytes

echo.
echo === Done ===
