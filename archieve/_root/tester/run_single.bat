@echo off
cd /d d:\precious_speed
echo === BST on single ===
tester\bst_div.exe < tester\single_98.in > tester\single_98.bst.out 2>NUL
echo bst exit=%errorlevel%
echo === CUR on single ===
tester\cur_div.exe < tester\single_98.in > tester\single_98.cur.out 2>NUL
echo cur exit=%errorlevel%
echo === Compare ===
fc /b tester\single_98.bst.out tester\single_98.cur.out >NUL 2>&1
if errorlevel 1 (echo [DIFF]) else (echo [MATCH])
echo === bst out first 100 ===
powershell -Command "(Get-Content 'tester\single_98.bst.out' -Raw).Substring(0,[Math]::Min(100,(Get-Content 'tester\single_98.bst.out' -Raw).Length))"
echo === cur out first 100 ===
powershell -Command "(Get-Content 'tester\single_98.cur.out' -Raw).Substring(0,[Math]::Min(100,(Get-Content 'tester\single_98.cur.out' -Raw).Length))"
