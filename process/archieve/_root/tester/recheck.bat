@echo off
cd /d d:\precious_speed
echo === Recompile cur_div.exe from current div.cpp ===
del tester\cur_div.exe 2>NUL
g++ -O2 -std=c++23 -Wl,--stack,268435456 -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp 2>&1
if exist tester\cur_div.exe (echo [OK] compiled) else (echo [FAIL] compile error & exit /b 1)
echo.
echo === Test all 3 failing cases ===
for %%N in (div_div_medium_98_0 div_div_medium_386_1 div_div_medium_620_2) do (
    tester\cur_div.exe < tester\failures\%%N.in > tester\%%N.new.out 2>nul
    echo --- %%N ---
    powershell -Command "$r1=[System.IO.File]::ReadAllBytes('d:\precious_speed\tester\%%N.new.out'); $ref=[System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\%%N.ref.out'); $r1Text=[System.Text.Encoding]::ASCII.GetString($r1) -replace \"`r`n\",\"`n\"; $refText=[System.Text.Encoding]::ASCII.GetString($ref); if($r1Text -eq $refText){Write-Host '[PASS] content matches REF'}else{Write-Host '[FAIL] content differs'; $r1L=($r1Text -split \"`n\"); $refL=($refText -split \"`n\"); for($i=0;$i -lt [Math]::Min($r1L.Count,$refL.Count);$i++){if($r1L[$i] -ne $refL[$i]){Write-Host ('  diff at line '+$i+': r1.len='+$r1L[$i].Length+' ref.len='+$refL[$i].Length); break}}}"
)
