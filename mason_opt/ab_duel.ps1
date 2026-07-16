# A/B 对比测试：当前工作区 vs HEAD（安全版本）
# 用 TEMP 目录备份，避免工作区文件冲突
param(
    [int]$scale = 1000000,
    [string]$op = "div2",
    [int]$inner = 5
)

Set-Location d:\precious_speed\mason_opt

# 安全备份到 TEMP 目录
$backupFile = Join-Path $env:TEMP "masonxiong_opt_backup_$(Get-Random).cpp"
Copy-Item masonxiong_opt.cpp $backupFile -Force
Write-Host "Backup saved to: $backupFile"

# 编译 NEW (当前工作区)
Write-Host "Compiling NEW (working)..."
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_new.exe tle_duel.cpp 2>$null
if (-not (Test-Path duel_new.exe)) {
    Write-Host "ERROR: NEW compilation failed"
    return
}

# 切换到 HEAD 版本
git checkout HEAD -- masonxiong_opt.cpp 2>$null
Write-Host "Compiling OLD (HEAD)..."
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_old.exe tle_duel.cpp 2>$null
if (-not (Test-Path duel_old.exe)) {
    Write-Host "ERROR: OLD compilation failed"
    Copy-Item $backupFile masonxiong_opt.cpp -Force
    return
}

# 恢复当前工作区版本
Copy-Item $backupFile masonxiong_opt.cpp -Force
Write-Host "Restored working copy"

# 预热
Write-Host "Warmup..."
$null = & .\duel_old.exe $inner $scale $op "W0" 2>$null
$null = & .\duel_new.exe $inner $scale $op "W0" 2>$null
$null = & .\duel_old.exe $inner $scale $op "W1" 2>$null
$null = & .\duel_new.exe $inner $scale $op "W1" 2>$null

# 交替 10 轮
$oldVals = @()
$newVals = @()
for ($i = 1; $i -le 10; $i++) {
    $out = & .\duel_old.exe $inner $scale $op "O$i" 2>$null
    $t = [double](($out -split ",")[1])
    $oldVals += $t
    Write-Host ("OLD #{0,2}: {1:F3} ms" -f $i, $t)

    $out = & .\duel_new.exe $inner $scale $op "N$i" 2>$null
    $t = [double](($out -split ",")[1])
    $newVals += $t
    Write-Host ("NEW #{0,2}: {1:F3} ms" -f $i, $t)
}

# 统计
$oldSum = ($oldVals | Measure-Object -Sum).Sum
$newSum = ($newVals | Measure-Object -Sum).Sum
$oldMin = ($oldVals | Measure-Object -Minimum).Minimum
$newMin = ($newVals | Measure-Object -Minimum).Minimum
$ratio = $newSum / $oldSum

Write-Host ""
Write-Host ("=== {0} scale={1} inner={2} ===" -f $op, $scale, $inner)
Write-Host ("OLD sum={0:F3}ms min={1:F3}ms" -f $oldSum, $oldMin)
Write-Host ("NEW sum={0:F3}ms min={1:F3}ms" -f $newSum, $newMin)
Write-Host ("NEW/OLD ratio = {0:F4}  ({1:P1})" -f $ratio, ($ratio - 1))
Write-Host ""
Write-Host ("OLD: " + ($oldVals -join '  '))
Write-Host ("NEW: " + ($newVals -join '  '))
