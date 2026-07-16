# bench_inv_k.ps1 - 对比 INV_NEWTON_BASE_K 不同阈值的性能
# Usage: .\bench_inv_k.ps1
# 编译 K=16/32/64 三个版本，在多个规模上各跑 N 轮取最小值
param(
    [int]$Rounds = 6
)

Set-Location d:\precious_speed\mason_opt

$ks = @(16, 32, 64)
foreach ($k in $ks) {
    Write-Host "Compiling K=$k..."
    g++ -O3 -mavx2 -mfma -funroll-loops -DINV_NEWTON_BASE_K=$k -o "bench_k$k.exe" bench_div.cpp 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host "K=$k compile FAIL"; exit 1 }
}

# 测试矩阵: scale, op
$tests = @(
    @{scale=10000; op="div2"},
    @{scale=10000; op="div4"},
    @{scale=50000; op="div2"},
    @{scale=50000; op="div4"},
    @{scale=100000; op="div2"},
    @{scale=100000; op="div4"},
    @{scale=100000; op="div8"},
    @{scale=250000; op="div2"},
    @{scale=250000; op="div4"},
    @{scale=1000000; op="div2"}
)

foreach ($t in $tests) {
    $scale = $t.scale; $op = $t.op
    Write-Host ""
    Write-Host ("=== scale={0} op={1} ===" -f $scale, $op)
    $results = @{}
    foreach ($k in $ks) {
        $times = @()
        for ($i = 1; $i -le $Rounds; $i++) {
            $out = & ".\bench_k$k.exe" 3 $scale $op "K${k}_$i"
            $lastLine = ($out | Select-Object -Last 1)
            $tm = [double]($lastLine.Split(',')[-1])
            $times += $tm
        }
        $sorted = $times | Sort-Object
        $min = $sorted[0]
        $med = ($sorted[[int]($Rounds/2)-1] + $sorted[[int]($Rounds/2)]) / 2.0
        $results[$k] = @{min=$min; med=$med; all=($times -join ' ')}
        Write-Host ("K={0,2}: min={1:F3} med={2:F3}  [{3}]" -f $k, $min, $med, ($times -join ' '))
    }
    $base = $results[16].min
    foreach ($k in $ks) {
        $delta = ($results[$k].min / $base - 1) * 100
        Write-Host ("  K={0,2} vs K=16: {1:F2}%" -f $k, $delta)
    }
}

# 清理
# Remove-Item bench_k*.exe -ErrorAction SilentlyContinue
Write-Host ""
Write-Host "Done."
