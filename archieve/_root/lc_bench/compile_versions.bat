@echo off
REM compile_versions.bat - Compile CUR/BEST x O2/O3 = 4 versions for add/mul/div
REM Usage: compile_versions.bat
setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set EXE=%~dp0exe
set SHIM=%ROOT%\archieve\win_memalign_shim.h

echo [compile_versions] Compiling 12 executables (3 ops x 4 versions)...

REM Common flags
set BASE_FLAGS=-std=c++20 -march=native -I.
set LDFLAGS=-lpthread

REM ====== ADD ======
echo [compile_versions] ADD: CUR_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 "%ROOT%\add.cpp" -o "%EXE%\cur_add_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_add_o2 & exit /b 1 )
echo [compile_versions] ADD: CUR_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 "%ROOT%\add.cpp" -o "%EXE%\cur_add_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_add_o3 & exit /b 1 )
echo [compile_versions] ADD: BEST_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 -include "%SHIM%" "%ROOT%\best\add.cpp" -o "%EXE%\best_add_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_add_o2 & exit /b 1 )
echo [compile_versions] ADD: BEST_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 -include "%SHIM%" "%ROOT%\best\add.cpp" -o "%EXE%\best_add_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_add_o3 & exit /b 1 )

REM ====== MUL ======
echo [compile_versions] MUL: CUR_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 "%ROOT%\mul.cpp" -o "%EXE%\cur_mul_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_mul_o2 & exit /b 1 )
echo [compile_versions] MUL: CUR_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 "%ROOT%\mul.cpp" -o "%EXE%\cur_mul_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_mul_o3 & exit /b 1 )
echo [compile_versions] MUL: BEST_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 -include "%SHIM%" "%ROOT%\best\mul.cpp" -o "%EXE%\best_mul_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_mul_o2 & exit /b 1 )
echo [compile_versions] MUL: BEST_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 -include "%SHIM%" "%ROOT%\best\mul.cpp" -o "%EXE%\best_mul_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_mul_o3 & exit /b 1 )

REM ====== DIV ======
echo [compile_versions] DIV: CUR_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 "%ROOT%\div.cpp" -o "%EXE%\cur_div_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_div_o2 & exit /b 1 )
echo [compile_versions] DIV: CUR_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 "%ROOT%\div.cpp" -o "%EXE%\cur_div_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: cur_div_o3 & exit /b 1 )
echo [compile_versions] DIV: BEST_O2
%TTO% 30 g++ %BASE_FLAGS% -O2 -include "%SHIM%" "%ROOT%\best\div.cpp" -o "%EXE%\best_div_o2.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_div_o2 & exit /b 1 )
echo [compile_versions] DIV: BEST_O3
%TTO% 30 g++ %BASE_FLAGS% -O3 -include "%SHIM%" "%ROOT%\best\div.cpp" -o "%EXE%\best_div_o3.exe" %LDFLAGS%
if !ERRORLEVEL! neq 0 ( echo FAIL: best_div_o3 & exit /b 1 )

echo [compile_versions] All 12 executables compiled.
exit /b 0
