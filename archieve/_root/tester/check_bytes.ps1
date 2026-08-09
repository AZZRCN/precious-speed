$r1 = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\run1.out')
$ref = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\div_div_medium_98_0.ref.out')
Write-Host ('run1 bytes=' + $r1.Length + ' ref bytes=' + $ref.Length)
# Compare byte by byte, find first diff
$min = [Math]::Min($r1.Length, $ref.Length)
$firstDiff = -1
for($i=0; $i -lt $min; $i++){
    if($r1[$i] -ne $ref[$i]){ $firstDiff = $i; break }
}
Write-Host ('First byte diff at: ' + $firstDiff)
if($firstDiff -ge 0){
    $start = [Math]::Max(0, $firstDiff - 10)
    $end = [Math]::Min($min, $firstDiff + 20)
    Write-Host ('  Context bytes [' + $start + '..' + $end + ']:')
    $r1Ctx = ($r1[$start..($end-1)] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
    $refCtx = ($ref[$start..($end-1)] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
    Write-Host ('  run1: ' + $r1Ctx)
    Write-Host ('  ref:  ' + $refCtx)
}
# Check line endings
$r1CR = 0; $r1LF = 0; $refCR = 0; $refLF = 0
foreach($b in $r1){ if($b -eq 13){$r1CR++} elseif($b -eq 10){$r1LF++} }
foreach($b in $ref){ if($b -eq 13){$refCR++} elseif($b -eq 10){$refLF++} }
Write-Host ('run1: CR=' + $r1CR + ' LF=' + $r1LF)
Write-Host ('ref:  CR=' + $refCR + ' LF=' + $refLF)
