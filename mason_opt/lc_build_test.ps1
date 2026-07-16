# Merge masonxiong_opt.cpp + lc_*.cpp main, compile, cross-check vs .NET BigInteger
$ErrorActionPreference = 'Stop'
$dir = 'd:\precious_speed\mason_opt'
$opt = Join-Path $dir 'masonxiong_opt.cpp'
$gcc = 'g++'
$flags = @('-O2','-std=c++23','-march=native','-mavx2','-mfma')

function Build-One($lcFile, $outExe) {
    # lc file layout: comments + #include + "// [paste...]" marker + #ifdef + namespace + main
    # extract everything after the marker (incl. #ifdef __linux__ block, namespace block, main),
    # append to OPT library tail
    $lcRaw = [System.IO.File]::ReadAllText($lcFile, [System.Text.Encoding]::UTF8)
    $marker = '// [PASTE masonxiong_opt.cpp HERE]'
    $idx = $lcRaw.IndexOf($marker)
    if ($idx -lt 0) { throw "marker not found in $lcFile" }
    $tail = $lcRaw.Substring($idx + $marker.Length)
    $optContent = [System.IO.File]::ReadAllText($opt, [System.Text.Encoding]::UTF8)
    $combined = $optContent + "`n" + $tail
    $tmp = Join-Path $dir '_lc_combined.cpp'
    [System.IO.File]::WriteAllText($tmp, $combined, [System.Text.UTF8Encoding]::new($false))
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $gcc @flags $tmp -o $outExe 2>&1 | ForEach-Object { Write-Host $_ }
    $ErrorActionPreference = $prev
    if (-not (Test-Path $outExe)) { throw "compile failed: $lcFile" }
    Remove-Item $tmp -Force
}

$exeAdd = Join-Path $dir '_lc_add.exe'
$exeMul = Join-Path $dir '_lc_mul.exe'
$exeDiv = Join-Path $dir '_lc_div.exe'

Write-Host '=== Compiling 3 LC files ===' -ForegroundColor Cyan
Build-One (Join-Path $dir 'lc_addition.cpp')        $exeAdd
Build-One (Join-Path $dir 'lc_multiplication.cpp')  $exeMul
Build-One (Join-Path $dir 'lc_division.cpp')        $exeDiv
Write-Host 'Compile done' -ForegroundColor Green

function Run-Exe($exe, $inp) {
    $tmpIn = Join-Path $dir '_in.txt'
    $tmpOut = Join-Path $dir '_out.txt'
    [System.IO.File]::WriteAllText($tmpIn, $inp, [System.Text.Encoding]::ASCII)
    if (Test-Path $tmpOut) { Remove-Item $tmpOut -Force }
    $proc = Start-Process -FilePath $exe -RedirectStandardInput $tmpIn -RedirectStandardOutput $tmpOut -NoNewWindow -Wait -PassThru
    if (-not (Test-Path $tmpOut)) { return '' }
    return [System.IO.File]::ReadAllText($tmpOut, [System.Text.Encoding]::ASCII)
}

function DotNet-Op($inp, $op) {
    $lines = $inp -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
    $t = [int]$lines[0]
    $out = New-Object System.Text.StringBuilder
    for ($i=1; $i -le $t; $i++) {
        $parts = $lines[$i] -split '\s+'
        $a = [bigint]::Parse($parts[0])
        $b = [bigint]::Parse($parts[1])
        if ($op -eq 'add') {
            $r = $a + $b
            [void]$out.AppendLine($r.ToString())
        } elseif ($op -eq 'mul') {
            $r = $a * $b
            [void]$out.AppendLine($r.ToString())
        } elseif ($op -eq 'div') {
            $q = $a / $b
            $r = $a - $q * $b
            [void]$out.AppendLine("$q $r")
        }
    }
    return $out.ToString().TrimEnd()
}

$sampleAdd = @"
9
47 10
50 10
3 -10
0 -10
-12 -34
12345678901234567890 98765432109876543210
-12345678901234567890 98765432109876543210
-12345678901234567890 -98765432109876543210
12345678901234567890 -12345678901234567890
"@

$sampleDiv = @"
6
47 10
50 10
3 10
0 10
100000000000000000000 9999999999
10000000000000000000000000000000000000000 99999999999999999999
"@

Write-Host ''
Write-Host '=== Sample tests ===' -ForegroundColor Cyan

$allPass = $true

$got = (Run-Exe $exeAdd $sampleAdd).Trim()
$exp = DotNet-Op $sampleAdd 'add'
if ($got -eq $exp) { Write-Host "[PASS] Addition sample" -ForegroundColor Green }
else {
    Write-Host "[FAIL] Addition sample" -ForegroundColor Red
    Write-Host "expect: $exp"; Write-Host "got:    $got"
    $allPass = $false
}

$got = (Run-Exe $exeMul $sampleAdd).Trim()
$exp = DotNet-Op $sampleAdd 'mul'
if ($got -eq $exp) { Write-Host "[PASS] Multiplication sample" -ForegroundColor Green }
else {
    Write-Host "[FAIL] Multiplication sample" -ForegroundColor Red
    Write-Host "expect: $exp"; Write-Host "got:    $got"
    $allPass = $false
}

$got = (Run-Exe $exeDiv $sampleDiv).Trim()
$exp = DotNet-Op $sampleDiv 'div'
if ($got -eq $exp) { Write-Host "[PASS] Division sample" -ForegroundColor Green }
else {
    Write-Host "[FAIL] Division sample" -ForegroundColor Red
    Write-Host "expect: $exp"; Write-Host "got:    $got"
    $allPass = $false
}

Write-Host ''
Write-Host '=== Random big-number tests ===' -ForegroundColor Cyan
$rng = [System.Random]::new(7777)

function Rand-BigStr($digits, $allowNeg) {
    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.Append($rng.Next(1,10))
    for ($i=1; $i -lt $digits; $i++) { [void]$sb.Append($rng.Next(0,10)) }
    $s = $sb.ToString()
    if ($allowNeg -and $rng.Next(0,2) -eq 1) { $s = '-' + $s }
    return $s
}

$scales = @(50, 500, 5000, 50000, 200000)

foreach ($n in $scales) {
    $a = Rand-BigStr $n $true
    $b = Rand-BigStr $n $true
    $inp = "1`n$a $b`n"
    $got = (Run-Exe $exeAdd $inp).Trim()
    $exp = DotNet-Op $inp 'add'
    if ($got -eq $exp) { Write-Host "[PASS] Addition  ${n} digits (signed)" -ForegroundColor Green }
    else {
        Write-Host "[FAIL] Addition  ${n} digits (signed)" -ForegroundColor Red
        Write-Host "expect head: $($exp.Substring(0,[Math]::Min(80,$exp.Length)))"
        Write-Host "got    head: $($got.Substring(0,[Math]::Min(80,$got.Length)))"
        $allPass = $false
    }
}

foreach ($n in $scales) {
    $a = Rand-BigStr $n $true
    $b = Rand-BigStr $n $true
    $inp = "1`n$a $b`n"
    $got = (Run-Exe $exeMul $inp).Trim()
    $exp = DotNet-Op $inp 'mul'
    if ($got -eq $exp) { Write-Host "[PASS] Multiply   ${n} digits (signed)" -ForegroundColor Green }
    else {
        Write-Host "[FAIL] Multiply   ${n} digits (signed)" -ForegroundColor Red
        Write-Host "expect head: $($exp.Substring(0,[Math]::Min(80,$exp.Length)))"
        Write-Host "got    head: $($got.Substring(0,[Math]::Min(80,$got.Length)))"
        $allPass = $false
    }
}

foreach ($n in $scales) {
    $a = Rand-BigStr $n $false
    $halfN = [Math]::Max(1, [int]($n / 2))
    $b = Rand-BigStr $halfN $false
    $inp = "1`n$a $b`n"
    $got = (Run-Exe $exeDiv $inp).Trim()
    $exp = DotNet-Op $inp 'div'
    if ($got -eq $exp) { Write-Host "[PASS] Division  ${n} / ${halfN} digits" -ForegroundColor Green }
    else {
        Write-Host "[FAIL] Division  ${n} / ${halfN} digits" -ForegroundColor Red
        Write-Host "expect: $exp"; Write-Host "got:    $got"
        $allPass = $false
    }
}

Write-Host ''
if ($allPass) { Write-Host 'ALL PASS' -ForegroundColor Green }
else { Write-Host 'SOME FAIL (see above)' -ForegroundColor Red }

Remove-Item $exeAdd,$exeMul,$exeDiv -ErrorAction SilentlyContinue
