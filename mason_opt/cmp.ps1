# cmp.ps1 — 优化前后对比
# 用法: .\cmp.ps1 <op> <scale> [rounds] [tle_sec]
# 流程:
#   1. git stash 保存当前未提交改动
#   2. 从 HEAD 编译 before 版本
#   3. git stash pop 恢复改动
#   4. 编译 after 版本
#   5. 交替运行对比
param(
    [Parameter(Mandatory=$true)][string]$Op,
    [Parameter(Mandatory=$true)][int]$Scale,
    [int]$Rounds = 30,
    [double]$TleSec = 10.0
)

$env:Path = "C:\Program Files\Git\cmd;$env:Path"
Set-Location "D:\precious_speed\mason_opt"

Write-Host "=== Backup working copy ==="
Copy-Item masonxiong_opt.cpp _working_copy.cpp -Force

Write-Host "=== Compile BEFORE (HEAD) ==="
git checkout HEAD -- masonxiong_opt.cpp 2>&1
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_before.exe tle_duel.cpp 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "BEFORE compile FAILED"; exit 1 }

Write-Host "=== Restore and compile AFTER (working) ==="
Copy-Item _working_copy.cpp masonxiong_opt.cpp -Force
Remove-Item _working_copy.cpp
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_after.exe tle_duel.cpp 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "AFTER compile FAILED"; exit 1 }

Write-Host "=== Duel: $Op @ ${Scale}digits, $Rounds rounds ==="
$beforeTotal = 0.0
$afterTotal = 0.0
$beforeTle = $false
$afterTle = $false

Write-Host "round  before(ms)  after(ms)  before_sum  after_sum"

for ($r = 1; $r -le $Rounds; $r++) {
    if ($r % 2 -eq 1) { $first = ".\duel_before.exe"; $second = ".\duel_after.exe"; $ft = "before"; $st = "after" }
    else { $first = ".\duel_after.exe"; $second = ".\duel_before.exe"; $ft = "after"; $st = "before" }

    $fMs = 0; $sMs = 0
    if ($ft -eq "before" -and -not $beforeTle) {
        $out = & $first 1 $Scale $Op "x" 2>&1
        $fMs = [double]($out -split ',')[1]
        $beforeTotal += $fMs
        if ($beforeTotal -gt $TleSec * 1000) { $beforeTle = $true }
    } elseif ($ft -eq "after" -and -not $afterTle) {
        $out = & $first 1 $Scale $Op "x" 2>&1
        $fMs = [double]($out -split ',')[1]
        $afterTotal += $fMs
        if ($afterTotal -gt $TleSec * 1000) { $afterTle = $true }
    }
    if ($st -eq "before" -and -not $beforeTle) {
        $out = & $second 1 $Scale $Op "x" 2>&1
        $sMs = [double]($out -split ',')[1]
        $beforeTotal += $sMs
        if ($beforeTotal -gt $TleSec * 1000) { $beforeTle = $true }
    } elseif ($st -eq "after" -and -not $afterTle) {
        $out = & $second 1 $Scale $Op "x" 2>&1
        $sMs = [double]($out -split ',')[1]
        $afterTotal += $sMs
        if ($afterTotal -gt $TleSec * 1000) { $afterTle = $true }
    }

    $dB = if ($r % 2 -eq 1) { $fMs } else { $sMs }
    $dA = if ($r % 2 -eq 1) { $sMs } else { $fMs }
    Write-Host ("{0,4}  {1,10:0.00}  {2,9:0.00}  {3,10:0.00}  {4,9:0.00}" -f $r, $dB, $dA, $beforeTotal, $afterTotal)
    if ($beforeTle -and $afterTle) { break }
}

Write-Host ""
Write-Host "=== Result ($Op @ ${Scale}) ==="
if ($beforeTle) { Write-Host "before: TLE (>${TleSec}s)" }
else { Write-Host ("before: {0:0.00} ms total, {1:0.00} ms avg" -f $beforeTotal, ($beforeTotal / $Rounds)) }
if ($afterTle) { Write-Host "after:  TLE (>${TleSec}s)" }
else { Write-Host ("after:  {0:0.00} ms total, {1:0.00} ms avg" -f $afterTotal, ($afterTotal / $Rounds)) }

if ($beforeTle -and -not $afterTle) { Write-Host "WINNER: after (before TLE)" }
elseif ($afterTle -and -not $beforeTle) { Write-Host "WINNER: before (after TLE)" }
elseif ($beforeTle -and $afterTle) { Write-Host "BOTH TLE" }
else {
    $ratio = $beforeTotal / $afterTotal
    if ($ratio -gt 1.02) { Write-Host ("WINNER: after ({0:0.00}x faster)" -f $ratio) }
    elseif ($ratio -lt 0.98) { Write-Host ("WINNER: before ({0:0.00}x faster)" -f (1/$ratio)) }
    else { Write-Host ("TIE (ratio {0:0.00})" -f $ratio) }
}
