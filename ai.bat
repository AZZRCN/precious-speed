@echo off
cd /d d:\precious_speed
echo === Step 1: Compile tools (gen_data + bench) ===
g++ -O2 -o gen_data.exe gen_data.cpp
if errorlevel 1 (echo GEN_DATA FAILED & exit /b 1) else (echo gen_data OK)
g++ -O2 -o bench.exe bench.cpp
if errorlevel 1 (echo BENCH FAILED & exit /b 1) else (echo bench OK)
echo.
echo === Step 2: Run bench (auto-compile moptm/best/gmp + gen data + 3-mode bench) ===
bench.exe %*
echo.
echo === Done ===
