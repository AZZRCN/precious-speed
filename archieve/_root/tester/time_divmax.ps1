Set-Location d:\precious_speed
Write-Host '=== Timing div_max seed=0 ==='
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& tester\gen\div_max.exe 0 > tester\div_max_s0.in 2>$null
$genMs = $sw.ElapsedMilliseconds
Write-Host "  gen: ${genMs}ms, input size: $((Get-Item 'tester\div_max_s0.in').Length) bytes"

$sw.Restart()
& tester\ref_div.exe < tester\div_max_s0.in > tester\div_max_s0.ref.out 2>$null
$refMs = $sw.ElapsedMilliseconds
Write-Host "  ref: ${refMs}ms"

$sw.Restart()
& tester\cur_div.exe < tester\div_max_s0.in > tester\div_max_s0.cur.out 2>$null
$curMs = $sw.ElapsedMilliseconds
Write-Host "  cur: ${curMs}ms"

# Compare
$r1 = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\div_max_s0.cur.out')
$ref = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\div_max_s0.ref.out')
$r1Text = ([System.Text.Encoding]::ASCII.GetString($r1)) -replace "`r`n","`n"
$refText = [System.Text.Encoding]::ASCII.GetString($ref)
$r1Text = $r1Text.TrimEnd("`n")
$refText = $refText.TrimEnd("`n")
if ($r1Text -eq $refText) { Write-Host '  [PASS] div_max seed=0' }
else { Write-Host '  [FAIL] div_max seed=0' }
