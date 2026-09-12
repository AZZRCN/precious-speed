@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
cd /d d:\precious_speed
cl /EHsc /O2 /std:c++17 /utf-8 /Fe:tester\tester.exe tester\main.cpp
echo EXIT_CODE=%errorlevel%
