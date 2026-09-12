$r = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\div_div_medium_98_0.ref.out')
$n = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\single_98.cur.out')
Write-Host "ref bytes: $($r.Length)"
Write-Host "new bytes: $($n.Length)"
# Count \r and \n in each
$refCR = 0; $refLF = 0
foreach ($b in $r) { if ($b -eq 0x0D) { $refCR++ } elseif ($b -eq 0x0A) { $refLF++ } }
$newCR = 0; $newLF = 0
foreach ($b in $n) { if ($b -eq 0x0D) { $newCR++ } elseif ($b -eq 0x0A) { $newLF++ } }
Write-Host "ref CR=$refCR LF=$refLF"
Write-Host "new CR=$newCR LF=$newLF"
# Check last 5 bytes of each
Write-Host "ref last 5: $($r[$r.Length-5]) $($r[$r.Length-4]) $($r[$r.Length-3]) $($r[$r.Length-2]) $($r[$r.Length-1])"
Write-Host "new last 5: $($n[$n.Length-5]) $($n[$n.Length-4]) $($n[$n.Length-3]) $($n[$n.Length-2]) $($n[$n.Length-1])"
# Check if new has \r\n at position 9526 (where ref has \n)
Write-Host "ref byte 9526: $($r[9526]) (should be 10=LF)"
Write-Host "new byte 9526: $($n[9526]) (if 13=CR, then CRLF)"
Write-Host "new byte 9527: $($n[9527]) (should be 10=LF)"
