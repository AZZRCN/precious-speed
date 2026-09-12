Set-Location d:\precious_speed
# Check debug log content
Write-Host '=== Full debug log ==='
Get-Content 'd:\precious_speed\tester\debug_120.log' | ForEach-Object { Write-Host $_ }

# Check if cyclic is used at all - add a use_cyclic counter
Write-Host ''
Write-Host '=== Check div_medium seed=98 sizes ==='
$lines = [System.IO.File]::ReadAllLines('d:\precious_speed\tester\gen_98.in')
for ($i = 1; $i -le 5; $i++) {
    $p = $lines[$i].Split(' ')
    Write-Host "Pair[$i]: A.len=$($p[0].Length) B.len=$($p[1].Length)"
}
# The failing pair is around 119-120
for ($i = 119; $i -le 120; $i++) {
    $p = $lines[$i].Split(' ')
    Write-Host "Pair[$i]: A.len=$($p[0].Length) B.len=$($p[1].Length)"
}
