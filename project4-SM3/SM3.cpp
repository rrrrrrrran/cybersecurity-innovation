#include <immintrin.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace std;

// --- 工具函数 ---
inline uint32_t rol(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

inline __m256i rol32_256(__m256i x, int n) {
    return _mm256_or_si256(_mm256_slli_epi32(x, n), _mm256_srli_epi32(x, 32 - n));
}

inline __m256i P0_256(__m256i x) {
    return _mm256_xor_si256(_mm256_xor_si256(x, rol32_256(x, 9)), rol32_256(x, 17));
}

inline __m256i P1_256(__m256i x) {
    return _mm256_xor_si256(_mm256_xor_si256(x, rol32_256(x, 15)), rol32_256(x, 23));
}

uint32_t Tj_scalar(int j) {
    return j <= 15 ? 0x79cc4519 : 0x7a879d8a;
}

inline __m256i Tj_256(int j) {
    return _mm256_set1_epi32(rol(Tj_scalar(j), j));
}

// --- 消息填充 ---
void messagePad(const vector<uint8_t>& msg, vector<uint32_t>& M) {
    size_t bit_len = msg.size() * 8ULL;
    vector<uint8_t> padded(msg);

    // Append 0x80 (1 bit)
    padded.push_back(0x80);

    // Append zero bytes until length mod 64 == 56
    while (padded.size() % 64 != 56) {
        padded.push_back(0x00);
    }

    // Append 64-bit big-endian length
    for (int i = 7; i >= 0; --i) {
        padded.push_back((bit_len >> (8 * i)) & 0xFF);
    }

    // Convert to uint32_t big endian array
    size_t n = padded.size() / 4;
    M.resize(n);
    for (size_t i = 0; i < n; ++i) {
        M[i] = (padded[4 * i] << 24) | (padded[4 * i + 1] << 16) |
            (padded[4 * i + 2] << 8) | padded[4 * i + 3];
    }
}

void messageExpand_SIMD(const vector<uint32_t>& B, vector<uint32_t>& w, vector<uint32_t>& w_) {
    w.resize(68);
    w_.resize(64);
    for (int i = 0; i < 16; ++i) w[i] = B[i];

    int j = 16;
    for (; j + 7 < 68; j += 8) {
        __m256i v1 = _mm256_loadu_si256((__m256i*) & w[j - 16]);
        __m256i v2 = _mm256_loadu_si256((__m256i*) & w[j - 9]);
        __m256i v3 = _mm256_loadu_si256((__m256i*) & w[j - 3]);
        __m256i v4 = _mm256_loadu_si256((__m256i*) & w[j - 13]);
        __m256i v5 = _mm256_loadu_si256((__m256i*) & w[j - 6]);

        __m256i t = _mm256_xor_si256(v1, v2);
        t = _mm256_xor_si256(t, rol32_256(v3, 15));
        __m256i p1 = P1_256(t);
        __m256i r7 = rol32_256(v4, 7);
        __m256i res = _mm256_xor_si256(_mm256_xor_si256(p1, r7), v5);

        _mm256_storeu_si256((__m256i*) & w[j], res);
    }
    for (; j < 68; ++j) {
        uint32_t tmp = w[j - 16] ^ w[j - 9] ^ rol(w[j - 3], 15);
        w[j] = tmp ^ rol(tmp, 15) ^ rol(tmp, 23) ^ rol(w[j - 13], 7) ^ w[j - 6];
    }

    for (int i = 0; i < 64; i += 8) {
        __m256i a = _mm256_loadu_si256((__m256i*) & w[i]);
        __m256i b = _mm256_loadu_si256((__m256i*) & w[i + 4]);
        __m256i val = _mm256_xor_si256(a, b);
        _mm256_storeu_si256((__m256i*) & w_[i], val);
    }
}

// --- 压缩函数 SIMD单条消息实现 ---
// 因AVX2最小并行8条，这里用8路寄存器，8条消息数据均重复
// 可以扩展为真正的8条不同消息SIMD
void compressFunction_SIMD_single(
    const vector<uint32_t>& w,
    const vector<uint32_t>& w_,
    vector<uint32_t>& V  // IV和输出
) {
    // 初始化8路状态，全部复制同一条消息IV，方便SIMD模拟单条消息
    __m256i A = _mm256_set1_epi32(V[0]);
    __m256i B = _mm256_set1_epi32(V[1]);
    __m256i C = _mm256_set1_epi32(V[2]);
    __m256i D = _mm256_set1_epi32(V[3]);
    __m256i E = _mm256_set1_epi32(V[4]);
    __m256i F = _mm256_set1_epi32(V[5]);
    __m256i G = _mm256_set1_epi32(V[6]);
    __m256i H = _mm256_set1_epi32(V[7]);

    for (int j = 0; j < 64; ++j) {
        __m256i Wj = _mm256_set1_epi32(w[j]);
        __m256i W_j_ = _mm256_set1_epi32(w_[j]);
        __m256i Tj = Tj_256(j);

        __m256i A12 = rol32_256(A, 12);
        __m256i tmp = _mm256_add_epi32(_mm256_add_epi32(A12, E), Tj);
        __m256i SS1 = rol32_256(tmp, 7);
        __m256i SS2 = _mm256_xor_si256(SS1, A12);

        __m256i FF_val, GG_val;
        if (j <= 15) {
            FF_val = _mm256_xor_si256(_mm256_xor_si256(A, B), C);
            GG_val = _mm256_xor_si256(_mm256_xor_si256(E, F), G);
        }
        else {
            __m256i x_and_y = _mm256_and_si256(A, B);
            __m256i x_and_z = _mm256_and_si256(A, C);
            __m256i y_and_z = _mm256_and_si256(B, C);
            FF_val = _mm256_or_si256(_mm256_or_si256(x_and_y, x_and_z), y_and_z);

            __m256i x_and_y2 = _mm256_and_si256(E, F);
            __m256i not_E = _mm256_andnot_si256(E, _mm256_set1_epi32(-1));
            __m256i notE_and_G = _mm256_and_si256(not_E, G);
            GG_val = _mm256_or_si256(x_and_y2, notE_and_G);
        }

        __m256i TT1 = _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(FF_val, D), SS2), W_j_);
        __m256i TT2 = _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(GG_val, H), SS1), Wj);

        D = C;
        C = rol32_256(B, 9);
        B = A;
        A = TT1;

        H = G;
        G = rol32_256(F, 19);
        F = E;
        E = P0_256(TT2);
    }

    // 只取第0路输出
    alignas(32) uint32_t A_arr[8], B_arr[8], C_arr[8], D_arr[8], E_arr[8], F_arr[8], G_arr[8], H_arr[8];
    _mm256_store_si256((__m256i*)A_arr, A);
    _mm256_store_si256((__m256i*)B_arr, B);
    _mm256_store_si256((__m256i*)C_arr, C);
    _mm256_store_si256((__m256i*)D_arr, D);
    _mm256_store_si256((__m256i*)E_arr, E);
    _mm256_store_si256((__m256i*)F_arr, F);
    _mm256_store_si256((__m256i*)G_arr, G);
    _mm256_store_si256((__m256i*)H_arr, H);

    V[0] ^= A_arr[0];
    V[1] ^= B_arr[0];
    V[2] ^= C_arr[0];
    V[3] ^= D_arr[0];
    V[4] ^= E_arr[0];
    V[5] ^= F_arr[0];
    V[6] ^= G_arr[0];
    V[7] ^= H_arr[0];
}

// --- SM3初始IV ---
const uint32_t IV[8] = {
    0x7380166F,
    0x4914B2B9,
    0x172442D7,
    0xDA8A0600,
    0xA96F30BC,
    0x163138AA,
    0xE38DEE4D,
    0xB0FB0E4E
};

// --- 测试单条消息 SIMD版本 ---
vector<uint32_t> SM3_SIMD(const vector<uint8_t>& msg) {
    vector<uint32_t> M;
    messagePad(msg, M);

    size_t n = M.size() / 16;
    vector<uint32_t> V(8);
    memcpy(V.data(), IV, 32);

    for (size_t i = 0; i < n; ++i) {
        vector<uint32_t> block(M.begin() + i * 16, M.begin() + (i + 1) * 16);
        vector<uint32_t> w, w_;
        messageExpand_SIMD(block, w, w_);
        compressFunction_SIMD_single(w, w_, V);
    }
    return V;
}

// --- 计时辅助 ---
template<typename F, typename... Args>
double timeFunction(F func, Args&&... args) {
    auto start = chrono::high_resolution_clock::now();
    func(std::forward<Args>(args)...);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    return diff.count();
}

int main() {
    string test_msg = "The quick brown fox jumps over the lazy dog";

    vector<uint8_t> msg(test_msg.begin(), test_msg.end());

    // 先测试正确性
    auto digest = SM3_SIMD(msg);
    cout << "SM3 SIMD Digest:\n";
    for (auto v : digest) {
        cout << hex << setw(8) << setfill('0') << v;
    }
    cout << "\n";

    // 测试效率 多次调用平均耗时
    const int runs = 100;
    double total_time = 0;
    for (int i = 0; i < runs; ++i) {
        total_time += timeFunction(SM3_SIMD, msg);
    }
    cout << "Average time per SM3_SIMD call (single message): "
        << (total_time / runs) * 1e6 << " us\n";

    return 0;
}
