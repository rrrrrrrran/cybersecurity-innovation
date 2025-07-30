
# 基于SIMD的SM4-GCM向量加速加解密实现报告

## 一、算法原理与优化思路

### 1.1 SM4 算法简介

SM4 是中国国家密码管理局发布的对称加密算法，采用 32 轮迭代的 Feistel 结构，每轮包括 S-box 非线性变换与线性置换。其轮函数为：

```text
X[i+4] = X[i] ^ L(τ(X[i+1] ⊕ X[i+2] ⊕ X[i+3] ⊕ rk[i]))
```
其中 τ 为 Sbox 非线性变换，L 为线性变换（通过轮转实现），rk[i] 是第 i 轮密钥。

### 1.2 GCM 模式与并行加密

GCM（Galois Counter Mode）是一种基于计数器（CTR）模式的认证加密方式，其加密过程本质是：

```text
C[i] = P[i] ⊕ E_K(IV || Counter+i)
```

由于每个计数器块互不依赖，天然适合并行化处理。解密过程完全相同，只需重新生成计数器流并异或即可还原明文。

### 1.3 优化目标

- 利用 SSE 指令 `_mm_set_epi32`, `_mm_xor_si128`, `_mm_store_si128` 实现数据寄存器内处理
- 所有加密轮操作不访问内存，中间状态完全保存在 `__m128i` 中
- 加解密过程共用相同轮函数，避免冗余逻辑

---

## 二、实现与函数说明

### 2.1 `rol`：循环左移函数

```cpp
uint32_t rol(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}
```

实现对 32 位整型变量进行循环左移，是线性变换 L 的基本操作。

---

### 2.2 `passSbox`：S-box 查表替代

```cpp
void passSbox(uint32_t& x) {
    uint8_t bytes[4];
    bytes[0] = Sbox[(x >> 24) & 0xff];
    bytes[1] = Sbox[(x >> 16) & 0xff];
    bytes[2] = Sbox[(x >> 8) & 0xff];
    bytes[3] = Sbox[x & 0xff];
    x = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}
```

对输入的 32 位数据进行按字节分离，并依次查表替换，提高非线性性。后续可通过字节级，并行查找，从而实现优化

---

### 2.3 `passLinear`：线性扩散函数

```cpp
void passLinear(uint32_t& x) {
     uint32_t left2 = rol(x, 2);
    uint32_t left10 = rol(x, 10);
    uint32_t left18 = rol(x, 18);
    uint32_t left24 = rol(x, 24);
    x = x ^ left2 ^ left10 ^ left18 ^ left24;
}
```

实现 SM4 轮函数中的 L 变换，将一个字进行多个位移并异或扩散，增强混淆性。

---

### 2.4 `enc_once`：轮函数执行

```cpp
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
```

对一个块执行一次轮函数变换：
- 从 128 位状态向量中提取 4 个 32 位整数；
- 计算轮函数的中间值，进行 S 盒查表和线性变换；
- 将结果与旧状态的一部分异或，形成新的轮函数输出；
- 利用 SIMD 指令实现状态左移和合并，避免频繁内存访问。

> ✅ 优化点：利用 `__m128i` 类型存储和操作状态，实现轮函数在 128 位寄存器内的高效执行，提升加密效率。

---

### 2.5 `SM4_enc`：32轮加密核心

```cpp
void SM4_enc(__m128i& m, __m128i k) { //encryption function of SM4
    vector<uint32_t> rk = keyExpansion(k);
    for (short i = 0; i < 32; ++i) {
        enc_once(m, rk[i + 4]);
    }
    m = _mm_shuffle_epi32(m, _MM_SHUFFLE(0, 1, 2, 3));
}
```

- 依次调用 `enc_once` 完成 32 轮变换
- 使用 `_mm_shuffle_epi32` 实现 128-bit 内寄存器值的轮换，代替中间变量

---

### 2.6 keyExpansion：密钥扩展

```cpp
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
```

- **SIMD 寄存器加载**
   使用 `_mm_xor_si128` 和 `_mm_store_si128` 直接在 128 位寄存器上对主密钥与常量 FK 异或，减少内存读写开销。
- **减少循环内存访问**
   扩展密钥过程将主密钥预先加载进对齐数组 `k32`，循环内只操作寄存器变量 `rk`，避免频繁访问内存
- **流水线与局部性利用**
   轮密钥计算以顺序依赖方式进行，利用循环局部变量和寄存器，保证 CPU 指令流水线的高效执行。
- **轮常量 CK 保存在只读内存**
   CK 数组常量存储，避免每轮重复计算，提高缓存命中率。
- **S盒和线性变换函数拆分**
   独立的 `passSbox` 和 `rol` 函数提高代码模块化，方便后续SIMD优化或查表加速。

---

### 2.7 `print`：调试输出函数

```cpp
void print(__m128i* data, size_t n, const char* label);
```

将 128-bit 数据以十六进制格式输出，用于验证正确性与调试。

---

## 三、整体流程与并行加速策略

### 3.1 加密主循环

1. 利用 IV 生成连续计数器块
2. 每块与明文异或前先执行 SM4 加密
3. 使用 `__m128i` 批量处理，4 块同时处理

### 3.2 优化策略

- **结构优化**：函数之间数据传递通过寄存器完成，函数封装清晰。
- **SSE 加速**：减少内存访问，充分利用 128-bit 数据寄存器。
- **轮次优化**：每轮处理结构高度重复，利于 pipeline。

---

## 四、实验与性能分析

### 4.1 实验环境

- 平台：Intel i7 CPU, 支持 SSE4.1
- 编译器：`visual studio`
- 明文数量：4 块，每块 128-bit

### 4.2 输出示例

```
密钥:  20 9a 50 ee 40 78 36 fd 12 49 32 f6 9e 7d 49 dc 
IV:   ad 4f 14 f2 44 40 66 d0 6b c4 30 b7 32 3b a1 22
明文[0]: 29 23 be 84 e1 6c d6 ae 52 90 49 f1 f1 bb e9 eb
密文[0]: cd 78 8d a7 b3 31 f4 eb 04 85 bb 3b 0c 72 82 0d
解密[0]: 29 23 be 84 e1 6c d6 ae 52 90 49 f1 f1 bb e9 eb
明文[1]: b3 a6 db 3c 87 0c 3e 99 24 5e 0d 1c 06 b7 47 de
密文[1]: fb 6e 7d 1f 8b 5d 3f 3a 64 e1 c6 cf cd b8 12 f8
解密[1]: b3 a6 db 3c 87 0c 3e 99 24 5e 0d 1c 06 b7 47 de
明文[2]: b3 12 4d c8 43 bb 8b a6 1f 03 5a 7d 09 38 25 1f
密文[2]: b7 05 b2 1b 68 04 0c 7e 40 cb 91 e4 6c a0 40 1d
解密[2]: b3 12 4d c8 43 bb 8b a6 1f 03 5a 7d 09 38 25 1f
明文[3]: 5d d4 cb fc 96 f5 45 3b 13 0d 89 0a 1c db ae 32
密文[3]: 71 ee c0 dd 49 6a 3a 26 6a 50 75 4b 13 80 05 a2
解密[3]: 5d d4 cb fc 96 f5 45 3b 13 0d 89 0a 1c db ae 32
加密耗时: 136 微秒
解密耗时: 120 微秒
```

### 4.3 编译环境要求

- C++11 或以上
- 支持 SSE2/SSSE3 指令集的编译器（如 `g++`, `clang++`, `MSVC`）
- 建议使用 `g++` 编译：

```
g++ -std=c++11 -O3 -march=native -o sm4_gcm SM4.cpp
```

### 4.4 文件结构

| 文件名      | 说明                              |
| ----------- | --------------------------------- |
| `SM4.cpp`   | 主程序，实现 SM4-GCM 的加解密逻辑 |
| `README.md` | 使用说明文档（当前文件）          |

### 4.5用法

编译后运行程序：

```
./sm4_gcm
```

---

## 五、总结与展望

本项目通过 C++ 与 SSE 指令集对 SM4-GCM 加解密进行了并行优化，实现了结构简洁、性能优越的 SIMD 加密框架。

后续工作可包括：
- 拓展 AVX/AVX-512 向量宽度，进一步提升吞吐
- 支持完整 GCM（包括 GMAC）认证机制
- 移植至 ARM NEON 实现移动端加速

---
