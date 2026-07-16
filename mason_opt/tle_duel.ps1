# tle_duel.ps1 — 原版 vs 优化版交替 TLE 对决
# 交替运行 duel_orig.exe 和 duel_opt.exe，每方累计超 10s 判 TLE
param(
    [int]$MaxRounds = 50,
    [int]$Scale = 1000000,
    [string]$Op = "div2",
    [int]$RoundsPerRun = 1,
    [double]$TleLimitSec = 10.0
)

$orig = ".\duel_orig.exe"
$opt = ".\duel_opt.exe"

$origTotal = 0.0
$optTotal = 0.0
$origTle = $false
$optTle = $false

Write-Host "=== TLE Duel: $Op @ ${Scale}digits, $MaxRounds rounds, TLE=${TleLimitSec}s ==="
Write-Host "round  orig(ms)  opt(ms)  orig_sum  opt_sum"

for ($r = 1; $r -le $MaxRounds; $r++) {
    # 交替：奇数轮先 orig，偶数轮先 opt（减少缓存偏向）
    if ($r % 2 -eq 1) {
        $first = $orig; $second = $opt
        $firstTag = "orig"; $secondTag = "opt"
    } else {
        $first = $opt; $second = $orig
        $firstTag = "opt"; $secondTag = "orig"
    }

    $firstMs = 0; $secondMs = 0

    if ($firstTag -eq "orig" -and -not $origTle) {
        $out = & $first $RoundsPerRun $Scale $Op "orig" 2>&1
        $firstMs = [double]($out -split ',')[1]
        $origTotal += $firstMs
        if ($origTotal -gt $TleLimitSec * 1000) { $origTle = $true }
    } elseif ($firstTag -eq "opt" -and -not $optTle) {
        $out = & $first $RoundsPerRun $Scale $Op "opt" 2>&1
        $firstMs = [double]($out -split ',')[1]
        $optTotal += $firstMs
        if ($optTotal -gt $TleLimitSec * 1000) { $optTle = $true }
    }

    if ($secondTag -eq "orig" -and -not $origTle) {
        $out = & $second $RoundsPerRun $Scale $Op "orig" 2>&1
        $secondMs = [double]($out -split ',')[1]
        $origTotal += $secondMs
        if ($origTotal -gt $TleLimitSec * 1000) { $origTle = $true }
    } elseif ($secondTag -eq "opt" -and -not $optTle) {
        $out = & $second $RoundsPerRun $Scale $Op "opt" 2>&1
        $secondMs = [double]($out -split ',')[1]
        $optTotal += $secondMs
        if ($optTotal -gt $TleLimitSec * 1000) { $optTle = $true }
    }

    # 显示：orig 在前 opt 在后
    $dispOrig = if ($r % 2 -eq 1) { $firstMs } else { $secondMs }
    $dispOpt = if ($r % 2 -eq 1) { $secondMs } else { $firstMs }
    Write-Host ("{0,4}  {1,8:0.00}  {2,8:0.00}  {3,8:0.00}  {4,8:0.00}" -f $r, $dispOrig, $dispOpt, $origTotal, $optTotal)

    if ($origTle -and $optTle) { break }
}

Write-Host ""
Write-Host "=== Result ==="
if ($origTle) { Write-Host "orig: TLE (>${TleLimitSec}s)" }
else { Write-Host ("orig: {0:0.00} ms total, {1:0.00} ms avg" -f $origTotal, ($origTotal / $MaxRounds)) }
if ($optTle) { Write-Host "opt:  TLE (>${TleLimitSec}s)" }
else { Write-Host ("opt:  {0:0.00} ms total, {1:0.00} ms avg" -f $optTotal, ($optTotal / $MaxRounds)) }

if ($origTle -and -not $optTle) { Write-Host "WINNER: opt (orig TLE)" }
elseif ($optTle -and -not $origTle) { Write-Host "WINNER: orig (opt TLE)" }
elseif ($origTle -and $optTle) { Write-Host "BOTH TLE" }
else {
    $ratio = $origTotal / $optTotal
    if ($ratio -gt 1) { Write-Host ("WINNER: opt ({0:0.00}x faster)" -f $ratio) }
    else { Write-Host ("WINNER: orig ({0:0.00}x faster)" -f (1/$ratio)) }
}
