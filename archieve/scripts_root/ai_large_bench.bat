@echo off
setlocal
set TTO=d:\precious_speed\toolbox\tto.exe
set ROOT=d:\precious_speed
echo [large] Generating large test cases (b_len=5000, q_len=5000, n=3)
%TTO% 60 python "%ROOT%\gen_prof_large.py"
echo [large] Pure div time (no I/O):
%TTO% 120 "%ROOT%\cur_div_pure.exe" < "%ROOT%\prof_in_large.txt" > "%ROOT%\large_out.txt" 2> "%ROOT%\large_err.txt"
type "%ROOT%\large_err.txt"
echo [large] Normal mode (with I/O):
%TTO% 120 "%ROOT%\cur_div.exe" < "%ROOT%\prof_in_large.txt" > "%ROOT%\large_out2.txt" 2> "%ROOT%\large_err2.txt"
echo [large] normal rc=%ERRORLEVEL%
exit /b 0
