@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "D:\precious_speed\superopt"
cl /nologo /O2 /GL /Oi /arch:AVX2 /std:c++17 /EHsc /DNDEBUG /Fe:"D:\precious_speed\superopt\so_core.exe" "D:\precious_speed\superopt\so_core.cpp" /link /LTCG
exit /b %ERRORLEVEL%
