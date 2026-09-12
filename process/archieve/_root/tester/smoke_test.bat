@echo off
cd /d d:\precious_speed

echo === Manual smoke test ===
echo.

REM Test ADD with add_small seed 0
echo [1] ADD: add_small seed=0
tester\gen\add_small.exe 0 > tester\smoke_add.in
tester\bst_add.exe < tester\smoke_add.in > tester\smoke_add.bst.out 2>&1
echo   bst_add exit=%errorlevel%
tester\cur_add.exe < tester\smoke_add.in > tester\smoke_add.cur.out 2>&1
echo   cur_add exit=%errorlevel%
fc /b tester\smoke_add.bst.out tester\smoke_add.cur.out >NUL 2>&1
if %errorlevel%==0 (echo   [PASS] outputs match) else (echo   [FAIL] outputs differ)
echo   input size:
for %%A in (tester\smoke_add.in) do echo     %%~zA bytes

REM Test MUL with mul_small seed 0
echo.
echo [2] MUL: mul_small seed=0
tester\gen\mul_small.exe 0 > tester\smoke_mul.in
tester\bst_mul.exe < tester\smoke_mul.in > tester\smoke_mul.bst.out 2>&1
echo   bst_mul exit=%errorlevel%
tester\cur_mul.exe < tester\smoke_mul.in > tester\smoke_mul.cur.out 2>&1
echo   cur_mul exit=%errorlevel%
fc /b tester\smoke_mul.bst.out tester\smoke_mul.cur.out >NUL 2>&1
if %errorlevel%==0 (echo   [PASS] outputs match) else (echo   [FAIL] outputs differ)
echo   input size:
for %%A in (tester\smoke_mul.in) do echo     %%~zA bytes

REM Test DIV with div_small seed 0
echo.
echo [3] DIV: div_small seed=0
tester\gen\div_small.exe 0 > tester\smoke_div.in
tester\bst_div.exe < tester\smoke_div.in > tester\smoke_div.bst.out 2>&1
echo   bst_div exit=%errorlevel%
tester\cur_div.exe < tester\smoke_div.in > tester\smoke_div.cur.out 2>&1
echo   cur_div exit=%errorlevel%
fc /b tester\smoke_div.bst.out tester\smoke_div.cur.out >NUL 2>&1
if %errorlevel%==0 (echo   [PASS] outputs match) else (echo   [FAIL] outputs differ)
echo   input size:
for %%A in (tester\smoke_div.in) do echo     %%~zA bytes

REM Test MUL with fft_killer seed 0 (the important one)
echo.
echo [4] MUL: mul_fft_killer seed=0
tester\gen\mul_fft_killer.exe 0 > tester\smoke_fft.in
tester\bst_mul.exe < tester\smoke_fft.in > tester\smoke_fft.bst.out 2>&1
echo   bst_mul exit=%errorlevel%
tester\cur_mul.exe < tester\smoke_fft.in > tester\smoke_fft.cur.out 2>&1
echo   cur_mul exit=%errorlevel%
fc /b tester\smoke_fft.bst.out tester\smoke_fft.cur.out >NUL 2>&1
if %errorlevel%==0 (echo   [PASS] outputs match) else (echo   [FAIL] outputs differ)
echo   input size:
for %%A in (tester\smoke_fft.in) do echo     %%~zA bytes

echo.
echo === Done ===
