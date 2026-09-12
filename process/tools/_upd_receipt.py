p = r"D:/precious_speed/hex_best/div.cpp"
s = open(p, encoding="utf-8").read()
i = s.index("Submission #392719")
j = s.index("*/", i) + 2
cases = [
 ("example_00","1 ms","2.79 Mib"),
 ("small_00","11 ms","11.52 Mib"),
 ("medium_00","7 ms","8.80 Mib"),
 ("medium_01","3 ms","6.79 Mib"),
 ("medium_02","2 ms","7.54 Mib"),
 ("large_00","2 ms","7.80 Mib"),
 ("large_01","2 ms","7.01 Mib"),
 ("max_00","4 ms","10.51 Mib"),
 ("max_01","3 ms","7.80 Mib"),
 ("max_02","3 ms","9.80 Mib"),
 ("a_max_b_random_00","21 ms","17.70 Mib"),
 ("a_max_b_random_01","32 ms","29.21 Mib"),
 ("a_max_b_random_02","34 ms","29.00 Mib"),
 ("power_00","3 ms","6.75 Mib"),
 ("r_nearly_zero_00","9 ms","7.29 Mib"),
 ("r_nearly_zero_01","2 ms","6.79 Mib"),
 ("r_nearly_zero_02","2 ms","7.29 Mib"),
 ("length_ratio_integer_00","39 ms","23.04 Mib"),
 ("length_ratio_integer_01","35 ms","21.77 Mib"),
 ("length_ratio_integer_02","36 ms","23.55 Mib"),
 ("length_ratio_integer_03","33 ms","24.01 Mib"),
 ("length_ratio_integer_04","31 ms","22.54 Mib"),
 ("length_ratio_integer_05","35 ms","23.54 Mib"),
 ("burnikel_ziegler_bound_00","10 ms","9.76 Mib"),
 ("burnikel_ziegler_bound_01","23 ms","20.51 Mib"),
 ("burnikel_ziegler_bound_02","6 ms","9.01 Mib"),
 ("burnikel_ziegler_bound_03","14 ms","19.21 Mib"),
]
lines = [
 "Submission #393027",
 "ID\tDate\tProblem\tLang\tUser\tStatus\tTime\tMemory",
 "393027\t2026/8/14 09:14:03\t",
 "",
 "Division of Hex Big Integers",
 "\tC++23\t(Anonymous)\tAC\t39 ms\t29.21 Mib",
 "Name\tStatus\tTime\tMemory",
]
for name, t, m in cases:
    lines.append(f"{name}\tAC\t{t}\t{m}")
lines.append("*/")
block = "\n".join(lines) + "\n"
s = s[:i] + block + s[j:]
open(p, "w", encoding="utf-8").write(s)
print("receipt updated -> #393027")
