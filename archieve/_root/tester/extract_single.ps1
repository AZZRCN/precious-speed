$bytes = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\failures\div_div_medium_98_0.in')
# Find first newline (after count)
$firstNL = -1
for ($i = 0; $i -lt $bytes.Length; $i++) { if ($bytes[$i] -eq 0x0A) { $firstNL = $i; break } }
# Find second newline (end of line 1 = first test case)
$secondNL = -1
for ($i = $firstNL + 1; $i -lt $bytes.Length; $i++) { if ($bytes[$i] -eq 0x0A) { $secondNL = $i; break } }
# Write "1\n" + first test case line
$out = [System.IO.File]::Create('d:\precious_speed\tester\single_98.in')
$header = [System.Text.Encoding]::ASCII.GetBytes("1`n")
$out.Write($header, 0, $header.Length)
$out.Write($bytes, $firstNL + 1, $secondNL - $firstNL)
$out.Close()
Write-Host "wrote single_98.in with 1 test case"
Write-Host "test case line: $([System.Text.Encoding]::ASCII.GetString($bytes, $firstNL + 1, $secondNL - $firstNL - 1).Substring(0, 40))..."
