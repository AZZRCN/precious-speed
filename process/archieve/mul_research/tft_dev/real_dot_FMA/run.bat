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
echo [RUN] real_dot_FMA precision + speed demo
d:\precious_speed\toolbox\tto.exe 10 proto.exe
echo [RUN] Done (exit code %ERRORLEVEL%)
