$bytes = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\div_div_medium_98_0.in')
Write-Host "total bytes: $($bytes.Length)"
# Find first newline
$firstNL = -1
for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($bytes[$i] -eq 0x0A) { $firstNL = $i; break }
}
Write-Host "first newline at: $firstNL"
Write-Host "line0 content: $([System.Text.Encoding]::ASCII.GetString($bytes, 0, $firstNL))"
# Find second newline
$secondNL = -1
for ($i = $firstNL + 1; $i -lt $bytes.Length; $i++) {
    if ($bytes[$i] -eq 0x0A) { $secondNL = $i; break }
}
Write-Host "second newline at: $secondNL"
Write-Host "line1 length: $($secondNL - $firstNL - 1)"
# Check for space in line1
$line1 = [System.Text.Encoding]::ASCII.GetString($bytes, $firstNL + 1, $secondNL - $firstNL - 1)
$spaceIdx = $line1.IndexOf(' ')
Write-Host "line1 space at: $spaceIdx"
if ($spaceIdx -ge 0) {
    Write-Host "line1 A length: $spaceIdx"
    Write-Host "line1 B length: $($line1.Length - $spaceIdx - 1)"
    Write-Host "line1 A first 40: $($line1.Substring(0, 40))"
    Write-Host "line1 B first 40: $($line1.Substring($spaceIdx + 1, 40))"
}
# Count total newlines
$nlCount = 0
foreach ($b in $bytes) { if ($b -eq 0x0A) { $nlCount++ } }
Write-Host "total newlines: $nlCount"
# Count spaces
$spCount = 0
foreach ($b in $bytes) { if ($b -eq 0x20) { $spCount++ } }
Write-Host "total spaces: $spCount"
