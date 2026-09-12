Set-Location d:\precious_speed
# Regenerate all 3 failing cases
$seeds = @(98, 386, 620)
foreach ($seed in $seeds) {
    & tester\gen\div_medium.exe $seed > "tester\gen_$seed.in" 2>$null
    Write-Host "Generated seed=$seed : $((Get-Item "tester\gen_$seed.in").Length) bytes"
}

# Run debug version on each full case
foreach ($seed in $seeds) {
    Write-Host ""
    Write-Host "=== Seed $seed ==="
    $inBytes = [System.IO.File]::ReadAllBytes("d:\precious_speed\tester\gen_$seed.in")
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'd:\precious_speed\tester\cur_div_dbg.exe'
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $proc.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
    $proc.StandardInput.Close()
    $errTask = $proc.StandardError.ReadToEndAsync()
    $out = $proc.StandardOutput.ReadToEnd()
    $proc.WaitForExit()
    $err = $errTask.Result
    Write-Host "Exit: $($proc.ExitCode), stderr length: $($err.Length)"
    if ($err.Length -gt 0) {
        # Count dispatch types
        $muCount = ([regex]::Matches($err, 'use_mu=1')).Count
        $core2Count = ([regex]::Matches($err, 'absDivNewtonCore2 ENTER')).Count
        $cyclicCount = ([regex]::Matches($err, 'use_cyclic=1')).Count
        Write-Host "  absDivMu calls: $muCount"
        Write-Host "  absDivNewtonCore2 calls: $core2Count"
        Write-Host "  cyclic enabled calls: $cyclicCount"
        # Show first few dispatch lines
        $dispatchLines = ($err -split "`n") | Where-Object { $_ -match 'dispatch:' } | Select-Object -First 5
        foreach ($d in $dispatchLines) { Write-Host "  $d".Trim() }
        # Show first few absDivMu ENTER lines
        $muLines = ($err -split "`n") | Where-Object { $_ -match 'absDivMu ENTER' } | Select-Object -First 5
        foreach ($m in $muLines) { Write-Host "  $m".Trim() }
    }
}
