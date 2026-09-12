@echo off
cd /d d:\precious_speed
echo === Test: cyclic ENABLED + ±1 adjustment restored ===
del tester\cur_div.exe 2>NUL
g++ -O2 -std=c++23 -Wl,--stack,268435456 -include tester\mingw_compat.h -o tester\cur_div.exe div.cpp 2>&1
if not exist tester\cur_div.exe (echo [FAIL] compile error & exit /b 1)
echo [OK] compiled with cyclic ENABLED + ±1 adjustment
for %%N in (div_div_medium_98_0 div_div_medium_386_1 div_div_medium_620_2) do (
    tester\cur_div.exe < tester\failures\%%N.in > tester\%%N.adj.out 2>nul
    powershell -Command "$r1=[System.IO.File]::ReadAllBytes('d:\precious_speed\tester\%%N.adj.out'); $ref=[System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\%%N.ref.out'); $r1Text=[System.Text.Encoding]::ASCII.GetString($r1) -replace \"`r`n\",\"`n\"; $refText=[System.Text.Encoding]::ASCII.GetString($ref); if($r1Text -eq $refText){Write-Host '  %%N: [PASS]'}else{Write-Host '  %%N: [FAIL]'}"
)
