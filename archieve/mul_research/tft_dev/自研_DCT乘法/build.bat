@echo off
cd /d "%~dp0"
echo [BUILD] 自研_DCT乘法 proto.cpp
d:\precious_speed\toolbox\tto.exe 30 g++ -std=c++20 -O2 -march=native proto.cpp -o proto.exe
if %ERRORLEVEL% == 0 (
    echo [BUILD] SUCCESS: proto.exe generated
) else (
    echo [BUILD] FAILED with code %ERRORLEVEL%
)
