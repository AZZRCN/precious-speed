@echo off
cd /d d:\precious_speed
tester\bst_div.exe < tester\failures\div_div_medium_98_0.in > tester\failures\div_div_medium_98_0.bst.out 2>NUL
echo exit=%errorlevel%
fc /b tester\failures\div_div_medium_98_0.bst.out tester\failures\div_div_medium_98_0.ref.out >NUL 2>&1
if errorlevel 1 (echo [BST != REF]) else (echo [BST == REF])
