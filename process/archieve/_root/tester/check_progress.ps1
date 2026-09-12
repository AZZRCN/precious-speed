$logPath = 'd:\precious_speed\tester\full_test.log'
if (Test-Path $logPath) {
    $f = Get-Item $logPath
    Write-Host "log size: $($f.Length) bytes, modified: $($f.LastWriteTime)"
    Write-Host '--- last 30 lines ---'
    Get-Content $logPath -Tail 30
} else {
    Write-Host 'log file not found'
}
# Also check failures dir
$failDir = 'd:\precious_speed\tester\failures'
if (Test-Path $failDir) {
    $files = Get-ChildItem $failDir
    Write-Host ""
    Write-Host "Failures dir: $($files.Count) files"
    if ($files.Count -gt 0) {
        $files | ForEach-Object { Write-Host "  $($_.Name)" }
    }
}
