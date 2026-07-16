@echo off
REM bench_div_check.bat - div_work vs masonxiong_opt 对拍测试
cd /d d:\precious_speed\mason_opt

echo Compiling test_work (div_work)...
g++ -O3 -mavx2 -mfma -funroll-loops -o test_work.exe test_work.cpp
if errorlevel 1 (echo test_work compile FAIL & exit /b 1)

echo Compiling test_mason (masonxiong_opt)...
g++ -O3 -mavx2 -mfma -funroll-loops -o test_mason.exe test_mason.cpp
if errorlevel 1 (echo test_mason compile FAIL & exit /b 1)

echo Compiling gen_div_input...
g++ -O2 -o gen_div_input.exe gen_div_input.cpp
if errorlevel 1 (echo gen_div_input compile FAIL & exit /b 1)

set ALLPASS=1

REM 测试用例格式: name cases na nb seed
for %%T in (
  "small_eq 50 10 10 42"
  "small_neq 50 20 10 43"
  "mid_eq 20 1000 1000 44"
  "mid_neq 20 2000 1000 45"
  "large_eq 5 10000 10000 46"
  "large_neq 5 20000 10000 47"
  "huge_eq 2 100000 100000 48"
  "huge_neq 2 200000 100000 49"
  "div2 3 200000 100000 50"
  "div4 2 400000 100000 51"
  "div8 1 800000 100000 52"
  "tiny_b 30 100 2 53"
  "edge_a_eq_b 10 50 50 54"
  "edge_a_lt_b 10 10 50 55"
) do (
  for /f "tokens=1-4" %%a in (%%T) do (
    echo.
    echo === %%a (cases=%%b na=%%c nb=%%d) ===
    gen_div_input.exe %%b %%c %%d > input_%%a.txt
    test_work.exe  < input_%%a.txt > out_work_%%a.txt  2>&1
    test_mason.exe < input_%%a.txt > out_mason_%%a.txt 2>&1
    fc /n out_work_%%a.txt out_mason_%%a.txt >nul 2>&1
    if errorlevel 1 (
      echo   FAIL: output mismatch
      set ALLPASS=0
    ) else (
      echo   PASS
    )
  )
)

echo.
if "%ALLPASS%"=="1" (echo ========== ALL PASS ==========) else (echo ========== SOME FAILED ==========)
