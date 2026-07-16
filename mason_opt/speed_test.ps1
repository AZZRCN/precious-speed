# speed_test.ps1 - Original vs Optimized speed comparison
# Modular, bulk, interleaved execution; results written to MD
param(
    [int]$Rounds = 3
)

$env:Path = "C:\Program Files\Git\cmd;$env:Path"
Set-Location "D:\precious_speed\mason_opt"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$origFile = "masonxiong.cpp"
$optFile  = "masonxiong_opt.cpp"

Write-Host "=== Compiling ORIGINAL ($origFile) ==="
g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o duel_orig.exe tle_duel.cpp 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "ORIG compile FAILED"; exit 1 }

Write-Host "=== Compiling OPTIMIZED ($optFile) ==="
g++ -O3 -mavx2 -mfma -funroll-loops -o duel_opt.exe tle_duel.cpp 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "OPT compile FAILED"; exit 1 }

$tests = @(
    @{ op="ctor";  scale=1000;    inner=2000 },
    @{ op="ctor";  scale=10000;   inner=500  },
    @{ op="ctor";  scale=100000;  inner=100  },
    @{ op="ctor";  scale=1000000; inner=10   },
    @{ op="io";    scale=1000;    inner=500  },
    @{ op="io";    scale=10000;   inner=100  },
    @{ op="io";    scale=100000;  inner=20   },
    @{ op="io";    scale=1000000; inner=3    },
    @{ op="tostr"; scale=1000;    inner=5000 },
    @{ op="tostr"; scale=100000;  inner=100  },
    @{ op="tostr"; scale=1000000; inner=10   },
    @{ op="conv";  scale=1000;    inner=5000 },
    @{ op="conv";  scale=100000;  inner=200  },
    @{ op="conv";  scale=1000000; inner=20   },
    @{ op="cmp";   scale=1000;    inner=100000 },
    @{ op="cmp";   scale=100000;  inner=5000   },
    @{ op="cmp";   scale=1000000; inner=500    },
    @{ op="add";   scale=1000;    inner=100000 },
    @{ op="add";   scale=10000;   inner=10000  },
    @{ op="add";   scale=100000;  inner=1000   },
    @{ op="add";   scale=1000000; inner=50     },
    @{ op="sub";   scale=1000;    inner=100000 },
    @{ op="sub";   scale=100000;  inner=1000   },
    @{ op="sub";   scale=1000000; inner=50     },
    @{ op="mul";   scale=1000;    inner=2000 },
    @{ op="mul";   scale=10000;   inner=200  },
    @{ op="mul";   scale=100000;  inner=20   },
    @{ op="mul";   scale=1000000; inner=3    },
    @{ op="sqr";   scale=1000;    inner=3000 },
    @{ op="sqr";   scale=10000;   inner=300  },
    @{ op="sqr";   scale=100000;  inner=30   },
    @{ op="sqr";   scale=1000000; inner=3    },
    @{ op="mul2";  scale=1000;    inner=1000 },
    @{ op="mul2";  scale=10000;   inner=100  },
    @{ op="mul2";  scale=100000;  inner=10   },
    @{ op="mul2";  scale=1000000; inner=2    },
    @{ op="div";   scale=1000;    inner=500  },
    @{ op="div";   scale=10000;   inner=50   },
    @{ op="div";   scale=100000;  inner=5    },
    @{ op="div";   scale=1000000; inner=1    },
    @{ op="div2";  scale=1000;    inner=500  },
    @{ op="div2";  scale=10000;   inner=50   },
    @{ op="div2";  scale=100000;  inner=5    },
    @{ op="div2";  scale=1000000; inner=1    },
    @{ op="mod";   scale=1000;    inner=500  },
    @{ op="mod";   scale=100000;  inner=5    },
    @{ op="mod";   scale=1000000; inner=1    }
)

$grouped = $tests | Group-Object { $_.op }
$maxGroup = 0
foreach ($g in $grouped) { if ($g.Count -gt $maxGroup) { $maxGroup = $g.Count } }
$shuffled = @()
for ($i = 0; $i -lt $maxGroup; $i++) {
    foreach ($g in $grouped) {
        if ($i -lt $g.Count) { $shuffled += $g.Group[$i] }
    }
}

Write-Host ""
Write-Host "=== Running $($shuffled.Count) test cases, $Rounds rounds each, interleaved ==="
Write-Host ""

$results = @()

for ($idx = 0; $idx -lt $shuffled.Count; $idx++) {
    $t = $shuffled[$idx]
    $op = $t.op; $scale = $t.scale; $inner = $t.inner
    $label = "${op}_${scale}"
    Write-Host ("[{0,2}/{1}] {2,-15} scale={3,-8} inner={4}" -f ($idx+1), $shuffled.Count, $label, $scale, $inner) -NoNewline

    $origTimes = @()
    $optTimes = @()

    for ($r = 0; $r -lt $Rounds; $r++) {
        $out1 = & .\duel_orig.exe $inner $scale $op "x" 2>&1
        $ms1 = [double]($out1 -split ',')[1]
        $origTimes += $ms1

        $out2 = & .\duel_opt.exe $inner $scale $op "x" 2>&1
        $ms2 = [double]($out2 -split ',')[1]
        $optTimes += $ms2
    }

    $origSorted = $origTimes | Sort-Object
    $optSorted = $optTimes | Sort-Object
    $origMed = $origSorted[[math]::Floor($Rounds/2)]
    $optMed = $optSorted[[math]::Floor($Rounds/2)]

    $origPer = $origMed / $inner
    $optPer = $optMed / $inner
    $speedup = if ($optPer -gt 0) { $origPer / $optPer } else { 0 }

    $results += [PSCustomObject]@{
        Op = $op
        Scale = $scale
        Inner = $inner
        OrigMs = [math]::Round($origMed, 3)
        OptMs = [math]::Round($optMed, 3)
        OrigPer = [math]::Round($origPer, 4)
        OptPer = [math]::Round($optPer, 4)
        Speedup = [math]::Round($speedup, 3)
    }

    $winStr = if ($speedup -gt 1.02) { "OPT" }
              elseif ($speedup -lt 0.98) { "ORIG" }
              else { "TIE" }
    Write-Host ("  orig={0,8:0.000}ms opt={1,8:0.000}ms  {2:0.00}x  [{3}]" -f $origMed, $optMed, $speedup, $winStr)
}

# ==================== Generate MD report (English to avoid encoding issues) ====================
$mdPath = "speed_report.md"
$ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

# Build MD content as a single string with explicit newlines, then write with UTF8 encoding
$sb = New-Object System.Text.StringBuilder

function AddLine($text) {
    [void]$sb.AppendLine($text)
}

AddLine "# Original vs Optimized Speed Comparison Report"
AddLine ""
AddLine "**Generated**: $ts (Asia/Shanghai)"
AddLine ""
AddLine "## File Information"
AddLine ""
AddLine "| Version | File Name | Path |"
AddLine "|---------|-----------|------|"
AddLine "| Original | $origFile | d:\precious_speed\$origFile |"
AddLine "| Optimized | $optFile | d:\precious_speed\mason_opt\$optFile |"
AddLine ""
AddLine "## Test Environment"
AddLine ""
AddLine "- **Compiler**: g++ (MinGW-w64)"
AddLine "- **Compile options**: -O3 -mavx2 -mfma -funroll-loops"
AddLine "- **C++ standard**: default"
AddLine "- **Test program**: tle_duel.cpp"
AddLine "- **Rounds**: each case runs $Rounds rounds per version, median taken"
AddLine "- **Interleave strategy**: different modules alternate to reduce cache/temperature bias"
AddLine ""
AddLine "## Test Matrix"
AddLine ""
AddLine "Covers 13 operation modules x 4 scale tiers (1K/10K/100K/1M), $($shuffled.Count) test cases total."
AddLine ""
AddLine "| Module | Op | Description |"
AddLine "|--------|-----|-------------|"
AddLine "| Construction | ctor | Construct UnsignedInteger from string |"
AddLine "| I/O | io | ostringstream output + istringstream readback |"
AddLine "| To string | tostr | Convert to const char* |"
AddLine "| Conversion | conv | long long / double / string (3 conversions) |"
AddLine "| Comparison | cmp | 6 comparison operators |"
AddLine "| Addition | add | a + b |"
AddLine "| Subtraction | sub | aBig - bSmall |"
AddLine "| Multiplication | mul | a * b |"
AddLine "| Square | sqr | Optimized uses square() dedicated path, original uses a*a |"
AddLine "| Triple mul | mul2 | a^2 * a, 2 FFT multiplications |"
AddLine "| Division | div | aBig / bSmall |"
AddLine "| Newton div | div2 | a^2 / a, Newton iteration |"
AddLine "| Modulo | mod | aBig % bSmall |"
AddLine ""
AddLine "## Detailed Results"
AddLine ""
AddLine "| Op | Scale | Inner | Orig(ms) | Opt(ms) | Orig per(ms) | Opt per(ms) | Speedup | Winner |"
AddLine "|-----|-------|-------|----------|---------|--------------|-------------|---------|--------|"

foreach ($r in $results) {
    $win = if ($r.Speedup -gt 1.02) { "OPT" }
           elseif ($r.Speedup -lt 0.98) { "ORIG" }
           else { "TIE" }
    AddLine "| $($r.Op) | $($r.Scale) | $($r.Inner) | $($r.OrigMs) | $($r.OptMs) | $($r.OrigPer) | $($r.OptPer) | $($r.Speedup)x | $win |"
}

AddLine ""
AddLine "## Module Summary"
AddLine ""
AddLine "### Speedup > 1.02 (Optimized wins)"
AddLine ""
AddLine "| Op | Scale | Speedup |"
AddLine "|-----|-------|---------|"
$winCount = 0
foreach ($r in $results) {
    if ($r.Speedup -gt 1.02) {
        AddLine "| $($r.Op) | $($r.Scale) | $($r.Speedup)x |"
        $winCount++
    }
}
if ($winCount -eq 0) { AddLine "| (none) | - | - |" }

AddLine ""
AddLine "### Speedup < 0.98 (Original wins)"
AddLine ""
AddLine "| Op | Scale | Speedup |"
AddLine "|-----|-------|---------|"
$loseCount = 0
foreach ($r in $results) {
    if ($r.Speedup -lt 0.98) {
        AddLine "| $($r.Op) | $($r.Scale) | $($r.Speedup)x |"
        $loseCount++
    }
}
if ($loseCount -eq 0) { AddLine "| (none) | - | - |" }

AddLine ""
AddLine "### Tied (0.98 ~ 1.02)"
AddLine ""
AddLine "| Op | Scale | Speedup |"
AddLine "|-----|-------|---------|"
$tieCount = 0
foreach ($r in $results) {
    if ($r.Speedup -ge 0.98 -and $r.Speedup -le 1.02) {
        AddLine "| $($r.Op) | $($r.Scale) | $($r.Speedup)x |"
        $tieCount++
    }
}
if ($tieCount -eq 0) { AddLine "| (none) | - | - |" }

AddLine ""
AddLine "## Statistics"
AddLine ""
$avgSpeedup = ($results | Measure-Object Speedup -Average).Average
$maxSpeedup = ($results | Measure-Object Speedup -Maximum).Maximum
$minSpeedup = ($results | Measure-Object Speedup -Minimum).Minimum
AddLine "- **Total cases**: $($results.Count)"
AddLine "- **Optimized wins**: $winCount / $($results.Count)"
AddLine "- **Original wins**: $loseCount / $($results.Count)"
AddLine "- **Tied**: $tieCount / $($results.Count)"
AddLine "- **Average speedup**: $([math]::Round($avgSpeedup, 3))x"
AddLine "- **Max speedup**: $([math]::Round($maxSpeedup, 3))x"
AddLine "- **Min speedup**: $([math]::Round($minSpeedup, 3))x"
AddLine ""
AddLine "## Key Optimization Points"
AddLine ""
AddLine "- **square() dedicated path** (sqr module): Optimized uses frequencyDomainPointwiseSquare, saving one DIF + half pointwise work"
AddLine "- **Unbalanced multiplication split** (mul/div2 large scale): splits when length >= 2*other.length"
AddLine "- **DigitAllocator memory pool**: bucket-based free list, reduces new/delete"
AddLine "- **__m256d butterfly**: FFT butterfly processes 2 complex numbers at once"
AddLine "- **DIF(other) cache**: reuses other's DIF result in unbalanced multiplication"
AddLine ""

[System.IO.File]::WriteAllText("D:\precious_speed\mason_opt\$mdPath", $sb.ToString(), [System.Text.Encoding]::UTF8)
Write-Host ""
Write-Host "=== Report saved to $mdPath ==="
Write-Host "OPT wins: $winCount, ORIG wins: $loseCount, TIE: $tieCount"
Write-Host "Avg speedup: $([math]::Round($avgSpeedup, 3))x, Max: $([math]::Round($maxSpeedup, 3))x, Min: $([math]::Round($minSpeedup, 3))x"
