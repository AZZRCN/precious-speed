$r = Get-Content 'd:\precious_speed\tester\failures\div_div_medium_98_0.ref.out' -Raw
$n = Get-Content 'd:\precious_speed\tester\failures\div_div_medium_98_0.new.out' -Raw
$rlines = $r -split "`n"
$nlines = $n -split "`n"
Write-Host "ref lines: $($rlines.Count) lengths: $($rlines[0].Length), $($rlines[1].Length)"
Write-Host "new lines: $($nlines.Count) lengths: $($nlines[0].Length), $($nlines[1].Length)"
Write-Host "ref line0 first 60: $($rlines[0].Substring(0,60))"
Write-Host "new line0 first 60: $($nlines[0].Substring(0,60))"
Write-Host "ref line0 last  60: $($rlines[0].Substring($rlines[0].Length-60))"
Write-Host "new line0 last  60: $($nlines[0].Substring($nlines[0].Length-60))"
Write-Host "ref line1 first 60: $($rlines[1].Substring(0,60))"
Write-Host "new line1 first 60: $($nlines[1].Substring(0,60))"
Write-Host "ref line1 last  60: $($rlines[1].Substring($rlines[1].Length-60))"
Write-Host "new line1 last  60: $($nlines[1].Substring($nlines[1].Length-60))"
