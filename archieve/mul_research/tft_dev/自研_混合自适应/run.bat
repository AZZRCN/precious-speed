@echo off
cd /d "%~dp0"
if not exist proto.exe (
    echo [RUN] proto.exe not found, building...
    call build.bat
    if not exist proto.exe (
        echo [RUN] BUILD FAILED, cannot run
        exit /b 1
    )
)
echo [RUN] 自研_混合自适应 correctness + benchmark
d:\precious_speed\toolbox\tto.exe 20 proto.exe
echo [RUN] Done (exit code %ERRORLEVEL%)
