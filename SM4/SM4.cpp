#include <iostream>
#include <immintrin.h>
#include<vector>
#include <chrono>
#include <iomanip>
#include <random>
using namespace std;

void print_m128i_hex(__m128i var) { // print function of __m128i with little-endian byte order
    alignas(16) unsigned char bytes[16];
    _mm_store_si128((__m128i*)bytes, var); 
    std::cout << "0x";
    for (int i = 15; i >= 0; --i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    }
    std::cout << std::dec << std::endl;
}

const uint8_t Sbox[256] = { // Sbox of SM4
    0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
    0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
    0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
    0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
    0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
    0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
    0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
    0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
    0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
    0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
    0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
    0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
    0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
    0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
    0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
    0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
};

uint32_t rol(uint32_t x, int n) { // rotate left x by n bits 
    return (x << n) | (x >> (32 - n));
}

void passSbox(uint32_t& x) { // pass Sbox for x of 32 bits
    uint8_t bytes[4];
    bytes[0] = Sbox[(x >> 24) & 0xff];
    bytes[1] = Sbox[(x >> 16) & 0xff];
    bytes[2] = Sbox[(x >> 8) & 0xff];
    bytes[3] = Sbox[x & 0xff];
    x = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

void passLinear(uint32_t& x) { // pass linear layer for x of 32 bits 
    uint32_t left2 = rol(x, 2);
    uint32_t left10 = rol(x, 10);
    uint32_t left18 = rol(x, 18);
    uint32_t left24 = rol(x, 24);
    x = x ^ left2 ^ left10 ^ left18 ^ left24;
}
void enc_once(__m128i& m, uint32_t& k) { // perform one round of encryption on m using round key k
    alignas(16) uint32_t m32[4];
    _mm_store_si128((__m128i*)m32, m);
    uint32_t m4 = m32[1] ^ m32[2] ^ m32[0] ^ k;
    passSbox(m4);
    passLinear(m4);
    m4 ^= m32[3];
    __m128i m4_128 = _mm_cvtsi32_si128(m4);
    m = _mm_slli_si128(m, 4);
    m = _mm_xor_si128(m, m4_128);
}
vector<uint32_t> keyExpansion(__m128i k) { // keyExpansion function of SM4
    const __m128i FK = _mm_set_epi32(0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc);
    const uint32_t CK[32] = {
        0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
        0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
        0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
        0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
    };
    __m128i mk = _mm_xor_si128(k, FK);
    vector<uint32_t> rk(36);
    uint32_t  r;
    alignas(16) uint32_t k32[4];
    _mm_store_si128((__m128i*)k32, mk);
    for (short i = 0; i <= 3; ++i)
        rk[i] = k32[3 - i];
    for (short i = 0; i < 32; ++i) {
        r = rk[i + 1] ^ rk[i + 2] ^ rk[i + 3] ^ CK[i];
        passSbox(r);
        r = r ^ rol(r, 13) ^ rol(r, 23);
        rk[i+4] = (r ^ rk[i]);
    }
    return rk;
}
void SM4_enc(__m128i& m, __m128i k) { //encryption function of SM4
    vector<uint32_t> rk = keyExpansion(k);
    for (short i = 0; i < 32; ++i) {
        enc_once(m, rk[i + 4]);
    }
    m = _mm_shuffle_epi32(m, _MM_SHUFFLE(0, 1, 2, 3));
}
vector<uint32_t> keyExpansion_dec(__m128i k) {  
    const __m128i FK = _mm_set_epi32(0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc);
    const uint32_t CK[32] = {
        0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
        0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
        0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
        0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
    };
    __m128i mk = _mm_xor_si128(k, FK);
    vector<uint32_t> rk(36);
    uint32_t  r;
    alignas(16) uint32_t k32[4];
    _mm_store_si128((__m128i*)k32, mk);
    for (short i = 0; i <= 3; ++i)
        rk[i] = k32[3 - i];
    for (short i = 0; i < 32; ++i) {
        r = rk[i + 1] ^ rk[i + 2] ^ rk[i + 3] ^ CK[i];
        passSbox(r);
        r = r ^ rol(r, 13) ^ rol(r, 23);
        rk[i + 4] = (r ^ rk[i]);
    }
    reverse(rk.begin(), rk.end());
    return rk;
}
void SM4_dec(__m128i& c, __m128i k) { //decryption function of SM4
    vector<uint32_t> rk = keyExpansion_dec(k);
    for (short i = 0; i < 32; ++i) {
        enc_once(c, rk[i]);
    }
    c = _mm_shuffle_epi32(c, _MM_SHUFFLE(0, 1, 2, 3));
}
__m128i random_m128i() {
    static random_device rd;
    static mt19937 gen(rd()); 
    static uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    return _mm_set_epi32(dis(gen), dis(gen), dis(gen), dis(gen));
}
int main()
{
    __m128i m = random_m128i();
    __m128i k = random_m128i();
    __m128i c = m;
    auto start = std::chrono::high_resolution_clock::now();
    SM4_enc(c, k);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    __m128i dec_m = c;
    auto start1 = std::chrono::high_resolution_clock::now();
    SM4_dec(dec_m, k);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    cout << "明文：";
    print_m128i_hex(m);
    cout << "密钥：";
    print_m128i_hex(k);
    cout << "密文：";
    print_m128i_hex(c);
    cout << "加密耗时: " << duration.count() << " 微秒" << endl;
    cout << "解密：";
    print_m128i_hex(dec_m);
    cout << "解密耗时: " << duration.count() << " 微秒" << endl;
    return 0;
