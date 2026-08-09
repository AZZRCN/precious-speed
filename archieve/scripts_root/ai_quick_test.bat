@echo off
setlocal
set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed

echo [quick] Compiling div.cpp (timeout=30s)
%TTO% 30 g++ -std=c++20 -O3 -march=native -funroll-loops -I. "%ROOT%\div.cpp" -o "%ROOT%\cur_div.exe" -lpthread
set RC=%ERRORLEVEL%
if %RC% neq 0 ( echo [quick] FAIL rc=%RC% & exit /b %RC% )
echo [quick] OK compiled

echo [quick] Quick correctness test (3 cases)
%TTO% 30 python "%ROOT%\gen_prof_in.py"
%TTO% 30 "%ROOT%\cur_div.exe" < "%ROOT%\prof_in.txt" > "%ROOT%\quick_out.txt" 2>nul
set RC=%ERRORLEVEL%
if %RC% neq 0 ( echo [quick] FAIL run rc=%RC% & exit /b %RC% )

echo [quick] Verifying...
%TTO% 30 python "%ROOT%\verify_quick.py"
echo [quick] Done
exit /b 0
