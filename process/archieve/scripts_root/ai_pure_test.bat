@echo off
setlocal
d:\precious_speed\toolbox\tto.exe 60 d:\precious_speed\cur_div_pure.exe < d:\precious_speed\prof_in.txt > d:\precious_speed\pure_out.txt 2> d:\precious_speed\pure_err.txt
echo rc=%ERRORLEVEL%
echo --- stderr ---
type d:\precious_speed\pure_err.txt
echo --- output first line ---
d:\precious_speed\toolbox\tto.exe 5 powershell -c "(Get-Content d:\precious_speed\pure_out.txt -TotalCount 1).Substring(0,80)"
exit /b 0
