$i = Get-Content 'd:\precious_speed\tester\failures\div_div_medium_98_0.in' -Raw
$ilines = $i -split "`n"
Write-Host "input lines: $($ilines.Count)"
for ($k = 0; $k -lt [Math]::Min(5, $ilines.Count); $k++) {
    Write-Host "line$k len: $($ilines[$k].Length) first40: $($ilines[$k].Substring(0, [Math]::Min(40, $ilines[$k].Length)))"
}
# Count test cases: each test case = 2 input lines (A, B)
Write-Host "test cases (input_lines/2): $($ilines.Count / 2)"
