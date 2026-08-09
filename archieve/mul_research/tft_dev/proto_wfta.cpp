// WFTA (Winograd Fourier Transform Algorithm) prototype
// Tests whether WFTA 8/16-point DFT cores can beat standard radix-2 FFT.
// Theory: 8-point WFTA needs 4 real mults (all by const sqrt(2)/2),
//          standard 8-point FFT needs 8 real mults (2 complex twiddle mults).
// On modern CPUs mul is as fast as add, so WFTA's longer add chain may negate savings.
#define _USE_MATH_DEFINES
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <complex>
#include <vector>
#include <random>
#include <chrono>

using cd = std::complex<double>;
const double SQRT2_2 = 0.707106781186547524400844362104849; // sqrt(2)/2

// ============ Standard iterative radix-2 DIF DFT (baseline) ============
void std_fft_8(cd a[8]) {
    // DIF: stage 1 (butterfly 4-point, twiddle W8^0/W8^4=±1)
    for (int i = 0; i < 4; i++) {
        cd u = a[i], v = a[i + 4];
        a[i] = u + v; a[i + 4] = u - v;
    }
    // DIF: stage 2 (twiddle W8^0=1, W8^2=-i)
    for (int i = 0; i < 2; i++) {
        cd u = a[i], v = a[i + 2];
        a[i] = u + v; a[i + 2] = u - v;
        cd u2 = a[i + 4], v2 = a[i + 6];
        cd w2(0, -1); // W8^2 = -i
        a[i + 4] = u2 + v2;
        a[i + 6] = (u2 - v2) * w2;
    }
    // DIF: stage 3 (twiddle W8^0=1, W8^1, W8^2, W8^3)
    {
        cd u0 = a[0], u1 = a[1], u2 = a[2], u3 = a[3];
        a[0] = u0 + u1; a[1] = u0 - u1;
        a[2] = u2 + u3; a[3] = u2 - u3;
        cd w1(SQRT2_2, -SQRT2_2); // W8^1 = (1-i)/sqrt(2)
        cd w3(-SQRT2_2, -SQRT2_2); // W8^3 = (-1-i)/sqrt(2)
        cd v0 = a[4], v1 = a[5], v2 = a[6], v3 = a[7];
        a[4] = v0 + v1; a[5] = (v0 - v1) * w1;
        a[6] = v2 + v3; a[7] = (v2 - v3) * w3;
    }
    // Bit-reversal permutation (0,4,2,6,1,5,3,7) -> (0,1,2,3,4,5,6,7)
    // DIF outputs in bit-reversed order; unscramble.
    // bit-rev of 0..7: 0(000)->0, 1(001)->4, 2(010)->2, 3(011)->6, 4(100)->1, 5(101)->5, 6(110)->3, 7(111)->7
    // So output[br(k)] = input[k] => swap to get output[k] = input[br(k)]
    std::swap(a[1], a[4]);
    std::swap(a[3], a[6]);
}

// ============ WFTA 8-point DFT (minimal multiplication) ============
// Based on N=2*4 decomposition. 4-point DFT uses 0 non-trivial mults.
// Combination uses 4 real mults by constant sqrt(2)/2.
void wfta_8(const double in[8], cd out[8]) {
    // Split even/odd
    double e0 = in[0], e1 = in[2], e2 = in[4], e3 = in[6];
    double o0 = in[1], o1 = in[3], o2 = in[5], o3 = in[7];

    // 4-point DFT of even part: E[k] = sum(e[m] * W4^km)
    // W4 = {1, -i, -1, i}. All trivial (add/sub/neg/swap-real-imag).
    double e_s0 = e0 + e2, e_s1 = e0 - e2;       // for E[0]/E[2] and E[1]/E[3]
    double e_s2 = e1 + e3, e_s3 = e1 - e3;
    // E[0] = e_s0 + e_s2;  E[2] = e_s0 - e_s2
    // E[1] = e_s1 - i*e_s3 = (e_s1) + i*(-e_s3)
    // E[3] = e_s1 + i*e_s3 = (e_s1) + i*( e_s3)
    cd E0(e_s0 + e_s2, 0);
    cd E2(e_s0 - e_s2, 0);
    cd E1(e_s1, -e_s3);
    cd E3(e_s1,  e_s3);

    // 4-point DFT of odd part: O[k] = sum(o[m] * W4^km)
    double o_s0 = o0 + o2, o_s1 = o0 - o2;
    double o_s2 = o1 + o3, o_s3 = o1 - o3;
    cd O0(o_s0 + o_s2, 0);
    cd O2(o_s0 - o_s2, 0);
    cd O1(o_s1, -o_s3);
    cd O3(o_s1,  o_s3);

    // Combine: X[k] = E[k mod 4] + W8^k * O[k mod 4]
    // X[0] = E0 + O0
    // X[4] = E0 - O0
    out[0] = E0 + O0;
    out[4] = E0 - O0;
    // X[2] = E2 + W8^2*O2 = E2 + (-i)*O2
    // X[6] = E2 + W8^6*O2 = E2 + ( i)*O2
    // O2 is real (o_s0 - o_s2, 0). -i*O2 = (0, -O2.real)
    {
        double or2 = O2.real();
        out[2] = cd(E2.real() + 0, -or2);  // E2 real + (-i)*O2
        out[6] = cd(E2.real() + 0,  or2);
    }
    // X[1] = E1 + W8^1*O1,  X[5] = E1 + W8^5*O1
    // W8^5 = -W8^1 (since W8^5/W8^1 = W8^4 = -1)
    // So X[1]=E1+w, X[5]=E1-w  (butterfly), where w = W8^1*O1.
    // Let O1 = a+bi. s1=a+b, s2=b-a.
    // W8^1*O1 = √2/2*s1 + i*√2/2*s2  (2 real mults by const)
    {
        double a = O1.real(), b = O1.imag();
        double s1 = a + b, s2 = b - a;
        double m1 = SQRT2_2 * s1;  // mult 1
        double m2 = SQRT2_2 * s2;  // mult 2
        cd w1_O1(m1, m2);           // W8^1 * O1
        out[1] = E1 + w1_O1;
        out[5] = E1 - w1_O1;
    }
    // Let O3 = c+di. u1=c-d, u2=c+d.
    // W8^3*O3     = -√2/2*u1 - i*√2/2*u2
    // conj(W8^1)*O3 = √2/2*u1 + i*√2/2*u2
    {
        double c = O3.real(), d = O3.imag();
        double u1 = c - d, u2 = c + d;
        double m3 = SQRT2_2 * u1;  // mult 3
        double m4 = SQRT2_2 * u2;  // mult 4
        cd w3_O3(-m3, -m4);        // W8^3 * O3
        cd w7_O3( m3,  m4);        // conj(W8^1) * O3
        out[3] = E3 + w3_O3;
        out[7] = E3 + w7_O3;
    }
}

// ============ Standard 16-point radix-2 DIF DFT (baseline) ============
void std_fft_16(cd a[16]) {
    // stage 1
    for (int i = 0; i < 8; i++) { cd u=a[i], v=a[i+8]; a[i]=u+v; a[i+8]=u-v; }
    // stage 2
    for (int i = 0; i < 4; i++) {
        cd u=a[i], v=a[i+4]; a[i]=u+v; a[i+4]=u-v;
        cd u2=a[i+8], v2=a[i+12];
        cd w2(0,-1);
        a[i+8]=u2+v2; a[i+12]=(u2-v2)*w2;
    }
    // stage 3
    for (int i = 0; i < 2; i++) {
        for (int k = 0; k < 2; k++) {
            cd u = a[i*4+k], v = a[i*4+k+2];
            a[i*4+k] = u + v; a[i*4+k+2] = u - v;
        }
        for (int k = 0; k < 2; k++) {
            int base = i*4+k+8;
            cd u = a[base], v = a[base+2];
            double ang = -2.0 * M_PI * (k==0? (i==0?2:i==1?6:0):0) / 16;
            // simpler: W16^(2*(2*i+k))
            ang = -2.0 * M_PI * (2*(2*i+k)) / 16;
            cd w(std::cos(ang), std::sin(ang));
            a[base] = u + v; a[base+2] = (u - v) * w;
        }
    }
    // stage 4 (final, W16^k for k=0..7)
    for (int g = 0; g < 4; g++) {
        int base = g * 4;
        cd u0=a[base], u1=a[base+1], u2=a[base+2], u3=a[base+3];
        a[base]   = u0 + u1; a[base+1] = u0 - u1;
        a[base+2] = u2 + u3; a[base+3] = u2 - u3;
        int b2 = base + 8;
        cd v0=a[b2], v1=a[b2+1], v2=a[b2+2], v3=a[b2+3];
        double a1 = -2.0*M_PI*(4*g+0)/16, a2 = -2.0*M_PI*(4*g+1)/16;
        double a3 = -2.0*M_PI*(4*g+2)/16, a4 = -2.0*M_PI*(4*g+3)/16;
        cd w1(std::cos(a1),std::sin(a1)), w2(std::cos(a2),std::sin(a2));
        cd w3(std::cos(a3),std::sin(a3)), w4(std::cos(a4),std::sin(a4));
        a[b2]   = v0 + v1; a[b2+1] = (v0 - v1)*w2;
        a[b2+2] = v2 + v3; a[b2+3] = (v2 - v3)*w4;
    }
    // Bit-reversal for 16
    int br[16] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
    for (int i = 0; i < 16; i++) if (br[i] > i) std::swap(a[i], a[br[i]]);
}

// ============ WFTA 16-point (2x8 decomposition using WFTA-8) ============
// X[k] = E[k%8] + W16^k * O[k%8], where E,O are 8-point DFTs.
// W16^k for k=0..7: W16^0=1, W16^1=(cos22.5 - i sin22.5), W16^2=W8^1, ...
// We reuse wfta_8 for the 8-point sub-DFTs (but they take real input, here complex).
// For 16-point, input is real, so even/odd splits give real arrays -> wfta_8 works.
void wfta_16(const double in[16], cd out[16]) {
    double e[8] = {in[0], in[2], in[4], in[6], in[8], in[10], in[12], in[14]};
    double o[8] = {in[1], in[3], in[5], in[7], in[9], in[11], in[13], in[15]};
    cd E[8], O[8];
    wfta_8(e, E);
    wfta_8(o, O);
    // Combine with W16^k. For k=0..7: X[k]=E[k]+W16^k*O[k], X[k+8]=E[k]-W16^k*O[k]
    // (since W16^(k+8) = -W16^k)
    for (int k = 0; k < 8; k++) {
        double ang = -2.0 * M_PI * k / 16;
        cd w(std::cos(ang), std::sin(ang));
        cd wo = w * O[k];
        out[k]    = E[k] + wo;
        out[k+8]  = E[k] - wo;
    }
}

// ============ Reference DFT (brute force) ============
void ref_dft(const double* in, int N, cd* out) {
    for (int k = 0; k < N; k++) {
        cd sum(0, 0);
        for (int n = 0; n < N; n++) {
            double ang = -2.0 * M_PI * k * n / N;
            sum += in[n] * cd(std::cos(ang), std::sin(ang));
        }
        out[k] = sum;
    }
}

int main() {
    printf("=== WFTA Correctness Test ===\n");
    std::mt19937 rng(42);
    bool all_pass = true;

    // 8-point correctness
    for (int t = 0; t < 100; t++) {
        double in[8]; cd out_w[8], out_ref[8];
        for (int i = 0; i < 8; i++) in[i] = rng() % 10000;
        wfta_8(in, out_w);
        ref_dft(in, 8, out_ref);
        double err = 0;
        for (int i = 0; i < 8; i++) err = std::max(err, std::abs(out_w[i] - out_ref[i]));
        if (err > 1e-6) { printf("  8pt FAIL t=%d err=%.2e\n", t, err); all_pass = false; break; }
    }
    printf("  8-point: %s\n", all_pass ? "100 cases PASS" : "FAIL");

    // 16-point correctness
    bool pass16 = true;
    for (int t = 0; t < 100; t++) {
        double in[16]; cd out_w[16], out_ref[16];
        for (int i = 0; i < 16; i++) in[i] = rng() % 10000;
        wfta_16(in, out_w);
        ref_dft(in, 16, out_ref);
        double err = 0;
        for (int i = 0; i < 16; i++) err = std::max(err, std::abs(out_w[i] - out_ref[i]));
        if (err > 1e-6) { printf("  16pt FAIL t=%d err=%.2e\n", t, err); pass16 = false; break; }
    }
    printf("  16-point: %s\n", pass16 ? "100 cases PASS" : "FAIL");
    all_pass = all_pass && pass16;

    if (!all_pass) { printf("CORRECTNESS FAILED\n"); return 1; }

    // ============ Benchmark ============
    printf("\n=== WFTA vs Standard FFT Benchmark ===\n");
    printf("  %8s %8s %12s %12s %10s %8s\n", "N", "iters", "std_ms", "wfta_ms", "ratio", "status");
    fflush(stdout);

    // 8-point
    {
        const int ITERS = 2000000;
        double in[8]; cd out[8];
        volatile double sink = 0;
        for (int i = 0; i < 8; i++) in[i] = rng() % 10000;
        // warmup
        for (int i = 0; i < 1000; i++) { wfta_8(in, out); std_fft_8(out); }
        // standard
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) {
            for (int j = 0; j < 8; j++) out[j] = cd(in[j], 0);
            std_fft_8(out);
            sink += out[i & 7].real();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_std = std::chrono::duration<double, std::milli>(t1-t0).count();
        // wfta
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) {
            wfta_8(in, out);
            sink += out[i & 7].real();
        }
        t1 = std::chrono::high_resolution_clock::now();
        double t_wfta = std::chrono::duration<double, std::milli>(t1-t0).count();
        double ratio = t_wfta / t_std;
        printf("  %8d %8d %12.4f %12.4f %10.3f %8s  sink=%.0f\n",
               8, ITERS, t_std, t_wfta, ratio, ratio < 1.0 ? "*WFTA*" : "", (double)sink);
        fflush(stdout);
    }
    // 16-point
    {
        const int ITERS = 1000000;
        double in[16]; cd out[16];
        volatile double sink = 0;
        for (int i = 0; i < 16; i++) in[i] = rng() % 10000;
        for (int i = 0; i < 1000; i++) { wfta_16(in, out); std_fft_16(out); }
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) {
            for (int j = 0; j < 16; j++) out[j] = cd(in[j], 0);
            std_fft_16(out);
            sink += out[i & 15].real();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_std = std::chrono::duration<double, std::milli>(t1-t0).count();
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) {
            wfta_16(in, out);
            sink += out[i & 15].real();
        }
        t1 = std::chrono::high_resolution_clock::now();
        double t_wfta = std::chrono::duration<double, std::milli>(t1-t0).count();
        double ratio = t_wfta / t_std;
        printf("  %8d %8d %12.4f %12.4f %10.3f %8s  sink=%.0f\n",
               16, ITERS, t_std, t_wfta, ratio, ratio < 1.0 ? "*WFTA*" : "", (double)sink);
        fflush(stdout);
    }
    return 0;
}
