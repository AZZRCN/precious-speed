@echo off
REM ai.bat - Unified entry with timeout (tto.exe)
REM Timeout: compile=30s, test/other=10s
REM Align with LC: -O2 -march=native (note: LC=AMD Zen3, local=Intel Tiger Lake)
REM Usage: ai.bat <step> [args...]
REM
REM Steps:
REM   compile_div    Compile div.cpp (30s)
REM   compile_add    Compile add.cpp (30s)
REM   compile_mul    Compile mul.cpp (30s)
REM   compile_mod    Compile div_dev/div_modular.cpp (30s)
REM   test_div       Run cur_div.exe (10s)
REM   test_add       Run cur_add.exe (10s)
REM   test_mul       Run cur_mul.exe (10s)
REM   test_mod       Run cur_mod.exe (10s)
REM   bench          Benchmark compare (10s each)
REM   all            compile_div + test_div
REM   custom <sec> <cmd> [args...]  Custom timeout command

setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set DIV_SRC=%ROOT%\div.cpp
set ADD_SRC=%ROOT%\add.cpp
set MUL_SRC=%ROOT%\mul.cpp
set DIV_EXE=%ROOT%\cur_div.exe
set ADD_EXE=%ROOT%\cur_add.exe
set MUL_EXE=%ROOT%\cur_mul.exe
set MOD_SRC=%ROOT%\div_dev\div_modular.cpp
set MOD_EXE=%ROOT%\div_dev\cur_mod.exe

REM LC-aligned: -O2 -march=native (drop -O3 -funroll-loops)
set CXXFLAGS=-std=c++20 -O2 -march=native -I.
set LDFLAGS=-lpthread

if "%~1"=="" goto usage
set STEP=%~1
shift

if /i "%STEP%"=="compile_div"  goto compile_div
if /i "%STEP%"=="compile_add"  goto compile_add
if /i "%STEP%"=="compile_mul"  goto compile_mul
if /i "%STEP%"=="compile_mod"  goto compile_mod
if /i "%STEP%"=="test_div"     goto test_div
if /i "%STEP%"=="test_add"     goto test_add
if /i "%STEP%"=="test_mul"     goto test_mul
if /i "%STEP%"=="test_mod"     goto test_mod
if /i "%STEP%"=="bench"        goto bench
if /i "%STEP%"=="custom"       goto custom
if /i "%STEP%"=="all"          goto all
goto unknown

REM ====== Compile div.cpp (30s) ======
:compile_div
echo [ai] compile_div (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%DIV_SRC%" -o "%DIV_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: div.cpp compilation killed & exit /b 124 )
if !RC!==0 ( echo [ai] OK: div.exe compiled ) else ( echo [ai] FAIL: div.cpp rc=!RC! & exit /b !RC! )
exit /b 0

REM ====== Compile add.cpp (30s) ======
:compile_add
echo [ai] compile_add (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%ADD_SRC%" -o "%ADD_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: add.cpp compilation killed & exit /b 124 )
if !RC!==0 ( echo [ai] OK: add.exe compiled ) else ( echo [ai] FAIL: add.cpp rc=!RC! & exit /b !RC! )
exit /b 0

REM ====== Compile mul.cpp (30s) ======
:compile_mul
echo [ai] compile_mul (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%MUL_SRC%" -o "%MUL_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: mul.cpp compilation killed & exit /b 124 )
if !RC!==0 ( echo [ai] OK: mul.exe compiled ) else ( echo [ai] FAIL: mul.cpp rc=!RC! & exit /b !RC! )
exit /b 0

REM ====== Compile div_modular.cpp (30s) ======
:compile_mod
echo [ai] compile_mod (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%MOD_SRC%" -o "%MOD_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: div_modular.cpp compilation killed & exit /b 124 )
if !RC!==0 ( echo [ai] OK: div_modular.exe compiled ) else ( echo [ai] FAIL: div_modular.cpp rc=!RC! & exit /b !RC! )
exit /b 0

REM ====== Test div (10s) ======
:test_div
echo [ai] test_div (timeout=10s)
%TTO% 10 "%DIV_EXE%"
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: div test killed & exit /b 124 )
echo [ai] test_div rc=!RC!
exit /b !RC!

REM ====== Test add (10s) ======
:test_add
echo [ai] test_add (timeout=10s)
%TTO% 10 "%ADD_EXE%"
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: add test killed & exit /b 124 )
echo [ai] test_add rc=!RC!
exit /b !RC!

REM ====== Test mul (10s) ======
:test_mul
echo [ai] test_mul (timeout=10s)
%TTO% 10 "%MUL_EXE%"
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: mul test killed & exit /b 124 )
echo [ai] test_mul rc=!RC!
exit /b !RC!

REM ====== Test mod (10s) ======
:test_mod
echo [ai] test_mod (timeout=10s)
%TTO% 10 "%MOD_EXE%"
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [ai] TIMEOUT: mod test killed & exit /b 124 )
echo [ai] test_mod rc=!RC!
exit /b !RC!

REM ====== Benchmark (10s each) ======
:bench
echo [ai] bench (timeout=10s each)
%TTO% 10 "%DIV_EXE%"
set RC1=!ERRORLEVEL!
%TTO% 10 "%MOD_EXE%"
set RC2=!ERRORLEVEL!
echo [ai] div rc=!RC1!  mod rc=!RC2!
exit /b 0

REM ====== Custom timeout command ======
:custom
REM Usage: ai.bat custom <seconds> <command> [args...]
if "%~1"=="" ( echo [ai] custom needs: ^<seconds^> ^<command^> [args...] & exit /b 1 )
set CSEC=%~1
shift
set CCMD=%~1
shift
set CARGS=
:custom_loop
if "%~1"=="" goto custom_run
set CARGS=!CARGS! %~1
shift
goto custom_loop
:custom_run
echo [ai] custom: %TTO% %CSEC% %CCMD% %CARGS%
%TTO% %CSEC% %CCMD% %CARGS%
exit /b !ERRORLEVEL!

REM ====== All steps ======
:all
call :compile_div
if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
call :test_div
if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
echo [ai] all steps done.
exit /b 0

:usage
echo Usage: ai.bat ^<step^> [args...]
echo Steps: compile_div ^| compile_add ^| compile_mul ^| compile_mod ^| test_div ^| test_add ^| test_mul ^| test_mod ^| bench ^| all ^| custom ^<sec^> ^<cmd^> [args]
exit /b 1

:unknown
echo [ai] unknown step: %STEP%
goto usage
