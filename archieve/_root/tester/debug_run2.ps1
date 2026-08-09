Set-Location d:\precious_speed
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = 'd:\precious_speed\tester\cur_div_dbg.exe'
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [System.Diagnostics.Process]::Start($psi)
$inBytes = [System.IO.File]::ReadAllBytes('d:\precious_speed\tester\debug_120.in')
$proc.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
$proc.StandardInput.Close()
# Read stderr asynchronously
$errTask = $proc.StandardError.ReadToEndAsync()
$out = $proc.StandardOutput.ReadToEnd()
$proc.WaitForExit()
$err = $errTask.Result
Write-Host "Exit: $($proc.ExitCode)"
Write-Host "stderr length: $($err.Length)"
Write-Host "stderr first 2000 chars:"
Write-Host $err.Substring(0, [Math]::Min(2000, $err.Length))
