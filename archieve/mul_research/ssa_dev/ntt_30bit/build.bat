@echo off
REM build.bat - 编译30位NTT原型
d:\precious_speed\toolbox\tto.exe 30 g++ -std=c++20 -O2 -march=native -o ntt_proto.exe proto.cpp
echo RC=%ERRORLEVEL%
