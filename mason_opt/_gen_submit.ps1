$opt = [System.IO.File]::ReadAllText('d:\precious_speed\mason_opt\masonxiong_opt.cpp', [System.Text.Encoding]::UTF8)
$lc = [System.IO.File]::ReadAllText('d:\precious_speed\mason_opt\lc_division.cpp', [System.Text.Encoding]::UTF8)
$marker = '// [PASTE masonxiong_opt.cpp HERE]'
$idx = $lc.IndexOf($marker)
$tail = $lc.Substring($idx + $marker.Length)
$combined = $opt + "`n" + $tail
[System.IO.File]::WriteAllText('d:\precious_speed\mason_opt\lc_division_submit.cpp', $combined, [System.Text.UTF8Encoding]::new($false))
$lines = $combined -split "`n"
Write-Host ("Total lines: " + $lines.Count)
Write-Host ("Line 1691: [" + $lines[1690] + "]")
Write-Host ("Line 1692: [" + $lines[1691] + "]")
Write-Host ("Line 1693: [" + $lines[1692] + "]")
Write-Host ("Last 5 lines:")
for ($i = $lines.Count - 5; $i -lt $lines.Count; $i++) {
    Write-Host ("  " + ($i + 1) + ": [" + $lines[$i] + "]")
}
