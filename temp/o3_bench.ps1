param([int]$runs = 7)

$testInput = "123 456"

$binaries = @(
    "O2 (baseline)|.\o3_verify_O2.exe",
    "O2+pragma O3 |.\o3_verify_PRG.exe",
    "O3 (cmdline) |.\o3_verify_O3.exe"
)

Write-Output "=== Windows O2 vs O2+pragma vs O3 benchmark ==="
Write-Output ("Input: '{0}', runs: {1}" -f $testInput, $runs)
Write-Output ""

foreach ($b in $binaries) {
    $parts = $b.Split('|')
    $name = $parts[0]
    $exe = $parts[1]
    $testInput | & $exe | Out-Null
}

foreach ($b in $binaries) {
    $parts = $b.Split('|')
    $name = $parts[0]
    $exe = $parts[1]
    $times = @()
    for ($i = 0; $i -lt $runs; $i++) {
        $ms = (Measure-Command {
            $testInput | & $exe | Out-Null
        }).TotalMilliseconds
        $times += $ms
    }
    $avg = ($times | Measure-Object -Average).Average
    $min = ($times | Measure-Object -Minimum).Minimum
    $max = ($times | Measure-Object -Maximum).Maximum
    $sorted = $times | Sort-Object
    $med = $sorted[[Math]::Floor($runs / 2)]
    Write-Output ("{0}: avg={1,7:N2}  min={2,7:N2}  med={3,7:N2}  max={4,7:N2}  ms" -f $name, $avg, $min, $med, $max)
    Write-Output ("    raw: {0}" -f ($times -join ", "))
}

Write-Output ""
Write-Output "Note: PowerShell pipe adds constant overhead to all three."
