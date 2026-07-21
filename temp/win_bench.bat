@echo off
REM win_bench.bat - Windows 本地 O2/O3 三版本对比 benchmark
REM 测试 fusion_o2only / fusion(v2) / moptm / best 在 O2/O3 下三题性能

setlocal
set WORK=d:\precious_speed\winbench
if not exist %WORK% mkdir %WORK%
cd /d %WORK%

REM 生成测试数据（Python）
python -c "import random; random.seed(42); a=''.join(random.choices('0123456789', k=1000000)); b=''.join(random.choices('0123456789', k=1000000)); open('add_1M.in','w').write(a+'\n'+b+'\n')"
python -c "import random; random.seed(42); a=''.join(random.choices('0123456789', k=500000)); b=''.join(random.choices('0123456789', k=500000)); open('mul_500k.in','w').write(a+'\n'+b+'\n')"
python -c "import random; random.seed(42); a=''.join(random.choices('0123456789', k=1000000)); b=''.join(random.choices('0123456789', k=500000)); open('div_1M_500k.in','w').write(a+'\n'+b+'\n')"

echo === Windows Local Benchmark ===
echo Date: %DATE% %TIME%
echo g++:
g++ --version | findstr /R "g++"
echo.

REM 编译所有组合
set SOURCES=fusion_o2only fusion moptm
for %%S in (%SOURCES%) do (
    for %%O in (O2 O3) do (
        for %%P in (ADD MUL DIV) do (
            echo Compiling %%S %%O %%P...
            if "%%S"=="fusion_o2only" set SRC=d:\precious_speed\fusion_o2only.cpp
            if "%%S"=="fusion" set SRC=d:\precious_speed\fusion.cpp
            if "%%S"=="moptm" set SRC=d:\precious_speed\archieve\cpp\moptm.cpp
            g++ -%%O -std=gnu++20 -DONLINE_JUDGE -DHINT_OP_%%P !SRC! -o %%S_%%O_%%P.exe 2>%%S_%%O_%%P.err
            if errorlevel 1 (echo FAIL %%S %%O %%P & type %%S_%%O_%%P.err)
        )
    )
)

REM best 三文件
for %%P in (ADD MUL DIV) do (
    for %%O in (O2 O3) do (
        echo Compiling best %%O %%P...
        set SRC=d:\precious_speed\best\%%P.cpp
        REM best 文件名小写
        if "%%P"=="ADD" set SRC=d:\precious_speed\best\add.cpp
        if "%%P"=="MUL" set SRC=d:\precious_speed\best\mul.cpp
        if "%%P"=="DIV" set SRC=d:\precious_speed\best\div.cpp
        g++ -%%O -std=gnu++20 -DONLINE_JUDGE !SRC! -o best_%%O_%%P.exe 2>best_%%O_%%P.err
        if errorlevel 1 (echo FAIL best %%O %%P & type best_%%O_%%P.err)
    )
)

echo.
echo === Running Benchmarks (5 runs, median) ===
echo Binary^|ADD_1M^|MUL_500k^|DIV_1M_500k > results.csv

REM 用 Python 跑 benchmark（PowerShell 计时不准）
python -c "import subprocess, time, statistics, os; work=r'd:\precious_speed\winbench'; binaries=['fusion_o2only_O2_ADD','fusion_o2only_O2_MUL','fusion_o2only_O2_DIV','fusion_o2only_O3_ADD','fusion_o2only_O3_MUL','fusion_o2only_O3_DIV','fusion_O2_ADD','fusion_O2_MUL','fusion_O2_DIV','fusion_O3_ADD','fusion_O3_MUL','fusion_O3_DIV','moptm_O2_ADD','moptm_O2_MUL','moptm_O2_DIV','moptm_O3_ADD','moptm_O3_MUL','moptm_O3_DIV','best_O2_ADD','best_O2_MUL','best_O2_DIV','best_O3_ADD','best_O3_MUL','best_O3_DIV']; inputs={'ADD':'add_1M.in','MUL':'mul_500k.in','DIV':'div_1M_500k.in'}; results={}; [results.update({b:[]}) for b in binaries];
import sys
for b in binaries:
    exe=os.path.join(work, b+'.exe')
    if not os.path.exists(exe):
        print(f'MISSING {b}'); continue
    op = 'ADD' if 'ADD' in b else ('MUL' if 'MUL' in b else 'DIV')
    infile = os.path.join(work, inputs[op])
    for _ in range(7):
        with open(infile, 'rb') as f:
            t0=time.perf_counter()
            r=subprocess.run([exe], stdin=f, capture_output=True, timeout=30)
            t1=time.perf_counter()
            if not r.stdout:
                print(f'FAIL {b} empty output'); break
            results[b].append((t1-t0)*1000)
    if results[b]:
        med = statistics.median(results[b])
        print(f'{b}: {med:.2f}ms (runs: {len(results[b])})')
        sys.stdout.flush()
" > bench_output.txt 2>&1
type bench_output.txt

echo.
echo === Done ===
endlocal
