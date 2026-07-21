#!/usr/bin/env python3
# 验证 fftMulModBm1Pre 的正确性: 比较 cyclic 和线性乘法的结果
import paramiko, base64, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

TEST_PROG = r'''
#include <bits/stdc++.h>
#include "moptm_fusion.cpp"
using namespace std;
using Limb = uint16_t;
using Limb2 = uint32_t;
constexpr Limb BASE = 10000;

int main() {
    // 测试: 随机 a, b，比较 fftMulModBm1 和线性乘法 mod B^m-1 的结果
    srand(42);
    int m = 8192; // cyclic_m
    int a_len = 4097; // this_in + 1
    int b_len = 8000; // len2
    
    vector<Limb> a(a_len), b(b_len);
    for (int i = 0; i < a_len; i++) a[i] = rand() % BASE;
    for (int i = 0; i < b_len; i++) b[i] = rand() % BASE;
    a[a_len-1] = BASE/2 + rand() % (BASE/2); // 最高位 >= HALF_BASE
    b[b_len-1] = BASE/2 + rand() % (BASE/2);
    
    // 线性乘法
    vector<Limb> lin(a_len + b_len, 0);
    Integer::absMul(View(a.data(), a_len), View(b.data(), b_len), Span(lin.data(), lin.size()));
    
    // 计算线性乘法 mod (B^m - 1)
    auto mod_val = [&](const vector<Limb>& x, int m) -> vector<Limb> {
        vector<Limb> res(m, 0);
        // 每 m 位折叠一次
        for (int i = 0; i < (int)x.size(); i += m) {
            int chunk = min(m, (int)x.size() - i);
            Limb carry = 0;
            for (int j = 0; j < chunk; j++) {
                Limb2 s = Limb2(res[j]) + x[i+j] + carry;
                res[j] = s % BASE;
                carry = s / BASE;
            }
            // 传播进位
            for (int j = chunk; j < m && carry; j++) {
                Limb2 s = Limb2(res[j]) + carry;
                res[j] = s % BASE;
                carry = s / BASE;
            }
            // 如果还有 carry，再绕一圈（最多一次）
            if (carry) {
                Limb2 s = Limb2(res[0]) + carry;
                res[0] = s % BASE;
                carry = s / BASE;
                for (int j = 1; j < m && carry; j++) {
                    Limb2 s = Limb2(res[j]) + carry;
                    res[j] = s % BASE;
                    carry = s / BASE;
                }
            }
        }
        // 归约 B^m - 1 -> 0
        bool all_max = true;
        for (int j = 0; j < m; j++) {
            if (res[j] != BASE-1) { all_max = false; break; }
        }
        if (all_max) fill(res.begin(), res.end(), 0);
        return res;
    };
    
    vector<Limb> lin_mod = mod_val(lin, m);
    
    // cyclic 乘法
    vector<Limb> cyc(m, 0);
    vector<double> b_dft(m * 2, 0); // 不够大，用 Integer 的函数
    // 直接用 Integer::fftMulModBm1
    Integer::fftMulModBm1(View(a.data(), a_len), View(b.data(), b_len), m, Span(cyc.data(), m));
    
    // 比较
    int diff = 0;
    int first_diff = -1;
    for (int i = 0; i < m; i++) {
        if (lin_mod[i] != cyc[i]) {
            diff++;
            if (first_diff == -1) first_diff = i;
        }
    }
    printf("m=%d a_len=%d b_len=%d diff=%d first_diff=%d\n", m, a_len, b_len, diff, first_diff);
    if (diff > 0) {
        printf("  lin_mod[%d]=%d cyc[%d]=%d\n", first_diff, lin_mod[first_diff], first_diff, cyc[first_diff]);
        // 检查最高位
        int lin_top = m-1; while (lin_top > 0 && lin_mod[lin_top] == 0) lin_top--;
        int cyc_top = m-1; while (cyc_top > 0 && cyc[cyc_top] == 0) cyc_top--;
        printf("  lin_top=%d cyc_top=%d\n", lin_top, cyc_top);
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
stdin, stdout, stderr = c.exec_command(f"cat > /home/azzr/test_cyclic.cpp << 'EOF'\n{TEST_PROG}\nEOF")
print(f"[EXIT {stdout.channel.recv_exit_status()}]")

# 编译
cmd = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV test_cyclic.cpp -o test_cyclic -pthread"
print(f"\n=== Compile ===")
stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err[:1000]}")
if rc != 0:
    c.close()
    sys.exit(1)

# 运行
print(f"\n=== Run ===")
stdin, stdout, stderr = c.exec_command("./test_cyclic", timeout=120)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err[:1000]}")

c.close()
