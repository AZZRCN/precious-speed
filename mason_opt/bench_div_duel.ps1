# bench_div_duel.ps1 - 10-round ORIG(div_1st) vs WORK(div_work) alternating test
# Usage: .\bench_div_duel.ps1 <scale> <op> <inner_round> [-SkipCompile]
param(
    [int]$scale = 1000000,
    [string]$op = "div2",
    [int]$inner = 3,
    [switch]$SkipCompile
)

Set-Location d:\precious_speed\mason_opt

if (-not $SkipCompile) {
    Write-Host "Compiling ORIG (div_1st)..."
    g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o bench_div_orig.exe bench_div.cpp
    if ($LASTEXITCODE -ne 0) { Write-Host "ORIG compile FAIL"; exit 1 }
    Write-Host "Compiling WORK (div_work)..."
    g++ -O3 -mavx2 -mfma -funroll-loops -o bench_div_work.exe bench_div.cpp
    if ($LASTEXITCODE -ne 0) { Write-Host "WORK compile FAIL"; exit 1 }
}

$origTimes = New-Object System.Collections.ArrayList
$workTimes = New-Object System.Collections.ArrayList

for ($i = 1; $i -le 10; $i++) {
    $out = .\bench_div_orig.exe $inner $scale $op "O$i"
    $lastLine = ($out | Select-Object -Last 1)
    $t = [double]($lastLine.Split(',')[-1])
    $null = $origTimes.Add($t)
    Write-Host ("ORIG #{0,2}: {1:F3} ms" -f $i, $t)

    $out = .\bench_div_work.exe $inner $scale $op "W$i"
    $lastLine = ($out | Select-Object -Last 1)
    $t = [double]($lastLine.Split(',')[-1])
    $null = $workTimes.Add($t)
    Write-Host ("WORK #{0,2}: {1:F3} ms" -f $i, $t)
}

$origArr = [double[]]$origTimes.ToArray()
$workArr = [double[]]$workTimes.ToArray()
$origSorted = $origArr | Sort-Object
$workSorted = $workArr | Sort-Object

$origSum = ($origArr | Measure-Object -Sum).Sum
$workSum = ($workArr | Measure-Object -Sum).Sum
$origMin = $origSorted[0]
$workMin = $workSorted[0]
$origMax = $origSorted[-1]
$workMax = $workSorted[-1]
$origMean = $origSum / $origArr.Length
$workMean = $workSum / $workArr.Length

$origMed = ($origSorted[4] + $origSorted[5]) / 2.0
$workMed = ($workSorted[4] + $workSorted[5]) / 2.0

$origTrim = ($origSorted[2..7] | Measure-Object -Average).Average
$workTrim = ($workSorted[2..7] | Measure-Object -Average).Average

$ratioSum = $workSum / $origSum
$ratioMin = $workMin / $origMin
$ratioTrim = $workTrim / $origTrim

Write-Host ""
Write-Host ("=== {0} scale={1} inner={2} ===" -f $op, $scale, $inner)
Write-Host ("ORIG sum={0:F3} mean={1:F3} med={2:F3} trim={3:F3} min={4:F3} max={5:F3}" -f $origSum, $origMean, $origMed, $origTrim, $origMin, $origMax)
Write-Host ("WORK sum={0:F3} mean={1:F3} med={2:F3} trim={3:F3} min={4:F3} max={5:F3}" -f $workSum, $workMean, $workMed, $workTrim, $workMin, $workMax)
Write-Host ("RATIO sum={0:F4} trim={1:F4} min={2:F4}  (delta {3:P1} / {4:P1} / {5:P1})" -f $ratioSum, $ratioTrim, $ratioMin, ($ratioSum-1), ($ratioTrim-1), ($ratioMin-1))
Write-Host ""
Write-Host ("ORIG: {0}" -f ($origArr -join '  '))
Write-Host ("WORK: {0}" -f ($workArr -join '  '))
