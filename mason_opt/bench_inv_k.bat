@echo off
cd /d d:\precious_speed\mason_opt
setlocal enabledelayedexpansion

REM bench_inv_k.bat - 对比 INV_NEWTON_BASE_K = 16/32/64
REM 每个测试跑 5 轮取最小值

set ROUNDS=5

REM 测试矩阵: scale op
set TESTS=10000 div2 10000 div4 50000 div2 50000 div4 100000 div2 100000 div4 100000 div8 250000 div2 250000 div4 1000000 div2

echo ============================================================
echo INV_NEWTON_BASE_K threshold comparison (5 rounds each, min)
echo ============================================================

set IDX=0
:LOOP
set /A IDX+=1
set SCALE=
set OP=
REM 取第 IDX*2-1 和 IDX*2 个 token
set TIDX=0
for %%T in (%TESTS%) do (
    set /A TIDX+=1
    if !TIDX!==!IDX!*2-1 set SCALE=%%T
    if !TIDX!==!IDX!*2 set OP=%%T
)
if "%SCALE%"=="" goto END

echo.
echo === scale=%SCALE% op=%OP% ===
for %%K in (16 32 64) do (
    set MIN=99999
    for /L %%I in (1,1,%ROUNDS%) do (
        for /F "tokens=2 delims=," %%A in ('bench_k%%K.exe 3 %SCALE% %OP% K%%K_%%I') do (
            if %%A LSS !MIN! set MIN=%%A
        )
    )
    echo K=%%K min=!MIN! ms
)

goto LOOP

:END
echo.
echo === Done ===
