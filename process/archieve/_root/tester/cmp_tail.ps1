param([string]$Case = 'div_div_medium_98_0')
$base = "d:\precious_speed\tester\failures\$Case"
$r = Get-Content "$base.ref.out" -Raw
$n = Get-Content "$base.new.out" -Raw
Write-Host "ref len: $($r.Length)"
Write-Host "new len: $($n.Length)"
$min = [Math]::Min($r.Length, $n.Length)
$diffAt = -1
for ($i = 0; $i -lt $min; $i++) {
    if ($r[$i] -ne $n[$i]) { $diffAt = $i; break }
}
Write-Host "first diff at: $diffAt (of $min)"
if ($diffAt -ge 0) {
    $start = [Math]::Max(0, $diffAt - 80)
    $end = [Math]::Min($min, $diffAt + 80)
    Write-Host "--- ref [$start..$end] ---"
    $r.Substring($start, $end - $start)
    Write-Host "--- new [$start..$end] ---"
    $n.Substring($start, $end - $start)
}
Write-Host "--- ref last 200 ---"
$r.Substring([Math]::Max(0, $r.Length - 200))
Write-Host "--- new last 200 ---"
$n.Substring([Math]::Max(0, $n.Length - 200))
