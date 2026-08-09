$r = Get-Content 'd:\precious_speed\tester\failures\div_div_medium_98_0.ref.out' -Raw
$n = Get-Content 'd:\precious_speed\tester\single_98.cur.out' -Raw
# ref line 0 is the first test case output
$rlines = $r -split "`n"
Write-Host "ref line0 len: $($rlines[0].Length)"
Write-Host "new single len: $($n.Length)"
Write-Host "ref line0 last 80: $($rlines[0].Substring($rlines[0].Length - 80))"
Write-Host "new single last 80: $($n.Substring($n.Length - 80))"
# Find space in both
$refSpace = $rlines[0].IndexOf(' ')
$newSpace = $n.IndexOf(' ')
Write-Host "ref space at: $refSpace (Q len = $refSpace, R len = $($rlines[0].Length - $refSpace - 1))"
Write-Host "new space at: $newSpace (Q len = $newSpace, R len = $($n.Length - $newSpace - 1 - $(if ($n[$n.Length-1] -eq "`n") {1} else {0})))"
