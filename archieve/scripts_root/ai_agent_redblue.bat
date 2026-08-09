@echo off
REM ai_agent_redblue.bat - Red-Blue adversarial test for div.cpp
REM Usage: ai_agent_redblue.bat [round]
REM Round 1-5: different test focuses
REM Timeout: compile=30s, test=300s

setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set DIV_SRC=%ROOT%\div.cpp
set DIV_EXE=%ROOT%\cur_div.exe
set CXXFLAGS=-std=c++20 -O3 -march=native -funroll-loops -I.
set LDFLAGS=-lpthread
set REDBLUE_PY=%ROOT%\div_dev\lc_test\red_blue_div.py

set ROUND=%1
if "%ROUND%"=="" set ROUND=1

echo [redblue] Round %ROUND%
echo [redblue] Step 1: Compile div.cpp (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%DIV_SRC%" -o "%DIV_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [redblue] TIMEOUT: compilation killed & exit /b 124 )
if !RC! neq 0 ( echo [redblue] FAIL: compilation rc=!RC! & exit /b !RC! )
echo [redblue] OK: div.exe compiled

echo [redblue] Step 2: Run red-blue test (timeout=300s)
%TTO% 300 python "%REDBLUE_PY%" %ROUND%
set RC=!ERRORLEVEL!
if !RC!==124 ( echo [redblue] TIMEOUT: test killed & exit /b 124 )
echo [redblue] Test rc=!RC!
exit /b !RC!
