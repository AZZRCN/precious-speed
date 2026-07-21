#!/usr/bin/env python3
# 调试 cyclic 路径: 比较 cyclic unwrap 和线性乘法的 product 差异
import paramiko, base64, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

TEST_PROG = r'''
#include <bits/stdc++.h>
#include "moptm_fusion.cpp"
using namespace std;

int main() {
    srand(123);
    
    int len2 = 1000;
    int this_in = 333;
    
    // 生成随机 divisor
    vector<Limb> divisor(len2, 0);
    for (int i = 0; i < len2; i++) divisor[i] = rand() % BASE;
    divisor[len2-1] = BASE/2 + rand() % (BASE/2);
    
    // 生成随机 qhat (this_in + 1 位)
    vector<Limb> qhat(this_in + 1, 0);
    for (int i = 0; i <= this_in; i++) qhat[i] = rand() % BASE;
    qhat[this_in] = rand() % BASE; // 可以为 0
    
    // 计算 cyclic_m
    int cyclic_m = 1;
    while (cyclic_m < len2 + 1) cyclic_m <<= 1;
    
    // 1. 线性乘法 (完整 product)
    int full_len = len2 + this_in + 1;
    vector<Limb> full_prod(full_len, 0);
    Integer::absMul(View(qhat.data(), this_in+1), View(divisor.data(), len2), Span(full_prod.data(), full_len));
    
    // 2. cyclic 乘法 + unwrap
    vector<double> dft_buf(cyclic_m * 2, 0.0);
    vector<Limb> cyc_prod(cyclic_m, 0);
    
    // 先计算 divisor 的 DFT
    Integer::prepareDFT(View(divisor.data(), len2), dft_buf.data(), cyclic_m);
    
    // cyclic 乘法
    Integer::fftMulModBm1Pre(View(qhat.data(), this_in+1), dft_buf.data(), len2, cyclic_m, Span(cyc_prod.data(), cyclic_m));
    
    // 打印 cyc_prod
    printf("cyc_prod (first 10):");
    for (int i = 0; i < 10; i++) printf(" %d", cyc_prod[i]);
    printf("\n");
    printf("full_prod (first 10):");
    for (int i = 0; i < 10; i++) printf(" %d", full_prod[i]);
    printf("\n");
    
    // 计算 full_prod mod (B^cyclic_m - 1)
    // 折叠: full_prod = sum_{i=0}^{k} full_prod[i*m : (i+1)*m]
    vector<Limb> folded(cyclic_m, 0);
    for (int i = 0; i < full_len; i += cyclic_m) {
        int chunk = min(cyclic_m, full_len - i);
        Limb carry = 0;
        for (int j = 0; j < chunk; j++) {
            uint32_t s = uint32_t(folded[j]) + full_prod[i+j] + carry;
            folded[j] = s % BASE;
            carry = s / BASE;
        }
        // 传播进位
        int j = chunk;
        while (carry > 0 && j < cyclic_m) {
            uint32_t s = uint32_t(folded[j]) + carry;
            folded[j] = s % BASE;
            carry = s / BASE;
            j++;
        }
        // 如果还有 carry，再绕一圈
        if (carry > 0) {
            j = 0;
            while (carry > 0 && j < cyclic_m) {
                uint32_t s = uint32_t(folded[j]) + carry;
                folded[j] = s % BASE;
                carry = s / BASE;
                j++;
            }
        }
    }
    // 归约 B^m - 1 -> 0
    bool all_max = true;
    for (int j = 0; j < cyclic_m; j++) {
        if (folded[j] != BASE-1) { all_max = false; break; }
    }
    if (all_max) fill(folded.begin(), folded.end(), 0);
    
    // 比较 cyc_prod 和 folded
    int diff = 0;
    int first_diff = -1;
    for (int i = 0; i < cyclic_m; i++) {
        if (cyc_prod[i] != folded[i]) {
            diff++;
            if (first_diff == -1) first_diff = i;
        }
    }
    printf("\ncyc vs folded: diff=%d positions, first_diff=%d\n", diff, first_diff);
    if (diff > 0) {
        printf("  cyc_prod[%d] = %d\n", first_diff, cyc_prod[first_diff]);
        printf("  folded[%d]   = %d\n", first_diff, folded[first_diff]);
        // 计算数值差
        // 只看最低位差
        Limb d = cyc_prod[first_diff] > folded[first_diff] ? 
                 cyc_prod[first_diff] - folded[first_diff] : 
                 folded[first_diff] - cyc_prod[first_diff];
        printf("  difference at pos %d: %d\n", first_diff, d);
    }
    
    return 0;
}
'''

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

# 写入测试文件
stdin, stdout, stderr = c.exec_command("cat > /home/azzr/debug_cyc.cpp << 'ENDOFFILE'\n" + TEST_PROG + "\nENDOFFILE")
print(f"[WRITE EXIT {stdout.channel.recv_exit_status()}]")

# 编译
cmd = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV debug_cyc.cpp -o debug_cyc -pthread"
print(f"\n=== Compile ===")
stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err[:2000]}")
if rc != 0:
    c.close()
    sys.exit(1)

# 运行
print(f"\n=== Run ===")
stdin, stdout, stderr = c.exec_command("./debug_cyc", timeout=120)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err[:1000]}")

c.close()
