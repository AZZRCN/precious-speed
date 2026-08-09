Set-Location d:\precious_speed
Write-Host '=== Step 1: Recompile cur_div.exe (cyclic DISABLED) ==='
Remove-Item tester\cur_div.exe -ErrorAction SilentlyContinue
& g++ -O2 -std=c++23 '-Wl,--stack,268435456' '-include' 'tester\mingw_compat.h' '-o' 'tester\cur_div.exe' 'div.cpp' 2>&1 | Out-Host
if (-not (Test-Path tester\cur_div.exe)) { Write-Host '[FAIL] compile error'; exit 1 }
Write-Host '[OK] compiled'

Write-Host ''
Write-Host '=== Step 2: Verify 3 previously failing cases ==='
$cases = @('div_div_medium_98_0','div_div_medium_386_1','div_div_medium_620_2')
$allPass = $true
foreach ($n in $cases) {
    $inPath = "d:\precious_speed\tester\failures\$n.in"
    $refPath = "d:\precious_speed\tester\failures\$n.ref.out"
    $outPath = "d:\precious_speed\tester\$n.verified.out"
    $proc = Start-Process -FilePath 'tester\cur_div.exe' -RedirectStandardInput $inPath -RedirectStandardOutput $outPath -NoNewWindow -Wait -PassThru
    $r1 = [System.IO.File]::ReadAllBytes($outPath)
    $ref = [System.IO.File]::ReadAllBytes($refPath)
    $r1Text = ([System.Text.Encoding]::ASCII.GetString($r1)) -replace "`r`n","`n"
    $refText = [System.Text.Encoding]::ASCII.GetString($ref)
    $r1Text = $r1Text.TrimEnd("`n")
    $refText = $refText.TrimEnd("`n")
    if ($r1Text -eq $refText) {
        Write-Host "  ${n}: [PASS]"
    } else {
        Write-Host "  ${n}: [FAIL]"
        $allPass = $false
        $r1L = $r1Text -split "`n"
        $refL = $refText -split "`n"
        for ($i=0; $i -lt [Math]::Min($r1L.Count,$refL.Count); $i++) {
            if ($r1L[$i] -ne $refL[$i]) {
                Write-Host "    diff at line $i : r1.len=$($r1L[$i].Length) ref.len=$($refL[$i].Length)"
                break
            }
        }
    }
}
if ($allPass) { Write-Host '  All 3 previously failing cases now PASS.' }
