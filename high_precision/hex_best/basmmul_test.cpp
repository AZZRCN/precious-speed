#include <cstdio>
#include <cstdint>
#include <vector>
typedef uint32_t Limb;
typedef uint64_t Limb2;
static const int BASE = 65536;

static void basicMul(const std::vector<Limb>& in1v, const std::vector<Limb>& in2v, std::vector<Limb>& out)
{
    const Limb* in1 = in1v.data();
    const Limb* in2 = in2v.data();
    size_t n1 = in1v.size(), n2 = in2v.size();
    if (n1 > n2) { const Limb* t = in1; in1 = in2; in2 = t; size_t tn = n1; n1 = n2; n2 = tn; }
    std::vector<Limb> bp(n1 + n2, 0);
    for (size_t i = 0; i < n1; i++) {
        Limb2 carry = 0;
        Limb a = in1[i];
        for (size_t j = 0; j < n2; j++) {
            Limb2 cur = Limb2(bp[i + j]) + Limb2(a) * in2[j] + carry;
            bp[i + j] = Limb(cur % BASE);
            carry = cur / BASE;
        }
        bp[i + n2] = Limb(carry);
    }
    out = bp;
}

int main()
{
    std::vector<Limb> inv0 = {64826,61936,20655,41800,54951,23394,13640,25643,37187,10519,39507,3992,42297,60129,14855,8319,25033,44250,16732,31853,13285,2728,11807,4001,14355,61623,20155,45353,39856,27141,51889,29003,40636,5581,33375,719,6413,44612,52413,19216,23449,40126,51977,41784,14564,45642,36621,21571,39904};
    std::vector<Limb> m = {872,36133,46074,7529,52792,2109,31831,19455,43262,19296,25299,11634,10465,29410,20883,9936,48337,40818,18304,44030,45993,45856,44517,57018,23754,64935,60960,17451,17895,49551,10639,22108,17748,58449,20072,35473,5810,29021,26483,44543,24017,59782,54567,44233,13132,10222,19912,18468,23903,15912,11878,40163,29173,20677,53587,49927,28723,23089,6370,18170,42743,26855,54623,41978,3278,22204,32923,16065,50133,45809,44554,51125,12423,45900,49370,48398,17258,58036,42864,28880,38587,24161,38987,54460,42378,35705,40543,43654,45383,16399,63567,317,58128,10495,40313};
    std::vector<Limb> A, P;
    basicMul(inv0, inv0, A);
    printf("A[0]=%u (expect 45348)\n", (unsigned)A[0]);
    basicMul(A, m, P);
    printf("P[0]=%u (expect 25248)\n", (unsigned)P[0]);
    printf("exp0 m[0]*A[0] mod BASE = %u\n", (unsigned)((Limb2(m[0]) * A[0]) % BASE));
    return 0;
}
