@echo off
cd /d d:\precious_speed
echo === Compare run1 (single-thread) vs REF ===
fc /b tester\run1.out tester\failures\div_div_medium_98_0.ref.out >NUL 2>&1
if %errorlevel%==0 (echo [MATCH] single-thread == REF [CORRECT]) else (echo [DIFFER] single-thread != REF [STILL WRONG])
echo.
echo === Line count ===
powershell -Command "$r1=(Get-Content 'tester\run1.out').Count; $ref=(Get-Content 'tester\failures\div_div_medium_98_0.ref.out').Count; Write-Host ('run1.lines=' + $r1 + ' ref.lines=' + $ref)"
echo.
echo === Find first diff between run1 and REF ===
powershell -Command "$r1=Get-Content 'tester\run1.out'; $ref=Get-Content 'tester\failures\div_div_medium_98_0.ref.out'; $d=-1; for($i=0;$i -lt [Math]::Min($r1.Count,$ref.Count);$i++){if($r1[$i] -ne $ref[$i]){$d=$i;break}}; Write-Host ('First diff line: ' + $d); if($d -ge 0){Write-Host ('  run1.len=' + $r1[$d].Length + ' ref.len=' + $ref[$d].Length)}"
