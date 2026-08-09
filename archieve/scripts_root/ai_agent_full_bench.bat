@echo off
REM ai_agent_full_bench.bat - Full benchmark: best VS cur for ADD/MUL/DIV under O2/O3
REM Uses LC generators from E:\library-checker-problems-master\big_integer
REM Usage: ai_agent_full_bench.bat
REM Timeout: 1800s (30 min) for entire pipeline

setlocal
set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
set SCRIPT=%ROOT%\run_full_bench.py

echo [full_bench] Starting full benchmark pipeline (timeout=1800s)
%TTO% 1800 python "%SCRIPT%"
set RC=!ERRORLEVEL!
echo [full_bench] rc=%RC%
exit /b %RC%
