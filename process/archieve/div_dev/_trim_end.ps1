$path = 'd:\precious_speed\div_dev\div_modular.cpp'
$lines = [System.IO.File]::ReadAllLines($path)
$before = $lines.Length
# Verify boundaries
Write-Host "Line 3938: '$($lines[3937])'"
Write-Host "Line 3939: '$($lines[3938])'"
Write-Host "Line 3942: '$($lines[3941])'"
Write-Host "Line 3986: '$($lines[3985])'"
Write-Host "Line 3987: '$($lines[3986])'"
Write-Host "Line 3988: '$($lines[3987])'"

# Delete lines 3939-3987 (1-based) = indices 3938-3986
# This includes: 3 comment lines, #ifdef DIAG_CYCLIC, body, #endif, and trailing empty line
$keep = New-Object System.Collections.Generic.List[string]
for ($i = 0; $i -lt 3938; $i++) { $keep.Add($lines[$i]) }
for ($i = 3987; $i -lt $lines.Length; $i++) { $keep.Add($lines[$i]) }
$content = ($keep -join "`r`n") + "`r`n"
[System.IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding $false))
$after = [System.IO.File]::ReadAllLines($path).Length
Write-Host "Lines before: $before, after: $after, deleted: $($before - $after)"
# Verify
$lines2 = [System.IO.File]::ReadAllLines($path)
Write-Host "=== After deletion ==="
Write-Host "Line 3937: '$($lines2[3936])'"
Write-Host "Line 3938: '$($lines2[3937])'"
Write-Host "Line 3939: '$($lines2[3938])'"
