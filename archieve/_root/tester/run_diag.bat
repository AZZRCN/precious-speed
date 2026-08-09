@echo off
chcp 65001 > nul
echo Compiling div.cpp with DIAG_CYCLIC_DETAIL...
g++ -std=c++17 -O3 -march=native -DDIAG_CYCLIC_DETAIL -o d:\precious_speed\tester\cur_div.exe d:\precious_speed\div.cpp -Wall -Wextra 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed!
    exit /b 1
)
echo Compilation OK.
echo.

echo Running seed=98...
echo d:\precious_speed\tester\failures\div_div_medium_98_0.in
type d:\precious_speed\tester\failures\div_div_medium_98_0.in | d:\precious_speed\tester\cur_div.exe > d:\precious_speed\tester\failures\div_div_medium_98_0.new.out 2>d:\precious_speed\tester\diag_stderr_98.txt
echo.
echo === STDERR (diagnostic output) ===
type d:\precious_speed\tester\diag_stderr_98.txt
echo.
echo === Checking result ===
fc /b d:\precious_speed\tester\failures\div_div_medium_98_0.new.out d:\precious_speed\tester\failures\div_div_medium_98_0.ref.out >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [PASS] Output matches reference
) else (
    echo [FAIL] Output differs from reference
)