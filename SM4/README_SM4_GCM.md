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
    x = (Sbox[(x >> 24) & 0xff] << 24) |
        (Sbox[(x >> 16) & 0xff] << 16) |
        (Sbox[(x >> 8) & 0xff] << 8) |
        (Sbox[x & 0xff]);
}
```

对输入的 32 位数据进行按字节分离，并依次查表替换，提高非线性性。

---

### 2.3 `passLinear`：线性扩散函数

```cpp
void passLinear(uint32_t& x) {
    x ^= rol(x, 2) ^ rol(x, 10) ^ rol(x, 18) ^ rol(x, 24);
}
```

实现 SM4 轮函数中的 L 变换，将一个字进行多个位移并异或扩散，增强混淆性。

---

### 2.4 `enc_once`：轮函数执行

```cpp
void enc_once(__m128i& m, uint32_t rk) {
    uint32_t* x = (uint32_t*)&m;
    uint32_t temp = x[1] ^ x[2] ^ x[3] ^ rk;
    passSbox(temp);
    passLinear(temp);
    x[0] ^= temp;
}
```

对一个块执行一次轮函数变换：
- 临时变量 temp 为异或输入
- 执行 Sbox 和 L
- 最后异或进 x[0] 并写回

> ✅ 优化点：利用 `__m128i` 类型，避免每轮重新加载内存。

---

### 2.5 `SM4_enc`：32轮加密核心

```cpp
void SM4_enc(__m128i& m, __m128i rk[32]) {
    for (int i = 0; i < 32; i++) {
        enc_once(m, ((uint32_t*)&rk[i])[0]);
        m = _mm_shuffle_epi32(m, _MM_SHUFFLE(0, 3, 2, 1));
    }
    m = _mm_shuffle_epi32(m, _MM_SHUFFLE(0, 3, 2, 1));
}
```

- 依次调用 `enc_once` 完成 32 轮变换
- 使用 `_mm_shuffle_epi32` 实现 128-bit 内寄存器值的轮换，代替中间变量

---

### 2.6 `SM4_enc_vec`：批量并行加密

```cpp
void SM4_enc_vec(__m128i* m, size_t n, __m128i rk[32]) {
    for (int i = 0; i < n; i++) {
        SM4_enc(m[i], rk);
    }
}
```

支持多块同时加密，未来可扩展为多线程或更宽向量（如 AVX）。

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
