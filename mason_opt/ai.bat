@echo off
cd /d d:\precious_speed\mason_opt
setlocal enabledelayedexpansion

echo ============================================================
echo [1] Verify correctness (absInvNewton thread_local)
echo ============================================================
g++ -O3 -mavx2 -mfma -funroll-loops -o verify_div.exe verify_div.cpp 2>&1
if errorlevel 1 (echo VERIFY compile FAILED ^& exit /b 1)
verify_div.exe 100000 8
if errorlevel 1 (echo VERIFY FAILED ^& exit /b 1)
echo.

echo ============================================================
echo [2] Key benchmarks (compare with baseline)
echo ============================================================
echo --- 1M div2 (baseline: -11.8%%) ---
powershell -ExecutionPolicy Bypass -File bench_div_duel.ps1 1000000 div2 3 -SkipCompile
echo.
echo --- 100K div2 (baseline: -23.7%%) ---
powershell -ExecutionPolicy Bypass -File bench_div_duel.ps1 100000 div2 3 -SkipCompile
echo.
echo --- 250K div4 (baseline: -24.8%%) ---
powershell -ExecutionPolicy Bypass -File bench_div_duel.ps1 250000 div4 3 -SkipCompile
echo.
echo --- 100K div8 (baseline: -34.2%%) ---
powershell -ExecutionPolicy Bypass -File bench_div_duel.ps1 100000 div8 3 -SkipCompile

echo === Done ===
