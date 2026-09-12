// radix_cmp.cpp -- split-radix vs radix-4 vs radix-8 DIF 指令数对照
// 目的: 用 perf instructions:u 实测三种 radix 的算术成本 (跨架构有效真值)
// 全部标量、同风格、同预计算 twiddle 表, 保证公平对比
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

static int NT;
static std::vector<double> Wr, Wi;

static void initW(int n)
{
    NT = n;
    Wr.assign(n, 0.0);
    Wi.assign(n, 0.0);
    for (int i = 0; i < n; ++i)
    {
        double a = -2.0 * M_PI * (double)i / (double)n;
        Wr[i] = std::cos(a);
        Wi[i] = std::sin(a);
    }
}

// ---------------- split-radix DIF (生产语义) ----------------
static void sr_dif(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    const int q = N / 4, h = N / 2;
    const double *wr = Wr.data(), *wi = Wi.data();
    for (int j = 0; j < q; ++j)
    {
        double *p0 = x + 2 * j;
        double *p1 = x + 2 * (j + q);
        double *p2 = x + 2 * (j + h);
        double *p3 = x + 2 * (j + h + q);
        double c0r = p0[0], c0i = p0[1];
        double c1r = p1[0], c1i = p1[1];
        double c2r = p2[0], c2i = p2[1];
        double c3r = p3[0], c3i = p3[1];
        double Ar = c0r - c2r, Ai = c0i - c2i;
        double Br = c1r - c3r, Bi = c1i - c3i;
        p0[0] = c0r + c2r; p0[1] = c0i + c2i;
        p1[0] = c1r + c3r; p1[1] = c1i + c3i;
        double u2r = Ar + Bi, u2i = Ai - Br;
        double u3r = Ar - Bi, u3i = Ai + Br;
        const int i1 = j * step, i3 = 3 * j * step;
        double w1r = wr[i1], w1i = wi[i1];
        double w3r = wr[i3], w3i = wi[i3];
        p2[0] = u2r * w1r - u2i * w1i; p2[1] = u2r * w1i + u2i * w1r;
        p3[0] = u3r * w3r - u3i * w3i; p3[1] = u3r * w3i + u3i * w3r;
    }
    sr_dif(x, h, step * 2);
    sr_dif(x + 2 * h, q, step * 4);
    sr_dif(x + 2 * (h + q), q, step * 4);
}

// ---------------- radix-4 DIF ----------------
static void r4_dif(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    const int q = N / 4;
    const double *wr = Wr.data(), *wi = Wi.data();
    for (int j = 0; j < q; ++j)
    {
        double *p0 = x + 2 * j;
        double *p1 = x + 2 * (j + q);
        double *p2 = x + 2 * (j + 2 * q);
        double *p3 = x + 2 * (j + 3 * q);
        double ar = p0[0], ai = p0[1];
        double br = p1[0], bi = p1[1];
        double cr = p2[0], ci = p2[1];
        double dr = p3[0], di = p3[1];
        double t0r = ar + cr, t0i = ai + ci;
        double t1r = ar - cr, t1i = ai - ci;
        double t2r = br + dr, t2i = bi + di;
        double t3r = br - dr, t3i = bi - di;
        double y0r = t0r + t2r, y0i = t0i + t2i;
        double y2r = t0r - t2r, y2i = t0i - t2i;
        double y1r = t1r + t3i, y1i = t1i - t3r;  // t1 - i*t3
        double y3r = t1r - t3i, y3i = t1i + t3r;  // t1 + i*t3
        const int i1 = j * step, i2 = 2 * j * step, i3 = 3 * j * step;
        double w1r = wr[i1], w1i = wi[i1];
        double w2r = wr[i2], w2i = wi[i2];
        double w3r = wr[i3], w3i = wi[i3];
        p0[0] = y0r; p0[1] = y0i;
        p1[0] = y1r * w1r - y1i * w1i; p1[1] = y1r * w1i + y1i * w1r;
        p2[0] = y2r * w2r - y2i * w2i; p2[1] = y2r * w2i + y2i * w2r;
        p3[0] = y3r * w3r - y3i * w3i; p3[1] = y3r * w3i + y3i * w3r;
    }
    r4_dif(x, q, step * 4);
    r4_dif(x + 2 * q, q, step * 4);
    r4_dif(x + 2 * 2 * q, q, step * 4);
    r4_dif(x + 2 * 3 * q, q, step * 4);
}

// ---------------- radix-8 DIF ----------------
static const double S8 = 0.70710678118654752440; // sqrt(2)/2

static void r8_dif(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    if (N == 4) { r4_dif(x, N, step); return; }
    const int q = N / 8;
    const double *wr = Wr.data(), *wi = Wi.data();
    for (int j = 0; j < q; ++j)
    {
        double *p[8];
        for (int r = 0; r < 8; ++r) p[r] = x + 2 * (j + r * q);
        double a0r = p[0][0], a0i = p[0][1];
        double a1r = p[1][0], a1i = p[1][1];
        double a2r = p[2][0], a2i = p[2][1];
        double a3r = p[3][0], a3i = p[3][1];
        double a4r = p[4][0], a4i = p[4][1];
        double a5r = p[5][0], a5i = p[5][1];
        double a6r = p[6][0], a6i = p[6][1];
        double a7r = p[7][0], a7i = p[7][1];
        // stage 1: b = a_m + a_{m+4}, c = (a_m - a_{m+4}) * W8^m
        double b0r = a0r + a4r, b0i = a0i + a4i;
        double b1r = a1r + a5r, b1i = a1i + a5i;
        double b2r = a2r + a6r, b2i = a2i + a6i;
        double b3r = a3r + a7r, b3i = a3i + a7i;
        double e0r = a0r - a4r, e0i = a0i - a4i;
        double e1r = a1r - a5r, e1i = a1i - a5i;
        double e2r = a2r - a6r, e2i = a2i - a6i;
        double e3r = a3r - a7r, e3i = a3i - a7i;
        // W8^0 = 1
        double c0r = e0r, c0i = e0i;
        // W8^1 = S8*(1 - i)
        double c1r = S8 * (e1r + e1i), c1i = S8 * (e1i - e1r);
        // W8^2 = -i
        double c2r = e2i, c2i = -e2r;
        // W8^3 = -S8*(1 + i)
        double c3r = S8 * (e3i - e3r), c3i = -S8 * (e3i + e3r);
        // DFT4(b) -> y0,y2,y4,y6 ; DFT4(c) -> y1,y3,y5,y7
        double s0r = b0r + b2r, s0i = b0i + b2i;
        double d0r = b0r - b2r, d0i = b0i - b2i;
        double s1r = b1r + b3r, s1i = b1i + b3i;
        double d1r = b1r - b3r, d1i = b1i - b3i;
        double y0r = s0r + s1r, y0i = s0i + s1i;
        double y4r = s0r - s1r, y4i = s0i - s1i;
        double y2r = d0r + d1i, y2i = d0i - d1r;
        double y6r = d0r - d1i, y6i = d0i + d1r;
        double t0r = c0r + c2r, t0i = c0i + c2i;
        double f0r = c0r - c2r, f0i = c0i - c2i;
        double t1r = c1r + c3r, t1i = c1i + c3i;
        double f1r = c1r - c3r, f1i = c1i - c3i;
        double y1r = t0r + t1r, y1i = t0i + t1i;
        double y5r = t0r - t1r, y5i = t0i - t1i;
        double y3r = f0r + f1i, y3i = f0i - f1r;
        double y7r = f0r - f1i, y7i = f0i + f1r;
        // twiddle: y_r *= W_N^{j*r}
        p[0][0] = y0r; p[0][1] = y0i;
        {
            const int i1 = j * step;
            double w = wr[i1], v = wi[i1];
            p[1][0] = y1r * w - y1i * v; p[1][1] = y1r * v + y1i * w;
        }
        {
            const int i2 = 2 * j * step;
            double w = wr[i2], v = wi[i2];
            p[2][0] = y2r * w - y2i * v; p[2][1] = y2r * v + y2i * w;
        }
        {
            const int i3 = 3 * j * step;
            double w = wr[i3], v = wi[i3];
            p[3][0] = y3r * w - y3i * v; p[3][1] = y3r * v + y3i * w;
        }
        {
            const int i4 = 4 * j * step;
            double w = wr[i4], v = wi[i4];
            p[4][0] = y4r * w - y4i * v; p[4][1] = y4r * v + y4i * w;
        }
        {
            const int i5 = 5 * j * step;
            double w = wr[i5], v = wi[i5];
            p[5][0] = y5r * w - y5i * v; p[5][1] = y5r * v + y5i * w;
        }
        {
            const int i6 = 6 * j * step;
            double w = wr[i6], v = wi[i6];
            p[6][0] = y6r * w - y6i * v; p[6][1] = y6r * v + y6i * w;
        }
        {
            const int i7 = 7 * j * step;
            double w = wr[i7], v = wi[i7];
            p[7][0] = y7r * w - y7i * v; p[7][1] = y7r * v + y7i * w;
        }
    }
    for (int r = 0; r < 8; ++r) r8_dif(x + 2 * r * q, q, step * 8);
}

// ---------------- radix-8 DIF, 输出序 == bit-reversed (drop-in 替换 split-radix) ----------------
// 关键: 把 y_r 写到子块 rev3(r) 而非 r。递归后位置数字串 = (rev3(m0),...,rev3(m5))
// 恰好 == bitrev(m)。零额外成本, 仅编译期重排 store 目标。
static const int REV3[8] = {0, 4, 2, 6, 1, 5, 3, 7};

// N==4 的 bitrev-序 radix-4 底座: y_r 写到子块 rev2(r), rev2 = {0,2,1,3}
static void r4_dif_br4(double *x, int step)
{
    const double *wr = Wr.data(), *wi = Wi.data();
    // q == 1, j == 0, 故 twiddle 全为 W^0 = 1
    (void)step; (void)wr; (void)wi;
    double ar = x[0], ai = x[1];
    double br = x[2], bi = x[3];
    double cr = x[4], ci = x[5];
    double dr = x[6], di = x[7];
    double t0r = ar + cr, t0i = ai + ci;
    double t1r = ar - cr, t1i = ai - ci;
    double t2r = br + dr, t2i = bi + di;
    double t3r = br - dr, t3i = bi - di;
    double y0r = t0r + t2r, y0i = t0i + t2i;
    double y2r = t0r - t2r, y2i = t0i - t2i;
    double y1r = t1r + t3i, y1i = t1i - t3r;
    double y3r = t1r - t3i, y3i = t1i + t3r;
    // rev2: 0->0, 1->2, 2->1, 3->3
    x[0] = y0r; x[1] = y0i;
    x[4] = y1r; x[5] = y1i;
    x[2] = y2r; x[3] = y2i;
    x[6] = y3r; x[7] = y3i;
}

static void r8_dif_br(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    if (N == 4) { r4_dif_br4(x, step); return; }
    const int q = N / 8;
    const double *wr = Wr.data(), *wi = Wi.data();
    const int q2 = 2 * q;
    double *base = x;
    for (int j = 0; j < q; ++j, base += 2)
    {
        // 输入按 r 顺序, 输出按 rev3(r) = {0,4,2,6,1,5,3,7} 顺序 -> 全部编译期常量偏移
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        double a0r = L0[0], a0i = L0[1];
        double a1r = L1[0], a1i = L1[1];
        double a2r = L2[0], a2i = L2[1];
        double a3r = L3[0], a3i = L3[1];
        double a4r = L4[0], a4i = L4[1];
        double a5r = L5[0], a5i = L5[1];
        double a6r = L6[0], a6i = L6[1];
        double a7r = L7[0], a7i = L7[1];
        double b0r = a0r + a4r, b0i = a0i + a4i;
        double b1r = a1r + a5r, b1i = a1i + a5i;
        double b2r = a2r + a6r, b2i = a2i + a6i;
        double b3r = a3r + a7r, b3i = a3i + a7i;
        double e0r = a0r - a4r, e0i = a0i - a4i;
        double e1r = a1r - a5r, e1i = a1i - a5i;
        double e2r = a2r - a6r, e2i = a2i - a6i;
        double e3r = a3r - a7r, e3i = a3i - a7i;
        double c0r = e0r, c0i = e0i;
        double c1r = S8 * (e1r + e1i), c1i = S8 * (e1i - e1r);
        double c2r = e2i, c2i = -e2r;
        double c3r = S8 * (e3i - e3r), c3i = -S8 * (e3i + e3r);
        double s0r = b0r + b2r, s0i = b0i + b2i;
        double d0r = b0r - b2r, d0i = b0i - b2i;
        double s1r = b1r + b3r, s1i = b1i + b3i;
        double d1r = b1r - b3r, d1i = b1i - b3i;
        double y0r = s0r + s1r, y0i = s0i + s1i;
        double y4r = s0r - s1r, y4i = s0i - s1i;
        double y2r = d0r + d1i, y2i = d0i - d1r;
        double y6r = d0r - d1i, y6i = d0i + d1r;
        double t0r = c0r + c2r, t0i = c0i + c2i;
        double f0r = c0r - c2r, f0i = c0i - c2i;
        double t1r = c1r + c3r, t1i = c1i + c3i;
        double f1r = c1r - c3r, f1i = c1i - c3i;
        double y1r = t0r + t1r, y1i = t0i + t1i;
        double y5r = t0r - t1r, y5i = t0i - t1i;
        double y3r = f0r + f1i, y3i = f0i - f1r;
        double y7r = f0r - f1i, y7i = f0i + f1r;
        // store: y_r -> 子块 rev3(r).  rev3 = {0,4,2,6,1,5,3,7}
        const int js = j * step;
        L0[0] = y0r; L0[1] = y0i;                            // y0 -> blk 0
        { double w = wr[js], v = wi[js];                     // y1 -> blk 4
          L4[0] = y1r * w - y1i * v; L4[1] = y1r * v + y1i * w; }
        { const int i = 2 * js; double w = wr[i], v = wi[i]; // y2 -> blk 2
          L2[0] = y2r * w - y2i * v; L2[1] = y2r * v + y2i * w; }
        { const int i = 3 * js; double w = wr[i], v = wi[i]; // y3 -> blk 6
          L6[0] = y3r * w - y3i * v; L6[1] = y3r * v + y3i * w; }
        { const int i = 4 * js; double w = wr[i], v = wi[i]; // y4 -> blk 1
          L1[0] = y4r * w - y4i * v; L1[1] = y4r * v + y4i * w; }
        { const int i = 5 * js; double w = wr[i], v = wi[i]; // y5 -> blk 5
          L5[0] = y5r * w - y5i * v; L5[1] = y5r * v + y5i * w; }
        { const int i = 6 * js; double w = wr[i], v = wi[i]; // y6 -> blk 3
          L3[0] = y6r * w - y6i * v; L3[1] = y6r * v + y6i * w; }
        { const int i = 7 * js; double w = wr[i], v = wi[i]; // y7 -> blk 7
          L7[0] = y7r * w - y7i * v; L7[1] = y7r * v + y7i * w; }
    }
    for (int r = 0; r < 8; ++r) r8_dif_br(x + 2 * r * q, q, step * 8);
}

// ---------------- radix-8 IDIT, 精确逆 (未归一化, 同生产 idit 约定) ----------------
static void r4_idit_br4(double *x)
{
    // r4_dif_br4 的共轭转置。输入位置序 = rev2, 输出 = 自然序
    double y0r = x[0], y0i = x[1];
    double y1r = x[4], y1i = x[5];
    double y2r = x[2], y2i = x[3];
    double y3r = x[6], y3i = x[7];
    double s0r = y0r + y2r, s0i = y0i + y2i;
    double d0r = y0r - y2r, d0i = y0i - y2i;
    double s1r = y1r + y3r, s1i = y1i + y3i;
    double d1r = y1r - y3r, d1i = y1i - y3i;
    x[0] = s0r + s1r; x[1] = s0i + s1i;
    x[4] = s0r - s1r; x[5] = s0i - s1i;
    x[2] = d0r - d1i; x[3] = d0i + d1r;
    x[6] = d0r + d1i; x[7] = d0i - d1r;
}

static void r8_idit_br(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    if (N == 4) { r4_idit_br4(x); return; }
    const int q = N / 8;
    for (int r = 0; r < 8; ++r) r8_idit_br(x + 2 * r * q, q, step * 8);
    const double *wr = Wr.data(), *wi = Wi.data();
    const int q2 = 2 * q;
    double *base = x;
    for (int j = 0; j < q; ++j, base += 2)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        const int js = j * step;
        // 逆序: 从 rev3(r) 位置 load, 乘 conj(W^{jr})
        double y0r = L0[0], y0i = L0[1];
        double y1r, y1i, y2r, y2i, y3r, y3i, y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i;
        { double a = L4[0], b = L4[1], w = wr[js], v = wi[js];
          y1r = a * w + b * v; y1i = b * w - a * v; }
        { const int i = 2 * js; double a = L2[0], b = L2[1], w = wr[i], v = wi[i];
          y2r = a * w + b * v; y2i = b * w - a * v; }
        { const int i = 3 * js; double a = L6[0], b = L6[1], w = wr[i], v = wi[i];
          y3r = a * w + b * v; y3i = b * w - a * v; }
        { const int i = 4 * js; double a = L1[0], b = L1[1], w = wr[i], v = wi[i];
          y4r = a * w + b * v; y4i = b * w - a * v; }
        { const int i = 5 * js; double a = L5[0], b = L5[1], w = wr[i], v = wi[i];
          y5r = a * w + b * v; y5i = b * w - a * v; }
        { const int i = 6 * js; double a = L3[0], b = L3[1], w = wr[i], v = wi[i];
          y6r = a * w + b * v; y6i = b * w - a * v; }
        { const int i = 7 * js; double a = L7[0], b = L7[1], w = wr[i], v = wi[i];
          y7r = a * w + b * v; y7i = b * w - a * v; }
        // IDFT4 (adjoint): z0=(Z0+Z2)+(Z1+Z3); z1=(Z0-Z2)+i(Z1-Z3);
        //                  z2=(Z0+Z2)-(Z1+Z3); z3=(Z0-Z2)-i(Z1-Z3)
        double s0r = y0r + y4r, s0i = y0i + y4i;   // Z0+Z2  (b: Z=y0,y2,y4,y6)
        double d0r = y0r - y4r, d0i = y0i - y4i;
        double s1r = y2r + y6r, s1i = y2i + y6i;
        double d1r = y2r - y6r, d1i = y2i - y6i;
        double b0r = s0r + s1r, b0i = s0i + s1i;
        double b2r = s0r - s1r, b2i = s0i - s1i;
        double b1r = d0r - d1i, b1i = d0i + d1r;
        double b3r = d0r + d1i, b3i = d0i - d1r;
        double t0r = y1r + y5r, t0i = y1i + y5i;   // c: Z=y1,y3,y5,y7
        double f0r = y1r - y5r, f0i = y1i - y5i;
        double t1r = y3r + y7r, t1i = y3i + y7i;
        double f1r = y3r - y7r, f1i = y3i - y7i;
        double c0r = t0r + t1r, c0i = t0i + t1i;
        double c2r = t0r - t1r, c2i = t0i - t1i;
        double c1r = f0r - f1i, c1i = f0i + f1r;
        double c3r = f0r + f1i, c3i = f0i - f1r;
        // e_m = c_m * conj(W8^m)
        double e0r = c0r, e0i = c0i;
        double e1r = S8 * (c1r - c1i), e1i = S8 * (c1i + c1r);      // *S8(1+i)
        double e2r = -c2i, e2i = c2r;                               // *i
        double e3r = -S8 * (c3r + c3i), e3i = S8 * (c3r - c3i);     // *(-S8)(1-i)
        // a_m = b_m + e_m ; a_{m+4} = b_m - e_m
        L0[0] = b0r + e0r; L0[1] = b0i + e0i;
        L4[0] = b0r - e0r; L4[1] = b0i - e0i;
        L1[0] = b1r + e1r; L1[1] = b1i + e1i;
        L5[0] = b1r - e1r; L5[1] = b1i - e1i;
        L2[0] = b2r + e2r; L2[1] = b2i + e2i;
        L6[0] = b2r - e2r; L6[1] = b2i - e2i;
        L3[0] = b3r + e3r; L3[1] = b3i + e3i;
        L7[0] = b3r - e3r; L7[1] = b3i - e3i;
    }
}

// ---------------- naive DFT (真值) ----------------
static void dft_naive(const double *x, double *X, int N)
{
    for (int k = 0; k < N; ++k)
    {
        double sr = 0, si = 0;
        for (int n = 0; n < N; ++n)
        {
            double a = -2.0 * M_PI * (double)((long long)k * n % N) / (double)N;
            double wr = std::cos(a), wi = std::sin(a);
            sr += x[2 * n] * wr - x[2 * n + 1] * wi;
            si += x[2 * n] * wi + x[2 * n + 1] * wr;
        }
        X[2 * k] = sr; X[2 * k + 1] = si;
    }
}

// 置换无关校验: 排序后的幅度谱对比
static double check_sorted_mag(const double *a, const double *b, int N)
{
    std::vector<double> ma(N), mb(N);
    for (int i = 0; i < N; ++i)
    {
        ma[i] = std::hypot(a[2 * i], a[2 * i + 1]);
        mb[i] = std::hypot(b[2 * i], b[2 * i + 1]);
    }
    std::sort(ma.begin(), ma.end());
    std::sort(mb.begin(), mb.end());
    double m = 0;
    for (int i = 0; i < N; ++i) m = std::max(m, std::fabs(ma[i] - mb[i]));
    return m;
}

int main(int argc, char **argv)
{
    // mode: 0=verify, 1=split-radix, 2=radix-4, 3=radix-8, 4=baseline(no fft)
    int mode = (argc > 1) ? atoi(argv[1]) : 0;
    int N = (argc > 2) ? atoi(argv[2]) : (1 << 18);
    int reps = (argc > 3) ? atoi(argv[3]) : 20;

    if (mode == 5)
    {
        // 决定性检验: r8_dif_br 输出序是否 == split-radix 输出序 (逐元素)
        // 若成立 -> radix-8 可 drop-in 替换生产 dif, rdot binrev 契约不变
        for (int n : {64, 512, 4096, 32768, 262144, 131072, 524288, 1048576})
        {
            initW(n);
            std::vector<double> x(2 * n), a(2 * n), b(2 * n), ref(2 * n);
            std::mt19937_64 g(777);
            std::uniform_real_distribution<double> d(-1, 1);
            for (int i = 0; i < 2 * n; ++i) x[i] = d(g);
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            sr_dif(a.data(), n, 1);
            memcpy(b.data(), x.data(), 2 * n * sizeof(double));
            r8_dif_br(b.data(), n, 1);
            double m = 0, scale = 0;
            for (int i = 0; i < 2 * n; ++i)
            {
                m = std::max(m, std::fabs(a[i] - b[i]));
                scale = std::max(scale, std::fabs(a[i]));
            }
            printf("N=%7d  |r8_br - split_radix|_max = %.3e  (scale %.3e)  %s\n",
                   n, m, scale, (m < 1e-9 * scale + 1e-12) ? "IDENTICAL-ORDER" : "*** DIFFERENT ***");
        }
        return 0;
    }

    if (mode == 6)
    {
        // 往返: r8_dif_br -> r8_idit_br 应得 N*x (F^H F = N I)
        for (int n : {8, 64, 512, 4096, 32768, 262144, 524288, 1048576})
        {
            initW(n);
            std::vector<double> x(2 * n), a(2 * n);
            std::mt19937_64 g(4242);
            std::uniform_real_distribution<double> d(-1, 1);
            for (int i = 0; i < 2 * n; ++i) x[i] = d(g);
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            r8_dif_br(a.data(), n, 1);
            r8_idit_br(a.data(), n, 1);
            double m = 0;
            for (int i = 0; i < 2 * n; ++i) m = std::max(m, std::fabs(a[i] / n - x[i]));
            printf("N=%7d  roundtrip |a/N - x|_max = %.3e  %s\n",
                   n, m, (m < 1e-10) ? "OK" : "*** BAD ***");
        }
        return 0;
    }

    if (mode == 7)
    {
        // idit 逆序也须与生产 iditSplit 语义一致: 检查 sr_dif 结果喂给 r8_idit_br
        for (int n : {64, 4096, 262144, 1048576})
        {
            initW(n);
            std::vector<double> x(2 * n), a(2 * n);
            std::mt19937_64 g(31337);
            std::uniform_real_distribution<double> d(-1, 1);
            for (int i = 0; i < 2 * n; ++i) x[i] = d(g);
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            sr_dif(a.data(), n, 1);
            r8_idit_br(a.data(), n, 1);
            double m = 0;
            for (int i = 0; i < 2 * n; ++i) m = std::max(m, std::fabs(a[i] / n - x[i]));
            printf("N=%7d  sr_dif->r8_idit |a/N - x|_max = %.3e  %s\n",
                   n, m, (m < 1e-10) ? "CROSS-OK" : "*** BAD ***");
        }
        return 0;
    }

    if (mode == 0)
    {
        for (int n : {64, 4096})
        {
            initW(n);
            std::vector<double> x(2 * n), a(2 * n), ref(2 * n);
            std::mt19937_64 g(12345);
            std::uniform_real_distribution<double> d(-1, 1);
            for (int i = 0; i < 2 * n; ++i) x[i] = d(g);
            dft_naive(x.data(), ref.data(), n);
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            sr_dif(a.data(), n, 1);
            printf("N=%6d split-radix sorted-mag err = %.3e\n", n, check_sorted_mag(a.data(), ref.data(), n));
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            r4_dif(a.data(), n, 1);
            printf("N=%6d radix-4     sorted-mag err = %.3e\n", n, check_sorted_mag(a.data(), ref.data(), n));
            memcpy(a.data(), x.data(), 2 * n * sizeof(double));
            r8_dif(a.data(), n, 1);
            printf("N=%6d radix-8     sorted-mag err = %.3e\n", n, check_sorted_mag(a.data(), ref.data(), n));
        }
        return 0;
    }

    initW(N);
    std::vector<double> x(2 * N), w(2 * N);
    std::mt19937_64 g(999);
    std::uniform_real_distribution<double> d(-1, 1);
    for (int i = 0; i < 2 * N; ++i) x[i] = d(g);
    double sink = 0;
    for (int r = 0; r < reps; ++r)
    {
        memcpy(w.data(), x.data(), 2 * N * sizeof(double));
        if (mode == 1) sr_dif(w.data(), N, 1);
        else if (mode == 2) r4_dif(w.data(), N, 1);
        else if (mode == 3) r8_dif(w.data(), N, 1);
        else if (mode == 4) r8_dif_br(w.data(), N, 1);
        sink += w[0] + w[2 * N - 1];
    }
    printf("mode=%d N=%d reps=%d sink=%.6e\n", mode, N, reps, sink);
    return 0;
}
