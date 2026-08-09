@echo off
cd /d d:\precious_speed
echo === Run 1 ===
tester\cur_div.exe < tester\failures\div_div_medium_98_0.in > tester\run1.out 2>nul
echo exit=%errorlevel%
echo === Run 2 ===
tester\cur_div.exe < tester\failures\div_div_medium_98_0.in > tester\run2.out 2>nul
echo exit=%errorlevel%
fc /b tester\run1.out tester\run2.out >NUL 2>&1
if %errorlevel%==0 (echo [DETERMINISTIC] run1 == run2) else (echo [NONDET] run1 != run2)
fc /b tester\run1.out tester\failures\div_div_medium_98_0.cur.out >NUL 2>&1
if %errorlevel%==0 (echo [STABLE] run1 == original cur.out) else (echo [DIFFERS] run1 != original cur.out)
