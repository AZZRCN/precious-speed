@echo off
cd /d d:\precious_speed
del tester\cur_div.exe 2>NUL
g++ -O2 -std=c++23 -Wl,--stack,268435456 -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp
if exist tester\cur_div.exe (echo [OK] cur_div compiled) else (echo [FAIL] compile error)
