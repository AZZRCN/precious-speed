$r = Get-Content 'd:\precious_speed\tester\failures\div_div_medium_98_0.ref.out' -Raw
$n = Get-Content 'd:\precious_speed\tester\single_98.cur.out' -Raw
$rlines = $r -split "`n"
$refQ = $rlines[0].Substring(0, 1173)
$refR = $rlines[0].Substring(1174)  # skip space
$newQ = $n.Substring(0, 1173)
$newR = $n.Substring(1174, $n.Length - 1174 - 1)  # skip trailing newline
Write-Host "Q match: $($refQ -eq $newQ)"
Write-Host "ref R len: $($refR.Length)  new R len: $($newR.Length)"
Write-Host "ref R first 60: $($refR.Substring(0, 60))"
Write-Host "new R first 60: $($newR.Substring(0, 60))"
Write-Host "ref R last 60: $($refR.Substring($refR.Length - 60))"
Write-Host "new R last 60: $($newR.Substring($newR.Length - 60))"
# Check if newR = refR + B or newR = refR (with leading zero stripped differently)
# Read B1 from input
$in = Get-Content 'd:\precious_speed\tester\single_98.in' -Raw
$inlines = $in -split "`n"
$b1 = $inlines[1].Split(' ')[1]
Write-Host "B1 len: $($b1.Length)"
Write-Host "B1 first 60: $($b1.Substring(0, 60))"
Write-Host "new R first 60: $($newR.Substring(0, 60))"
Write-Host "B1 last 60: $($b1.Substring($b1.Length - 60))"
Write-Host "new R last 60: $($newR.Substring($newR.Length - 60))"
# Is newR >= B1?
if ($newR.Length -gt $b1.Length) {
    Write-Host "newR > B1 (longer)"
} elseif ($newR.Length -lt $b1.Length) {
    Write-Host "newR < B1 (shorter)"
} else {
    $cmp = [string]::Compare($newR, $b1, [System.StringComparison]::Ordinal)
    if ($cmp -ge 0) { Write-Host "newR >= B1 (same length, cmp=$cmp)" }
    else { Write-Host "newR < B1 (same length, cmp=$cmp)" }
}
