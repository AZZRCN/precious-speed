# bench_div_check.ps1 - div_work vs masonxiong_opt 对拍测试
# Usage: .\bench_div_check.ps1
Set-Location d:\precious_speed\mason_opt

Write-Host "Compiling test_work (div_work)..."
g++ -O3 -mavx2 -mfma -funroll-loops -o test_work.exe test_work.cpp
if ($LASTEXITCODE -ne 0) { Write-Host "test_work compile FAIL"; exit 1 }

Write-Host "Compiling test_mason (masonxiong_opt)..."
g++ -O3 -mavx2 -mfma -funroll-loops -o test_mason.exe test_mason.cpp
if ($LASTEXITCODE -ne 0) { Write-Host "test_mason compile FAIL"; exit 1 }

# 生成测试数据的函数
function Gen-Input($filename, $cases, $na, $nb) {
    $rng = New-Object System.Random(42)
    $lines = @()
    $lines += "$cases"
    for ($i = 0; $i -lt $cases; $i++) {
        $a = New-Object System.Text.StringBuilder
        $a.Append([char]('1' + $rng.Next(9))) | Out-Null
        for ($j = 1; $j -lt $na; $j++) { $a.Append([char]('0' + $rng.Next(10))) | Out-Null }
        $b = New-Object System.Text.StringBuilder
        $b.Append([char]('1' + $rng.Next(9))) | Out-Null
        for ($j = 1; $j -lt $nb; $j++) { $b.Append([char]('0' + $rng.Next(10))) | Out-Null }
        $lines += $a.ToString()
        $lines += $b.ToString()
    }
    $lines | Set-Content $filename -Encoding ASCII
}

$testCases = @(
    @{ name="small_eq";    cases=50; na=10;    nb=10 },
    @{ name="small_neq";   cases=50; na=20;    nb=10 },
    @{ name="mid_eq";      cases=20; na=1000;  nb=1000 },
    @{ name="mid_neq";     cases=20; na=2000;  nb=1000 },
    @{ name="large_eq";    cases=5;  na=10000; nb=10000 },
    @{ name="large_neq";   cases=5;  na=20000; nb=10000 },
    @{ name="huge_eq";     cases=2;  na=100000; nb=100000 },
    @{ name="huge_neq";    cases=2;  na=200000; nb=100000 },
    @{ name="div2";        cases=3;  na=200000; nb=100000 },
    @{ name="div4";        cases=2;  na=400000; nb=100000 },
    @{ name="div8";        cases=1;  na=800000; nb=100000 },
    @{ name="tiny_b";      cases=30; na=100;   nb=2 },
    @{ name="edge_a_eq_b"; cases=10; na=50;    nb=50 },
    @{ name="edge_a_lt_b"; cases=10; na=10;    nb=50 }
)

$allPass = $true
foreach ($tc in $testCases) {
    $name = $tc.name
    $cases = $tc.cases
    $na = $tc.na
    $nb = $tc.nb
    Write-Host ""
    Write-Host "=== $name (cases=$cases na=$na nb=$nb) ==="

    Gen-Input "input_$name.txt" $cases $na $nb

    # 用 cmd /c 执行输入重定向
    $outWork  = cmd /c ".\test_work.exe  < input_$name.txt" 2>&1
    $outMason = cmd /c ".\test_mason.exe < input_$name.txt" 2>&1

    # 逐行对比
    $linesWork = $outWork -split "`n" | Where-Object { $_.Trim() -ne "" }
    $linesMason = $outMason -split "`n" | Where-Object { $_.Trim() -ne "" }

    if ($linesWork.Count -ne $linesMason.Count) {
        Write-Host "  FAIL: line count mismatch ($($linesWork.Count) vs $($linesMason.Count))"
        $allPass = $false
        continue
    }

    $failCount = 0
    for ($i = 0; $i -lt $linesWork.Count; $i++) {
        if ($linesWork[$i].Trim() -ne $linesMason[$i].Trim()) {
            $failCount++
            if ($failCount -le 3) {
                $w = $linesWork[$i]
                $m = $linesMason[$i]
                if ($w.Length -gt 80) { $w = $w.Substring(0, 80) + "..." }
                if ($m.Length -gt 80) { $m = $m.Substring(0, 80) + "..." }
                Write-Host "  FAIL case $($i+1):"
                Write-Host "    work : $w"
                Write-Host "    mason: $m"
            }
        }
    }

    if ($failCount -eq 0) {
        Write-Host "  PASS ($cases cases)"
    } else {
        Write-Host "  FAIL: $failCount / $($linesWork.Count) cases"
        $allPass = $false
    }
}

Write-Host ""
if ($allPass) {
    Write-Host "========== ALL PASS =========="
} else {
    Write-Host "========== SOME FAILED =========="
}
