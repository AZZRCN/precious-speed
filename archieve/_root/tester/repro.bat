@echo off
cd /d d:\precious_speed
REM Extract single pair from line 240 of failure case, wrap with T=1
powershell -Command "$lines = (Get-Content 'tester\failures\div_div_medium_98_0.in' -Raw) -split \"`r?`n\" | Where-Object { $_.Length -gt 0 }; '1' | Set-Content 'tester\single.in'; $lines[240] | Add-Content 'tester\single.in'"
tester\cur_div.exe < tester\single.in > tester\single.cur.out 2>nul
tester\ref_div.exe < tester\single.in > tester\single.ref.out 2>nul
fc /b tester\single.ref.out tester\single.cur.out >NUL 2>&1
if %errorlevel%==0 (echo [PASS] single pair) else (echo [FAIL] single pair)
echo REF:
type tester\single.ref.out | powershell -Command "$parts = (Get-Content -Raw) -split ' '; Write-Host \"quot_len=$($parts[0].Length) rem_len=$($parts[1].Trim().Length)\""
echo CUR:
type tester\single.cur.out | powershell -Command "$parts = (Get-Content -Raw) -split ' '; Write-Host \"quot_len=$($parts[0].Length) rem_len=$($parts[1].Trim().Length)\""
