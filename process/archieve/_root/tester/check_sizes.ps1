Set-Location d:\precious_speed
# Read pair 120 (the failing pair in 98_0)
$lines = [System.IO.File]::ReadAllLines('d:\precious_speed\tester\gen_98.in')
$p = $lines[120].Split(' ')
$aLen = $p[0].Length
$bLen = $p[1].Length
Write-Host "Pair 120: A.len=$aLen B.len=$bLen"
Write-Host "  len1 = ceil($aLen / 4) = $([Math]::Ceiling($aLen / 4.0))"
Write-Host "  len2 = ceil($bLen / 4) = $([Math]::Ceiling($bLen / 4.0))"
Write-Host "  len1 - len2 = $([Math]::Ceiling($aLen / 4.0) - [Math]::Ceiling($bLen / 4.0))"
Write-Host "  len2*2 = $([Math]::Ceiling($bLen / 4.0) * 2)"
Write-Host "  len1 >= len2*2? $([Math]::Ceiling($aLen / 4.0) -ge ([Math]::Ceiling($bLen / 4.0) * 2))"
Write-Host "  len1 < len2*2? $([Math]::Ceiling($aLen / 4.0) -lt ([Math]::Ceiling($bLen / 4.0) * 2))"
# Also check pair 119
$p119 = $lines[119].Split(' ')
Write-Host "Pair 119: A.len=$($p119[0].Length) B.len=$($p119[1].Length)"
