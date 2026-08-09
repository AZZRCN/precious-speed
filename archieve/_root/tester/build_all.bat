@echo off
cd /d d:\precious_speed

REM Setup MSVC environment (for editbin and tester.exe)
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1

REM Create output directories
if not exist tester\gen mkdir tester\gen
if not exist tester\obj mkdir tester\obj
if not exist tester\failures mkdir tester\failures

set LC_ROOT=E:\library-checker-problems-master
set COMMON=%LC_ROOT%\common
set BI=%LC_ROOT%\big_integer
set STACK_OPT=-Wl,--stack,268435456

echo === [1/4] Compiling 22 official generators (g++ -O2 -std=c++23) ===

REM Delete old generator exes to avoid permission issues
if exist tester\gen\*.exe del tester\gen\*.exe 2>NUL

REM ADD generators
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_carry_chain.exe"  "%BI%\addition_of_big_integers\gen\carry_chain.cpp"  && echo [OK] add_carry_chain  || echo [FAIL] add_carry_chain
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_large.exe"        "%BI%\addition_of_big_integers\gen\large.cpp"        && echo [OK] add_large        || echo [FAIL] add_large
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_large_small.exe"  "%BI%\addition_of_big_integers\gen\large_small.cpp"  && echo [OK] add_large_small  || echo [FAIL] add_large_small
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_max_max.exe"      "%BI%\addition_of_big_integers\gen\max_max.cpp"      && echo [OK] add_max_max      || echo [FAIL] add_max_max
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_medium.exe"       "%BI%\addition_of_big_integers\gen\medium.cpp"       && echo [OK] add_medium       || echo [FAIL] add_medium
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_small.exe"        "%BI%\addition_of_big_integers\gen\small.cpp"        && echo [OK] add_small        || echo [FAIL] add_small
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\addition_of_big_integers" -o "tester\gen\add_sum_zero.exe"     "%BI%\addition_of_big_integers\gen\sum_zero.cpp"     && echo [OK] add_sum_zero     || echo [FAIL] add_sum_zero

REM MUL generators
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_fft_killer.exe"  "%BI%\multiplication_of_big_integers\gen\fft_killer.cpp"  && echo [OK] mul_fft_killer  || echo [FAIL] mul_fft_killer
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_large.exe"       "%BI%\multiplication_of_big_integers\gen\large.cpp"       && echo [OK] mul_large       || echo [FAIL] mul_large
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_large_small.exe" "%BI%\multiplication_of_big_integers\gen\large_small.cpp" && echo [OK] mul_large_small || echo [FAIL] mul_large_small
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_max_max.exe"     "%BI%\multiplication_of_big_integers\gen\max_max.cpp"     && echo [OK] mul_max_max     || echo [FAIL] mul_max_max
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_medium.exe"      "%BI%\multiplication_of_big_integers\gen\medium.cpp"      && echo [OK] mul_medium      || echo [FAIL] mul_medium
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_small.exe"       "%BI%\multiplication_of_big_integers\gen\small.cpp"       && echo [OK] mul_small       || echo [FAIL] mul_small
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\multiplication_of_big_integers" -o "tester\gen\mul_zero.exe"        "%BI%\multiplication_of_big_integers\gen\zero.cpp"        && echo [OK] mul_zero        || echo [FAIL] mul_zero

REM DIV generators
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_a_max_b_random.exe"       "%BI%\division_of_big_integers\gen\a_max_b_random.cpp"       && echo [OK] div_a_max_b_random       || echo [FAIL] div_a_max_b_random
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_burnikel_ziegler.exe"     "%BI%\division_of_big_integers\gen\burnikel_ziegler_bound.cpp" && echo [OK] div_burnikel_ziegler     || echo [FAIL] div_burnikel_ziegler
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_large.exe"                "%BI%\division_of_big_integers\gen\large.cpp"                && echo [OK] div_large                || echo [FAIL] div_large
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_length_ratio_integer.exe" "%BI%\division_of_big_integers\gen\length_ratio_integer.cpp" && echo [OK] div_length_ratio_integer || echo [FAIL] div_length_ratio_integer
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_max.exe"                  "%BI%\division_of_big_integers\gen\max.cpp"                  && echo [OK] div_max                  || echo [FAIL] div_max
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_medium.exe"               "%BI%\division_of_big_integers\gen\medium.cpp"               && echo [OK] div_medium               || echo [FAIL] div_medium
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_r_nearly_zero.exe"        "%BI%\division_of_big_integers\gen\r_nearly_zero.cpp"        && echo [OK] div_r_nearly_zero        || echo [FAIL] div_r_nearly_zero
g++ -O2 -std=c++23 -I "%COMMON%" -I "%BI%\division_of_big_integers" -o "tester\gen\div_small.exe"                "%BI%\division_of_big_integers\gen\small.cpp"                && echo [OK] div_small                || echo [FAIL] div_small

echo.
echo === [2/4] Compiling best/ reference solutions (g++ -O2 -std=c++23 with 256MB stack) ===
del tester\bst_add.exe 2>NUL
del tester\bst_mul.exe 2>NUL
del tester\bst_div.exe 2>NUL
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\bst_add.exe best\add.cpp && echo [OK] bst_add || echo [FAIL] bst_add
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\bst_mul.exe best\mul.cpp && echo [OK] bst_mul || echo [FAIL] bst_mul
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\bst_div.exe best\div.cpp && echo [OK] bst_div || echo [FAIL] bst_div

echo.
echo === [3/4] Compiling current dev solutions (g++ -O2 -std=c++23 with 256MB stack) ===
del tester\cur_add.exe 2>NUL
del tester\cur_mul.exe 2>NUL
del tester\cur_div.exe 2>NUL
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\cur_add.exe add.cpp && echo [OK] cur_add || echo [FAIL] cur_add
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\cur_mul.exe mul.cpp && echo [OK] cur_mul || echo [FAIL] cur_mul
g++ -O2 -std=c++23 %STACK_OPT% -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp && echo [OK] cur_div || echo [FAIL] cur_div

echo.
echo === [4/4] Building tester.exe (MSVC) ===
cl /EHsc /O2 /std:c++17 /utf-8 /F268435456 /Fe:tester\tester.exe /Fo:tester\obj\tester.obj tester\main.cpp > tester\build_tester.log 2>&1 && echo [OK] tester || echo [FAIL] tester

echo.
echo === Verify stack sizes (should be 0x10000000 = 256MB) ===
echo bst_add: 
dumpbin /headers tester\bst_add.exe | findstr /i "size of stack"
echo bst_mul:
dumpbin /headers tester\bst_mul.exe | findstr /i "size of stack"
echo bst_div:
dumpbin /headers tester\bst_div.exe | findstr /i "size of stack"
echo cur_add:
dumpbin /headers tester\cur_add.exe | findstr /i "size of stack"
echo cur_mul:
dumpbin /headers tester\cur_mul.exe | findstr /i "size of stack"
echo cur_div:
dumpbin /headers tester\cur_div.exe | findstr /i "size of stack"

echo.
echo === If stack is still 1MB, applying editbin fix ===
editbin /STACK:268435456 tester\bst_add.exe >NUL 2>&1
editbin /STACK:268435456 tester\bst_mul.exe >NUL 2>&1
editbin /STACK:268435456 tester\bst_div.exe >NUL 2>&1
editbin /STACK:268435456 tester\cur_add.exe >NUL 2>&1
editbin /STACK:268435456 tester\cur_mul.exe >NUL 2>&1
editbin /STACK:268435456 tester\cur_div.exe >NUL 2>&1

echo.
echo === Stack sizes after editbin ===
echo bst_add:
dumpbin /headers tester\bst_add.exe | findstr /i "size of stack"
echo bst_mul:
dumpbin /headers tester\bst_mul.exe | findstr /i "size of stack"
echo bst_div:
dumpbin /headers tester\bst_div.exe | findstr /i "size of stack"
echo cur_add:
dumpbin /headers tester\cur_add.exe | findstr /i "size of stack"
echo cur_mul:
dumpbin /headers tester\cur_mul.exe | findstr /i "size of stack"
echo cur_div:
dumpbin /headers tester\cur_div.exe | findstr /i "size of stack"

echo.
echo === Done ===
