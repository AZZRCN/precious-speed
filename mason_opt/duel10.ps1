# 10 轮 ORIG / OPT 交替测试
# 用法: .\duel10.ps1 <scale> <op> <inner_round>
# 例:   .\duel10.ps1 1000000 div2 3
param(
    [int]$scale = 1000000,
    [string]$op = "div2",
    [int]$inner = 3
)

Set-Location d:\precious_speed\mason_opt

# 编译两个版本
Write-Host "Compiling ORIG..."
g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o duel_orig.exe tle_duel.cpp
Write-Host "Compiling OPT..."
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_opt.exe tle_duel.cpp

# 交替 10 轮
$origTimes = @()
$optTimes = @()
for ($i = 1; $i -le 10; $i++) {
    # ORIG
    $out = .\duel_orig.exe $inner $scale $op "O$i"
    $t = [double]($out.Split(",")[1])
    $origTimes += $t
    Write-Host ("ORIG #{0,2}: {1:F3} ms" -f $i, $t)

    # OPT
    $out = .\duel_opt.exe $inner $scale $op "P$i"
    $t = [double]($out.Split(",")[1])
    $optTimes += $t
    Write-Host ("OPT  #{0,2}: {1:F3} ms" -f $i, $t)
}

# 统计
$origSum = ($origTimes | Measure-Object -Sum).Sum
$optSum = ($optTimes | Measure-Object -Sum).Sum
$origMed = ($origTimes | Sort-Object | Select-Object -Skip 4 -First 2 | Measure-Object -Average).Average
$optMed = ($optTimes | Sort-Object | Select-Object -Skip 4 -First 2 | Measure-Object -Average).Average
$ratio = $optSum / $origSum

Write-Host ""
Write-Host ("=== {0} scale={1} inner={2} ===" -f $op, $scale, $inner)
Write-Host ("ORIG sum={0:F3}ms median={1:F3}ms" -f $origSum, $origMed)
Write-Host ("OPT  sum={0:F3}ms median={1:F3}ms" -f $optSum, $optMed)
Write-Host ("OPT/ORIG ratio = {0:F4}  ({1:P1})" -f $ratio, ($ratio - 1))
Write-Host ""
Write-Host "ORIG: $($origTimes -join '  ')"
Write-Host "OPT : $($optTimes -join '  ')"
