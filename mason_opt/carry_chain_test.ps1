# carry_chain 边界测试：复现并验证 operator+= 进位 bug 修复
$ErrorActionPreference = 'Stop'
$dir = 'd:\precious_speed\mason_opt'
$opt = Join-Path $dir 'masonxiong_opt.cpp'
$gcc = 'g++'
$flags = @('-O2','-std=c++23','-march=native','-mavx2','-mfma')

# 合并编译 lc_addition.cpp
$lcFile = Join-Path $dir 'lc_addition.cpp'
$lcRaw = [System.IO.File]::ReadAllText($lcFile, [System.Text.Encoding]::UTF8)
$marker = '// [PASTE masonxiong_opt.cpp HERE]'
$idx = $lcRaw.IndexOf($marker)
if ($idx -lt 0) { throw "marker not found" }
$tail = $lcRaw.Substring($idx + $marker.Length)
$optContent = [System.IO.File]::ReadAllText($opt, [System.Text.Encoding]::UTF8)
$combined = $optContent + "`n" + $tail
$tmp = Join-Path $dir '_carry_combined.cpp'
$outExe = Join-Path $dir '_carry.exe'
[System.IO.File]::WriteAllText($tmp, $combined, [System.Text.UTF8Encoding]::new($false))
& $gcc @flags $tmp -o $outExe 2>&1 | ForEach-Object { Write-Host $_ }
if (-not (Test-Path $outExe)) { throw "compile failed" }
Remove-Item $tmp -Force

function Run-Exe($inp) {
    $tmpIn = Join-Path $dir '_cin.txt'
    $tmpOut = Join-Path $dir '_cout.txt'
    [System.IO.File]::WriteAllText($tmpIn, $inp, [System.Text.Encoding]::ASCII)
    if (Test-Path $tmpOut) { Remove-Item $tmpOut -Force }
    Start-Process -FilePath $outExe -RedirectStandardInput $tmpIn -RedirectStandardOutput $tmpOut -NoNewWindow -Wait | Out-Null
    if (-not (Test-Path $tmpOut)) { return '' }
    return [System.IO.File]::ReadAllText($tmpOut, [System.Text.Encoding]::ASCII).Trim()
}

function Nines($n) { return ('9' * $n) }
function Pow10($n) { return '1' + ('0' * $n) }

$allPass = $true

Write-Host '=== carry_chain boundary tests ===' -ForegroundColor Cyan

# Test 1: 9*15 + 1 = 10^15
$inp = "1`n$(Nines 15) 1`n"
$got = Run-Exe $inp
$exp = Pow10 15
if ($got -eq $exp) { Write-Host "[PASS] 9*15 + 1 = 10^15" -ForegroundColor Green }
else { Write-Host "[FAIL] 9*15 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 2: 9*16 + 1 = 10^16 (原 bug 触发点)
$inp = "1`n$(Nines 16) 1`n"
$got = Run-Exe $inp
$exp = Pow10 16
if ($got -eq $exp) { Write-Host "[PASS] 9*16 + 1 = 10^16 (was bug)" -ForegroundColor Green }
else { Write-Host "[FAIL] 9*16 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 3: 9*17 + 1 = 10^17
$inp = "1`n$(Nines 17) 1`n"
$got = Run-Exe $inp
$exp = Pow10 17
if ($got -eq $exp) { Write-Host "[PASS] 9*17 + 1 = 10^17" -ForegroundColor Green }
else { Write-Host "[FAIL] 9*17 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 4: 9*8 + 1 = 10^8 (单 digit 溢出)
$inp = "1`n$(Nines 8) 1`n"
$got = Run-Exe $inp
$exp = Pow10 8
if ($got -eq $exp) { Write-Host "[PASS] 9*8 + 1 = 10^8" -ForegroundColor Green }
else { Write-Host "[FAIL] 9*8 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 5: 9*24 + 1 = 10^24 (三个 digit 全溢出)
$inp = "1`n$(Nines 24) 1`n"
$got = Run-Exe $inp
$exp = Pow10 24
if ($got -eq $exp) { Write-Host "[PASS] 9*24 + 1 = 10^24" -ForegroundColor Green }
else { Write-Host "[FAIL] 9*24 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 6: 混合长度 1..20，每行 9*n + 1 = 10^n
$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine('20')
$expected = @()
for ($n=1; $n -le 20; $n++) {
    [void]$sb.AppendLine("$(Nines $n) 1")
    $expected += (Pow10 $n)
}
$inp = $sb.ToString()
$got = Run-Exe $inp
$gotLines = $got -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
$ok = $true
for ($i=0; $i -lt 20; $i++) {
    if ($gotLines[$i] -ne $expected[$i]) { $ok = $false; break }
}
if ($ok) { Write-Host "[PASS] mixed 9*n+1 for n=1..20 (line-by-line)" -ForegroundColor Green }
else { Write-Host "[FAIL] mixed: line $($i+1) expect=$($expected[$i]) got=$($gotLines[$i])" -ForegroundColor Red; $allPass = $false }

# Test 7: 负数 -9*16 + 1 = -(9*16 - 1) = -9999999999999998
$inp = "1`n-$(Nines 16) 1`n"
$got = Run-Exe $inp
$exp = '-' + (Nines 15) + '8'
if ($got -eq $exp) { Write-Host "[PASS] -9*16 + 1 = -(9*16-1)" -ForegroundColor Green }
else { Write-Host "[FAIL] -9*16 + 1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

# Test 8: 双负 -9*16 + -1 = -(10^16)
$inp = "1`n-$(Nines 16) -1`n"
$got = Run-Exe $inp
$exp = '-' + (Pow10 16)
if ($got -eq $exp) { Write-Host "[PASS] -9*16 + -1 = -10^16" -ForegroundColor Green }
else { Write-Host "[FAIL] -9*16 + -1 : expect=$exp got=$got" -ForegroundColor Red; $allPass = $false }

Write-Host ''
if ($allPass) { Write-Host 'ALL PASS' -ForegroundColor Green }
else { Write-Host 'SOME FAIL' -ForegroundColor Red }

Remove-Item $outExe -ErrorAction SilentlyContinue
