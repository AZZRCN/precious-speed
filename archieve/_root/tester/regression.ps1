Set-Location d:\precious_speed
# Clean old failures
Remove-Item 'd:\precious_speed\tester\failures\*' -ErrorAction SilentlyContinue
Write-Host 'Cleaned old failures.'
# Run regression: 200 cases, 16 threads, quickMode=10 (10 seeds per generator max)
# This avoids div_max seeds > 10 which have 10-min timeout
Write-Host 'Running regression: 200 cases, 16 threads, quickMode=10 ...'
& tester\tester.exe 200 16 10 2>&1 | Out-Host
Write-Host ''
Write-Host '=== Exit code:' $LASTEXITCODE
# Check failures
$failDir = 'd:\precious_speed\tester\failures'
if (Test-Path $failDir) {
    $files = Get-ChildItem $failDir -ErrorAction SilentlyContinue
    if ($files) {
        Write-Host "NEW FAILURES: $($files.Count) files"
        $files | ForEach-Object { Write-Host "  $($_.Name)" }
    } else {
        Write-Host 'NO FAILURES - all tests passed!'
    }
}
