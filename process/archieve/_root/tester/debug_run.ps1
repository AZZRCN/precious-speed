Set-Location d:\precious_speed
# Regenerate failing case: div_medium seed=98
Write-Host 'Generating div_medium seed=98...'
& tester\gen\div_medium.exe 98 > tester\gen_98.in 2>$null
Write-Host "Generated: $((Get-Item 'tester\gen_98.in').Length) bytes"

# Extract first 120 pairs
$lines = [System.IO.File]::ReadAllLines('d:\precious_speed\tester\gen_98.in')
Write-Host "Total pairs: $($lines[0])"
$outPath = 'd:\precious_speed\tester\debug_120.in'
'120' | Set-Content $outPath -NoNewline
for ($i = 1; $i -le 120 -and $i -lt $lines.Count; $i++) {
    Add-Content $outPath $lines[$i]
}
Write-Host "Created debug_120.in with 120 pairs"

# Run debug version
Write-Host 'Running debug version...'
$inBytes = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\debug_120.in')
$inStr = [System.Text.Encoding]::ASCII.GetString($inBytes)

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = 'd:\precious_speed\tester\cur_div_dbg.exe'
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [System.Diagnostics.Process]::Start($psi)
$proc.StandardInput.Write($inStr)
$proc.StandardInput.Close()
$out = $proc.StandardOutput.ReadToEnd()
$err = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()
Write-Host "Exit: $($proc.ExitCode)"
Write-Host "stderr lines: $(($err -split "`n").Count)"

# Save stderr to file
$err | Set-Content 'd:\precious_speed\tester\debug_120.log' -Encoding utf8
Write-Host "Debug log saved"

# Show blocks where r != 0 after correction or correction limit reached
Write-Host ''
Write-Host '=== Blocks with r != 0 after correction ==='
$errLines = $err -split "`n"
$rNonZero = $errLines | Where-Object { $_ -match 'after corr: r=' -and $_ -notmatch 'r=0 corr_cnt=0' }
Write-Host "Count: $($rNonZero.Count)"
$rNonZero | Select-Object -First 30 | ForEach-Object { Write-Host $_ }

Write-Host ''
Write-Host '=== Correction limit reached ==='
$errLines | Where-Object { $_ -match 'CORRECTION LIMIT' } | Select-Object -First 10 | ForEach-Object { Write-Host $_ }
