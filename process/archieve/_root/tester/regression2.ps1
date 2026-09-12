Set-Location d:\precious_speed
# Clean old failures
Remove-Item 'd:\precious_speed\tester\failures\*' -ErrorAction SilentlyContinue
Write-Host 'Cleaned old failures.'
# Run regression: 2000 cases, 16 threads, quickMode=100
# Covers div_medium seeds 0-99 (includes failing seed 98)
Write-Host 'Running regression: 2000 cases, 16 threads, quickMode=100 ...'
& tester\tester.exe 2000 16 100 2>&1 | Out-Host
Write-Host ''
Write-Host "=== Exit code: $LASTEXITCODE"
# Check failures
$failDir = 'd:\precious_speed\tester\failures'
if (Test-Path $failDir) {
    $files = Get-ChildItem $failDir -ErrorAction SilentlyContinue
    if ($files -and $files.Count -gt 0) {
        Write-Host "NEW FAILURES: $($files.Count) files"
        $files | ForEach-Object { Write-Host "  $($_.Name)" }
    } else {
        Write-Host 'NO FAILURES - all tests passed!'
    }
}
