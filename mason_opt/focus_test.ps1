# focus_test.ps1 - A/B test for 9 orig-win cases
$cases = @(
    @{op='ctor';  scale=100000;  inner=100},
    @{op='io';    scale=1000;    inner=500},
    @{op='io';    scale=10000;   inner=100},
    @{op='io';    scale=100000;  inner=20},
    @{op='io';    scale=1000000; inner=3},
    @{op='tostr'; scale=100000;  inner=100},
    @{op='tostr'; scale=1000000; inner=10},
    @{op='conv';  scale=1000;    inner=5000},
    @{op='conv';  scale=100000;  inner=200},
    @{op='conv';  scale=1000000; inner=20},
    @{op='cmp';   scale=100000;  inner=5000},
    @{op='sub';   scale=100000;  inner=1000}
)

$schedule = @()
for ($i=0; $i -lt 5; $i++) {
    foreach ($c in $cases) {
        $schedule += @{ver='orig'; op=$c.op; scale=$c.scale; inner=$c.inner}
        $schedule += @{ver='opt';  op=$c.op; scale=$c.scale; inner=$c.inner}
    }
}

$rng = New-Object System.Random(42)
for ($i=$schedule.Count-1; $i -gt 0; $i--) {
    $j = $rng.Next($i+1)
    $tmp = $schedule[$i]; $schedule[$i] = $schedule[$j]; $schedule[$j] = $tmp
}

$results = @{}
foreach ($s in $schedule) {
    $tag = "$($s.ver)_$($s.op)_$($s.scale)"
    $exe = if ($s.ver -eq 'orig') { '.\duel_orig.exe' } else { '.\duel_opt.exe' }
    $line = & $exe $s.inner $s.scale $s.op $tag 2>&1
    $parts = $line -split ','
    if ($parts.Count -ge 2) {
        $ms = [double]$parts[1]
        if (-not $results.ContainsKey($tag)) { $results[$tag] = @() }
        $results[$tag] += $ms
    }
}

Write-Host "Op|Scale|Orig_ms(med)|Opt_ms(med)|Speedup|Winner"
Write-Host "---|---|---|---|---|---"
$pairs = @(
    @('ctor','100000'),
    @('io','1000'),@('io','10000'),@('io','100000'),@('io','1000000'),
    @('tostr','100000'),@('tostr','1000000'),
    @('conv','1000'),@('conv','100000'),@('conv','1000000'),
    @('cmp','100000'),
    @('sub','100000')
)

foreach ($p in $pairs) {
    $op = $p[0]; $scale = $p[1]
    $oKey = "orig_${op}_${scale}"; $tKey = "opt_${op}_${scale}"
    if ($results.ContainsKey($oKey) -and $results.ContainsKey($tKey)) {
        $oArr = $results[$oKey] | Sort-Object
        $tArr = $results[$tKey] | Sort-Object
        $oMed = $oArr[[int]($oArr.Count/2)]
        $tMed = $tArr[[int]($tArr.Count/2)]
        $sp = [math]::Round($oMed/$tMed, 3)
        $win = if ($sp -gt 1.02) {'OPT'} elseif ($sp -lt 0.98) {'ORIG'} else {'TIE'}
        Write-Host "$op|$scale|$oMed|$tMed|${sp}x|$win"
    }
}
