$exe = "d:\precious_speed\tester\cur_div.exe"
$cases = @(
    @{seed=98;  in="d:\precious_speed\tester\failures\div_div_medium_98_0.in";  ref="d:\precious_speed\tester\failures\div_div_medium_98_0.ref.out"}
    @{seed=386; in="d:\precious_speed\tester\failures\div_div_medium_386_1.in"; ref="d:\precious_speed\tester\failures\div_div_medium_386_1.ref.out"}
    @{seed=620; in="d:\precious_speed\tester\failures\div_div_medium_620_2.in"; ref="d:\precious_speed\tester\failures\div_div_medium_620_2.ref.out"}
)

$passed = 0
$failed = 0

foreach ($c in $cases) {
    $newFile = $c.in -replace '\.in$', '.new.out'
    
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    
    $p = [System.Diagnostics.Process]::Start($psi)
    
    # Write input bytes asynchronously to avoid deadlock
    $inBytes = [System.IO.File]::ReadAllBytes($c.in)
    $p.StandardInput.BaseStream.WriteAsync($inBytes, 0, $inBytes.Length).Wait()
    $p.StandardInput.Close()
    
    # Read output
    $outText = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit(30000)
    
    [System.IO.File]::WriteAllText($newFile, $outText)
    
    $refBytes = [System.IO.File]::ReadAllBytes($c.ref) | Where-Object { $_ -ne 13 }
    $newBytes = [System.IO.File]::ReadAllBytes($newFile) | Where-Object { $_ -ne 13 }
    
    if ($refBytes.Count -eq $newBytes.Count) {
        $same = $true
        for ($i = 0; $i -lt $refBytes.Count; $i++) {
            if ($refBytes[$i] -ne $newBytes[$i]) { $same = $false; break }
        }
    } else { $same = $false }
    
    if ($same) {
        Write-Host "[PASS] seed $($c.seed)"
        $passed++
    } else {
        Write-Host "[FAIL] seed $($c.seed) (ref=$($refBytes.Count) new=$($newBytes.Count))"
        if (-not $same) {
            # Find first diff
            $minLen = [Math]::Min($refBytes.Count, $newBytes.Count)
            for ($i = 0; $i -lt $minLen; $i++) {
                if ($refBytes[$i] -ne $newBytes[$i]) { Write-Host "  First diff at byte $i"; break }
            }
        }
        $failed++
    }
}

Write-Host "`n=== Results: $passed passed, $failed failed ==="