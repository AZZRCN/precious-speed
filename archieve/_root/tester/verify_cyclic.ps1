Set-Location d:\precious_speed
$seeds = @(98, 386, 620)
foreach ($seed in $seeds) {
    Write-Host "=== Seed $seed ==="
    # Run cur_div_dbg (cyclic enabled)
    $inPath = "tester\gen_$seed.in"
    $outPath = "tester\gen_$seed.dbg.out"
    $refPath = "tester\gen_$seed.ref.out"
    
    # Generate REF
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'd:\precious_speed\tester\ref_div.exe'
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $inBytes = [System.IO.File]::ReadAllBytes("d:\precious_speed\$inPath")
    $proc.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
    $proc.StandardInput.Close()
    $refOut = $proc.StandardOutput.ReadToEnd()
    $proc.WaitForExit()
    [System.IO.File]::WriteAllText("d:\precious_speed\$refPath", $refOut)
    
    # Run CUR (debug version with cyclic enabled)
    $psi2 = New-Object System.Diagnostics.ProcessStartInfo
    $psi2.FileName = 'd:\precious_speed\tester\cur_div_dbg.exe'
    $psi2.RedirectStandardInput = $true
    $psi2.RedirectStandardOutput = $true
    $psi2.RedirectStandardError = $true
    $psi2.UseShellExecute = $false
    $psi2.CreateNoWindow = $true
    $proc2 = [System.Diagnostics.Process]::Start($psi2)
    $proc2.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
    $proc2.StandardInput.Close()
    $errTask = $proc2.StandardError.ReadToEndAsync()
    $curOut = $proc2.StandardOutput.ReadToEnd()
    $proc2.WaitForExit()
    $err = $errTask.Result
    [System.IO.File]::WriteAllText("d:\precious_speed\$outPath", $curOut)
    
    # Compare
    $refText = $refOut -replace "`r`n","`n"
    $curText = $curOut -replace "`r`n","`n"
    $refText = $refText.TrimEnd("`n")
    $curText = $curText.TrimEnd("`n")
    if ($refText -eq $curText) {
        Write-Host "  [PASS] content matches REF"
    } else {
        Write-Host "  [FAIL] content differs"
        Write-Host "  stderr length: $($err.Length)"
        $refL = $refText -split "`n"
        $curL = $curText -split "`n"
        for ($i=0; $i -lt [Math]::Min($refL.Count,$curL.Count); $i++) {
            if ($refL[$i] -ne $curL[$i]) {
                Write-Host "    diff at line $i : ref.len=$($refL[$i].Length) cur.len=$($curL[$i].Length)"
                break
            }
        }
    }
}
