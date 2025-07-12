#include <immintrin.h>
#include <iostream>
#include<bitset>
#include<vector>
using namespace std;
uint32_t rol(uint32_t x, int n) { // rotate left x by n bits 
    return (x << n) | (x >> (32 - n));
}
inline __m128i rol32(__m128i x, int n) {
    return _mm_or_si128(_mm_slli_epi32(x, n), _mm_srli_epi32(x, 32 - n));
}
__m128i P0(__m128i x) {
    return _mm_xor_si128(_mm_xor_si128(x, rol32(x, 9)), rol32(x, 17));
}
__m128i P1(__m128i x) {
    return _mm_xor_si128(_mm_xor_si128(x, rol32(x, 15)), rol32(x, 23));
}
void messagePad(vector<uint32_t>& m) {
    unsigned int len = m.size();
    uint32_t last = m[len - 1];
    bitset<32> bits(last);
    short i = 31;
    short x;
    for (; i >= 0; --i) {
        if (bits[i] == 1) {
            x = (i + 1) % 4;
            last = last << (27 - i+ x);
            uint32_t a = 1 << (26 - i + x);
            last = a ^ last;
            m[len - 1] = last;
            break;
        }
    }
    uint64_t bitlen = (len - 1) * 32 + i + 5 - x;
    uint32_t high = (uint32_t)(bitlen >> 32);     // 高 32 位
    uint32_t low = (uint32_t)(bitlen & 0xFFFFFFFF);
    unsigned short padlen = len % 16;
    if (padlen <= 13) {
        padlen = 14 - padlen;
        m.insert(m.end(), padlen, 0);
    }
    else if (padlen == 15) {
        m.insert(m.end(), padlen, 0);
    }
    else if (padlen == 16) {
        m.insert(m.end(), 14, 0);
    }
    m.push_back(high);
    m.push_back(low);
}
void messageExpand(vector<uint32_t>& w,vector<uint32_t>& w_) {
    for (short i = 0; i <= 16; ++i) {
        __m128i a = _mm_set_epi32(w[i * 3], w[i * 3 + 1], w[i * 3 + 2], w[i]);
        __m128i b = _mm_set_epi32(w[i * 3 + 7], w[i * 3 + 8], w[i * 3 + 9], w[i]);
        __m128i c = _mm_set_epi32(w[i * 3 + 13], w[i * 3 + 14], w[i * 3 + 15], w[i]);
        c = rol32(c, 15);
        a = _mm_xor_si128(a, b);
        a = _mm_xor_si128(a, c);
        a = P1(a);
        b = _mm_set_epi32(w[i * 3 + 3], w[i * 3 + 4], w[i * 3 + 5], w[i]);
        b = rol32(b, 7);
        c = _mm_set_epi32(w[i * 3 + 10], w[i * 3 + 11], w[i * 3 + 12], w[i]);
        a = _mm_xor_si128(a, b);
        a = _mm_xor_si128(a, c);
        alignas(16) uint32_t W32[4];
        _mm_store_si128((__m128i*)W32, a);
        w.push_back(W32[3]);
        w.push_back(W32[2]);
        w.push_back(W32[1]);
    }
    uint32_t aa = w[51] ^ w[58] ^ rol(w[64], 15);
    aa = aa ^ rol(aa, 15) ^ rol(aa, 23);
    w.push_back(aa ^ rol(w[54], 7) ^ w[61]);
    for (short i = 0; i < 8; ++i) {
        __m256i a = _mm256_loadu_si256((__m256i*)&w[8 * i]);
        __m256i b = _mm256_loadu_si256((__m256i*)&w[8 * i + 4]);
        a = _mm256_xor_si256(a, b);
        alignas(32) uint32_t W32[8];
        _mm256_store_si256((__m256i*)W32, a);
        w_.insert(w_.end(), W32, W32 + 8);
    }
}
/*__m256i SM3(vector<uint32_t> M) {
    messagePad(M);
    uint32_t len = M.size();
    len = len / 16;
    vector<vector<uint32_t>> mm;
    for (uint32_t i = 0; i < len; ++i) {
        vector<uint32_t> a(16);
        copy(M.begin() + 16 * i, M.begin() + 16 * i + 16, a.begin());
        mm.push_back(a);
    }
    
}*/
int main()
{   
    vector<uint32_t> M;
    M.push_back(0x616263);
    cout << hex;
    messagePad(M);
    for (auto it : M) {
        cout<<it<<" ";
    }
    cout << endl;
    cout << endl;
    vector<uint32_t> W(16);
    copy(M.begin(), M.end(), W.begin());
    vector<uint32_t> w_;
    messageExpand(W, w_);
    cout << hex;
    for (int i = 0; i <= 67; ++i) {
        cout << W[i] << ' ';
        if (i % 8 == 7)
            cout << endl;
    }
    cout << endl;
    for (int i = 0; i <= 63; ++i) {
        cout << w_[i] << ' ';
        if (i % 8 == 7)
            cout << endl;
    }
    return 0;
}
