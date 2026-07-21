#!/usr/bin/env python3
# 诊断 cyclic 路径: 在代码中加入调试，比较 unwrap product 和真实 product
import paramiko, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 先上传源码
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion_orig.cpp")
sftp.close()

# 读取源码，在 cyclic 路径插入调试代码
with open("d:/precious_speed/moptm_fusion.cpp", "r", encoding="utf-8") as f:
    code = f.read()

# 在 cyclic 修正逻辑前加入调试：同时计算完整 product 并比较
# 找到 "#ifndef DISABLE_2NXN_CYCLIC\n                // === cyclic 路径修正逻辑 ==="
marker = "#ifndef DISABLE_2NXN_CYCLIC\n                // === cyclic 路径修正逻辑 ==="
insert_code = '''
#ifndef DISABLE_2NXN_CYCLIC
                // DEBUG: 同时计算完整 product 用于比较
                thread_local std::vector<Limb> tprod_full;
                if (tprod_full.size() < prod_len) tprod_full.resize(prod_len);
                Span prod_full_span(tprod_full.data(), prod_len);
                fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_full_span);
                // 比较低 len2 位
                int diff_cnt = 0;
                int first_diff = -1;
                Limb max_diff = 0;
                for (size_t ii = 0; ii < len2; ii++) {
                    Limb d = tprod[ii] > tprod_full[ii] ? tprod[ii] - tprod_full[ii] : tprod_full[ii] - tprod[ii];
                    if (d > 0) {
                        diff_cnt++;
                        if (first_diff == -1) first_diff = ii;
                        if (d > max_diff) max_diff = d;
                    }
                }
                if (diff_cnt > 0 && diff_cnt < 50) {
                    printf("[DEBUG-CYC] block=%zu len2=%zu this_in=%zu cyclic_m=%d diff_limbs=%d first_diff=%d max_diff=%d\\n",
                        (qn - qn_remaining) / in, len2, this_in, cyclic_m, diff_cnt, first_diff, max_diff);
                    printf("  tprod[%d..%d]:", first_diff, std::min(first_diff+5, (int)len2-1));
                    for (int ii = first_diff; ii < std::min(first_diff+5, (int)len2); ii++)
                        printf(" %d", tprod[ii]);
                    printf("\\n");
                    printf("  tprod_full[%d..%d]:", first_diff, std::min(first_diff+5, (int)len2-1));
                    for (int ii = first_diff; ii < std::min(first_diff+5, (int)len2); ii++)
                        printf(" %d", tprod_full[ii]);
                    printf("\\n");
                }
                // DEBUG END
'''

# 找到正确的位置插入
old_str = "#ifndef DISABLE_2NXN_CYCLIC\n                // === cyclic 路径修正逻辑 ==="
new_str = insert_code + "\n                // === cyclic 路径修正逻辑 ==="

code = code.replace(old_str, new_str, 1)

# 写入调试版本
with open("d:/precious_speed/moptm_debug.cpp", "w", encoding="utf-8") as f:
    f.write(code)

# 上传
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_debug.cpp", "/home/azzr/moptm_debug.cpp")
sftp.close()
print("[OK] Uploaded debug version")

# 编译
cmd = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_debug.cpp -o moptm_debug -pthread 2>&1"
print("\n=== Compile debug version ===")
stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
rc = stdout.channel.recv_exit_status()
out = stdout.read().decode()
if out: print(out[:1000])
if rc != 0:
    print("COMPILE FAILED")
    c.close()
    sys.exit(1)
print("[OK] Compiled")

# 运行一个小测试
test_input = "1\n" + "9" * 10000 + "\n" + "1" + "0" * 4999 + "\n"
cmd = f"echo '{test_input}' | timeout 30 ./moptm_debug 2>&1 | head -100"
print("\n=== Run debug test ===")
stdin, stdout, stderr = c.exec_command(cmd, timeout=60)
print(stdout.read().decode()[:3000])
err = stderr.read().decode()
if err: print(f"[STDERR] {err[:500]}")

c.close()
