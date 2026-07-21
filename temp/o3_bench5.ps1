param([int]$runs = 7)

$testInput = "123 456"

$binaries = @(
    "O2 (GCC15 default)|.\o3_verify_O2.exe",
    "O2-novec (sim GCC11)|.\o3_verify_O2NOVEC.exe",
    "O2+pragma O3,unroll |.\o3_verify_PRG.exe",
    "O3 (cmdline)        |.\o3_verify_O3.exe",
    "O2-novec+pragma     |.\o3_verify_NOVEC_PRG.exe"
)

Write-Output "=== Windows 5-version benchmark (MinGW g++ 15.2.0) ==="
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
    $sorted = $times | Sort-Object
    $med = $sorted[[Math]::Floor($runs / 2)]
    Write-Output ("{0}: avg={1,7:N2}  min={2,7:N2}  med={3,7:N2}  ms" -f $name, $avg, $min, $med)
}

Write-Output ""
Write-Output "Assembly vectorization summary (packed instrs / scalar instrs):"
Write-Output "  O2          : 2 packed / 4 scalar  (vectorized)"
Write-Output "  O2-novec    : 0 packed / 4 scalar  (NOT vectorized)"
Write-Output "  O2+pragma   : 16 packed / 32 scalar (vectorized + 8x unrolled)"
Write-Output "  O3          : 2 packed / 4 scalar  (same as O2)"
Write-Output "  O2-novec+prg: 0 packed / 36 scalar (NOT vectorized, but unrolled)"
