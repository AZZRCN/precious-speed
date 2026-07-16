@echo off
cd /d d:\precious_speed\toolbox\src
echo ============================================================
echo Building tbx (toolbox) ...
echo ============================================================
g++ -O2 -std=c++17 -s -o ..\tbx.exe main.cpp cmd_split.cpp cmd_sam.cpp cmd_search.cpp cmd_es.cpp cmd_file.cpp -lwinhttp
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo.
echo Build OK: d:\precious_speed\toolbox\tbx.exe
echo.
echo Quick test:
..\tbx.exe 2>nul
echo.
..\tbx.exe sam -h 2>nul
echo.
echo Done. Add d:\precious_speed\toolbox to PATH or use full path.
