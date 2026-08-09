@echo off
REM ai_agent_profile.bat - Profile div.cpp to find hot spots
REM Usage: ai_agent_profile.bat

setlocal enabledelayedexpansion

set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set DIV_SRC=%ROOT%\div.cpp
set DIV_EXE=%ROOT%\cur_div_prof.exe
set CXXFLAGS=-std=c++20 -O3 -march=native -funroll-loops -I. -DPROFILE_DIV
set LDFLAGS=-lpthread

echo [profile] Compiling div.cpp with PROFILE_DIV (timeout=30s)
%TTO% 30 g++ %CXXFLAGS% "%DIV_SRC%" -o "%DIV_EXE%" %LDFLAGS%
set RC=!ERRORLEVEL!
if !RC! neq 0 ( echo [profile] FAIL: compilation rc=!RC! & exit /b !RC! )
echo [profile] OK: div_prof.exe compiled

echo [profile] Generating large test case (b_len=1000, q_len=1000, n=3)
%TTO% 30 python "%ROOT%\gen_prof_in.py"

echo [profile] Running profile (timeout=60s)
%TTO% 60 "%DIV_EXE%" < "%ROOT%\prof_in.txt" > "%ROOT%\prof_out.txt"
set RC=!ERRORLEVEL!
if !RC! neq 0 ( echo [profile] FAIL: run rc=!RC! & exit /b !RC! )
echo [profile] OK: run completed

echo [profile] Profile log (last 80 lines):
type "%ROOT%\prof_detail.log" 2>nul | more +0

exit /b 0
