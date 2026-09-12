@echo off
cd /d d:\precious_speed
echo === Compiling ===
g++ -O2 -std=c++23 -Wl,--stack,268435456 -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp
if errorlevel 1 (echo [FAIL] Compile & exit /b 1)
echo [OK]
echo === Running seed=98 (stderr to dbg) ===
tester\cur_div.exe < tester\failures\div_div_medium_98_0.in > tester\dbg_98.out 2> tester\dbg_98.err
echo exit=%errorlevel%
echo === First 30 stderr lines ===
powershell -Command "Get-Content tester\dbg_98.err | Select-Object -First 30"
echo === Last 10 stderr lines ===
powershell -Command "Get-Content tester\dbg_98.err | Select-Object -Last 10"
echo === stderr line count ===
powershell -Command "(Get-Content tester\dbg_98.err | Measure-Object).Count"
echo === Compare output ===
fc /b tester\dbg_98.out tester\failures\div_div_medium_98_0.ref.out >NUL 2>&1
if errorlevel 1 (echo [FAIL]) else (echo [PASS])
