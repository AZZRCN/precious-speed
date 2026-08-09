$ErrorActionPreference = "Continue"
$LC_ROOT = "E:\library-checker-problems-master"
$COMMON = "$LC_ROOT\common"
$BI = "$LC_ROOT\big_integer"
$OUT = "d:\precious_speed\tester\gen"
New-Item -ItemType Directory -Force -Path $OUT | Out-Null

$tasks = @(
    @{name="add_carry_chain"; dir="addition_of_big_integers"; gen="carry_chain"},
    @{name="add_large";       dir="addition_of_big_integers"; gen="large"},
    @{name="add_large_small"; dir="addition_of_big_integers"; gen="large_small"},
    @{name="add_max_max";     dir="addition_of_big_integers"; gen="max_max"},
    @{name="add_medium";      dir="addition_of_big_integers"; gen="medium"},
    @{name="add_small";       dir="addition_of_big_integers"; gen="small"},
    @{name="add_sum_zero";    dir="addition_of_big_integers"; gen="sum_zero"},
    @{name="mul_fft_killer";  dir="multiplication_of_big_integers"; gen="fft_killer"},
    @{name="mul_large";       dir="multiplication_of_big_integers"; gen="large"},
    @{name="mul_large_small"; dir="multiplication_of_big_integers"; gen="large_small"},
    @{name="mul_max_max";     dir="multiplication_of_big_integers"; gen="max_max"},
    @{name="mul_medium";      dir="multiplication_of_big_integers"; gen="medium"},
    @{name="mul_small";       dir="multiplication_of_big_integers"; gen="small"},
    @{name="mul_zero";        dir="multiplication_of_big_integers"; gen="zero"},
    @{name="div_a_max_b_random";      dir="division_of_big_integers"; gen="a_max_b_random"},
    @{name="div_burnikel_ziegler";    dir="division_of_big_integers"; gen="burnikel_ziegler_bound"},
    @{name="div_large";               dir="division_of_big_integers"; gen="large"},
    @{name="div_length_ratio_integer";dir="division_of_big_integers"; gen="length_ratio_integer"},
    @{name="div_max";                 dir="division_of_big_integers"; gen="max"},
    @{name="div_medium";              dir="division_of_big_integers"; gen="medium"},
    @{name="div_r_nearly_zero";       dir="division_of_big_integers"; gen="r_nearly_zero"},
    @{name="div_small";               dir="division_of_big_integers"; gen="small"}
)

$ok = 0; $fail = 0
foreach ($t in $tasks) {
    $src = "$BI\$($t.dir)\gen\$($t.gen).cpp"
    $exe = "$OUT\$($t.name).exe"
    $args = @("-O2", "-std=c++23", "-I", $COMMON, "-I", "$BI\$($t.dir)", "-o", $exe, $src)
    & g++ @args 2>&1 | Out-Null
    if (Test-Path $exe) { Write-Host "[OK] $($t.name)"; $ok++ }
    else { Write-Host "[FAIL] $($t.name)"; $fail++ }
}
Write-Host "`n=== Done: $ok ok, $fail fail ==="
