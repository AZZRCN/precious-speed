@echo off
cd /d d:\precious_speed
echo === Compiling cur_div.exe (current div.cpp state) ===
g++ -O2 -std=c++23 -Wl,--stack,268435456 -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp
if errorlevel 1 (
    echo [FAIL] Compile error
    exit /b 1
)
echo [OK] Compiled
echo.
echo === Testing 3 cases (CRLF-normalized compare) ===
setlocal enabledelayedexpansion
for %%C in (98 386 620) do (
    for /F "tokens=1,2" %%A in ('dir /b tester\failures\div_div_medium_%%C_*.in 2^>NUL') do (
        set "INFILE=tester\failures\%%A"
        set "REFFILE=!INFILE:.in=.ref.out!"
        set "NEWFILE=!INFILE:.in=.new.out!"
        tester\cur_div.exe < !INFILE! > !NEWFILE! 2>NUL
        rem Normalize CRLF to LF before compare (Windows text mode adds \r)
        powershell -Command "$r = [System.IO.File]::ReadAllBytes('!REFFILE!'); $n = [System.IO.File]::ReadAllBytes('!NEWFILE!'); $r2 = $r | Where-Object { $_ -ne 13 }; $n2 = $n | Where-Object { $_ -ne 13 }; $same = $r2.Count -eq $n2.Count; if ($same) { for ($i=0; $i -lt $r2.Count; $i++) { if ($r2[$i] -ne $n2[$i]) { $same=$false; break } } }; if ($same) { Write-Host '[PASS] %%A' } else { Write-Host '[FAIL] %%A' }"
    )
)
echo.
echo === Done ===
