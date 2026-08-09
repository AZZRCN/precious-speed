# Manual CRLF-normalized comparison for seed 98
$refFile = 'd:\precious_speed\tester\failures\div_div_medium_98_0.ref.out'
$newFile = 'd:\precious_speed\tester\failures\div_div_medium_98_0.new.out'

$r = [System.IO.File]::ReadAllBytes($refFile)
$n = [System.IO.File]::ReadAllBytes($newFile)

# Strip \r (0x0D) from both
$r2 = [System.Collections.Generic.List[byte]]::new()
$n2 = [System.Collections.Generic.List[byte]]::new()
foreach ($b in $r) { if ($b -ne 13) { $r2.Add($b) } }
foreach ($b in $n) { if ($b -ne 13) { $n2.Add($b) } }

Write-Host "ref bytes: $($r.Length) -> stripped: $($r2.Count)"
Write-Host "new bytes: $($n.Length) -> stripped: $($n2.Count)"

if ($r2.Count -ne $n2.Count) {
    Write-Host "Length mismatch after CRLF strip: ref=$($r2.Count) new=$($n2.Count)"
    $min = [Math]::Min($r2.Count, $n2.Count)
    $diffAt = -1
    for ($i = 0; $i -lt $min; $i++) {
        if ($r2[$i] -ne $n2[$i]) { $diffAt = $i; break }
    }
    Write-Host "first diff at: $diffAt (of $min)"
    if ($diffAt -ge 0) {
        $start = [Math]::Max(0, $diffAt - 40)
        $end = [Math]::Min($min, $diffAt + 40)
        Write-Host "ref context:"
        $refCtx = [System.Text.Encoding]::ASCII.GetString($r2.ToArray(), $start, $end - $start)
        Write-Host $refCtx
        Write-Host "new context:"
        $newCtx = [System.Text.Encoding]::ASCII.GetString($n2.ToArray(), $start, $end - $start)
        Write-Host $newCtx
    }
} else {
    $same = $true
    for ($i = 0; $i -lt $r2.Count; $i++) {
        if ($r2[$i] -ne $n2[$i]) { $same = $false; Write-Host "diff at $i"; break }
    }
    if ($same) { Write-Host "[PASS] Identical after CRLF normalization" }
    else { Write-Host "[FAIL] Content differs" }
}
