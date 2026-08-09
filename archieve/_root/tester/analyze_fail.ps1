$names = @('div_div_medium_98_0','div_div_medium_386_1','div_div_medium_620_2')
foreach($name in $names){
    $inPath  = 'd:\precious_speed\tester\failures\' + $name + '.in'
    $refPath = 'd:\precious_speed\tester\failures\' + $name + '.ref.out'
    $curPath = 'd:\precious_speed\tester\failures\' + $name + '.cur.out'
    $inRaw  = [System.IO.File]::ReadAllText($inPath)
    $refRaw = [System.IO.File]::ReadAllText($refPath)
    $curRaw = [System.IO.File]::ReadAllText($curPath)
    $inLines  = $inRaw  -split "`r?`n"
    $refLines = $refRaw -split "`r?`n"
    $curLines = $curRaw -split "`r?`n"
    Write-Host ('=== ' + $name + ' ===')
    Write-Host ('inLines=' + $inLines.Count + ' refLines=' + $refLines.Count + ' curLines=' + $curLines.Count)
    Write-Host ('in[0] (T)=' + $inLines[0])
    Write-Host ('in[1] head=' + $inLines[1].Substring(0,[Math]::Min(80,$inLines[1].Length)))
    Write-Host ('ref[0] head=' + $refLines[0].Substring(0,[Math]::Min(80,$refLines[0].Length)))
    Write-Host ('ref[0] has space: ' + ($refLines[0].Contains(' ')))
    # Find diff
    $diffIdx=-1
    $max=[Math]::Min($refLines.Count,$curLines.Count)
    for($i=0;$i -lt $max;$i++){ if($refLines[$i] -ne $curLines[$i]){ $diffIdx=$i; break } }
    Write-Host ('First diff at line: ' + $diffIdx)
    if($diffIdx -ge 0){
        $r=$refLines[$diffIdx]; $c=$curLines[$diffIdx]
        Write-Host ('  REF.len=' + $r.Length + ' CUR.len=' + $c.Length)
        # check space position (separates q and r)
        $spaceIdx = $r.IndexOf(' ')
        Write-Host ('  REF space at: ' + $spaceIdx)
        $spaceIdx2 = $c.IndexOf(' ')
        Write-Host ('  CUR space at: ' + $spaceIdx2)
        # quotient and remainder
        if($spaceIdx -gt 0){
            $rq=$r.Substring(0,$spaceIdx); $rr=$r.Substring($spaceIdx+1)
            $cq=$c.Substring(0,$spaceIdx2); $cr=$c.Substring($spaceIdx2+1)
            Write-Host ('  Q.len REF=' + $rq.Length + ' CUR=' + $cq.Length)
            Write-Host ('  R.len REF=' + $rr.Length + ' CUR=' + $cr.Length)
            # common prefix of quotient
            $cp=0; $min=[Math]::Min($rq.Length,$cq.Length)
            for($i=0;$i -lt $min;$i++){ if($rq[$i] -ne $cq[$i]){break}; $cp++ }
            Write-Host ('  Quotient common prefix: ' + $cp + ' (divergence at digit idx ' + $cp + ')')
            Write-Host ('  Q tail REF: ...' + $rq.Substring([Math]::Max(0,$rq.Length-40)))
            Write-Host ('  Q tail CUR: ...' + $cq.Substring([Math]::Max(0,$cq.Length-40)))
            Write-Host ('  R REF: ' + $rr.Substring(0,[Math]::Min(80,$rr.Length)) + '(...)')
            Write-Host ('  R CUR: ' + $cr.Substring(0,[Math]::Min(80,$cr.Length)) + '(...)')
        }
        # The failing pair: if 1 line per pair, pairIdx=diffIdx; if 2 lines per pair, pairIdx=floor(diffIdx/2)
        $pairIdx1 = $diffIdx
        $pairIdx2 = [Math]::Floor($diffIdx / 2)
        Write-Host ('  If 1-line-per-pair: pairIdx=' + $pairIdx1)
        Write-Host ('  If 2-line-per-pair: pairIdx=' + $pairIdx2)
        foreach($pidx in @($pairIdx1, $pairIdx2)){
            if($pidx+1 -lt $inLines.Count){
                $p = $inLines[$pidx+1].Split(' ')
                if($p.Count -ge 2){
                    Write-Host ('    pairIdx=' + $pidx + ' -> A.len=' + $p[0].Length + ' B.len=' + $p[1].Length)
                }
            }
        }
    }
    Write-Host ''
}
