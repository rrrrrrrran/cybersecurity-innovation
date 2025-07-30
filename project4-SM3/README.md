# SM3 哈希算法 SIMD 优化实现

## 一、项目简介

本项目实现了基于 AVX2 SIMD 指令集的国密哈希算法 SM3 的优化版本。利用 SIMD 并行计算特性，对 SM3 算法中的消息扩展和压缩函数阶段进行向量化重构，显著提升了算法的运行效率。

---

## 二、优化报告

### 1. 算法背景

SM3 是中国国家密码管理局发布的密码杂凑算法，广泛应用于数字签名和消息认证中。其核心计算流程包括：

- 消息填充（padding）  
- 消息扩展（message expansion）  
- 压缩函数（compression function）  

---

### 2. SIMD 优化思路

#### 消息填充阶段

- 使用标量代码实现，保证数据格式正确  
- 将消息填充成 512 位的整数倍  
- 转换为大端格式的 `uint32_t` 数组，便于后续 SIMD 加载  

#### 消息扩展阶段

- 使用 AVX2 256位寄存器并行计算  
- 一次处理 8 个 `w[i]`，通过批量加载、异或、循环左移实现  
- 采用 SIMD 指令实现 P1 置换函数  
- 同时批量计算辅助数组 `w'`  
- 余数部分采用标量计算保证正确性  

#### 压缩函数阶段

- 使用 8 路 AVX2 寄存器模拟 8 条消息并行处理（单条消息模式下重复填充）  
- 每轮迭代使用 SIMD 指令批量执行布尔函数、循环左移和加法操作  
- 轮间数据依赖强，无法并行，只能提升单轮内部执行效率  
- 最终取 SIMD 寄存器中第 0 路数据更新哈希状态  

---

### 3. 优化效果总结

| 阶段       | 优化方法                     | 说明                         |
|------------|------------------------------|------------------------------|
| 消息填充   | 标量实现                     | 数据准备，格式转化           |
| 消息扩展   | AVX2批量8字并行计算          | 大幅减少循环次数和计算时间   |
| 压缩函数   | 8路AVX2寄存器并行状态变量计算 | SIMD指令减少指令数，提高吞吐 |

---

## 三、代码结构

- `messagePad()`：消息填充，转换为大端uint32_t数组  
- `messageExpand_SIMD()`：消息扩展，批量计算扩展字数组  
- `compressFunction_SIMD_single()`：压缩函数，使用SIMD寄存器并行处理64轮迭代  
- `SM3_SIMD()`：主函数，负责分块调用以上模块完成哈希计算  
- `main()`：测试函数，验证正确性并测试性能  

---

## 四、环境与编译说明

### 运行环境

- 支持 AVX2 指令集的 CPU  
- C++17 或以上标准编译器（GCC、Clang、MSVC）

### 编译命令示例

```bash
g++ -O3 -mavx2 -march=native -std=c++17 SM3.cpp -o sm3_simd
```

### 运行测试

```bash
./sm3_simd
```
### 程序输出：

```
SM3 SIMD Digest:
66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0
Average time per SM3_SIMD call (single message): 95.3 us
```

时间为8个消息的压缩实现。

# SM3 长度扩展攻击分析与实现

## 一、攻击原理概述

SM3 是我国设计的哈希函数，类似于 SHA-256。其工作原理是基于 **Merkle–Damgård 构造**：
 将消息进行填充，按块分组处理，每块输入压缩函数 `CF`，并将上一个块的输出作为下一个块的输入。

由于哈希值的计算方式是公开的，我们可以利用中间状态（压缩函数的输入）伪造新消息，使其哈希值与合法拼接后的哈希值一致——**这就是长度扩展攻击**。

> 即：已知 `hash(m)` 和 `len(m)`，构造 `m || pad(m) || extension`，得到与合法哈希结果相同的伪造 hash。

## 二、攻击流程

1. **受害者**发送消息 `m` 和 `H(m)`（例如 `H("abc")`）。

2. **攻击者**不知道 `m` 的内容，但知道 `len(m)` 和 `H(m)`。

3. 攻击者构造扩展消息 `extension`，如 `"def"`。

4. 伪造新消息为：`m || padding || extension`，并从 `H(m)` 继续哈希，得到新 hash。

5. 用新消息和新 hash 替换原始消息和 hash，伪装成合法数据。

## 三、核心函数

### 1.`sm3_hash(msg)`：标准 SM3 哈希计算

```python
def sm3_hash(msg):
    m = padding(msg)
    v = IV.copy()
    for i in range(0, len(m), 64):
        b = m[i:i+64]
        v = sm3_cf(v, b)
    return b''.join(x.to_bytes(4, 'big') for x in v)
```

- 对消息进行标准填充和分块。
- 每块调用 `sm3_cf` 进行压缩。
- 最终拼接压缩结果得到哈希值。

	### 2.`padding(msg, total_len=None)`：手动实现填充逻辑

```python
def padding(msg, total_len=None):
    m = bytearray(msg)
    if total_len is None:
        total_len = len(m)
    bit_len = total_len * 8
    m.append(0x80)
    while (len(m) + 8) % 64 != 0:
        m.append(0x00)
    m += struct.pack('>Q', bit_len)
    return m
```

- 添加 `0x80` 和若干 `0x00` 直至满足 `(len + 8) % 64 == 0`。
- 最后添加原始消息长度的 64 位大端表示。

### 3.`sm3_continue_hash(orig_hash_bytes, append_data, total_len)`

```python
def sm3_continue_hash(orig_hash_bytes, append_data, total_len):
    v = [int.from_bytes(orig_hash_bytes[i*4:(i+1)*4], 'big') for i in range(8)]
    new_data = padding(append_data, total_len + len(append_data))
    for i in range(0, len(new_data), 64):
        v = sm3_cf(v, new_data[i:i+64])
    return b''.join(x.to_bytes(4, 'big') for x in v)
```

- 将 `orig_hash` 视作 SM3 的中间状态 `v`。
- 对扩展部分重新填充并继续调用 `sm3_cf`。

------

### 4. `length_extension_attack(orig_hash, orig_len, extension)`

```python
def length_extension_attack(orig_hash, orig_len, extension):
    padding_bytes = padding(b'A'*orig_len)[orig_len:]  # 构造合法 padding
    new_hash = sm3_continue_hash(orig_hash, extension, orig_len + len(padding_bytes))
    return new_hash, padding_bytes
```

- 用任意占位符 `b'A'*orig_len` 模拟原始消息，获得 `padding`。
- 利用 `orig_hash` 继续压缩扩展内容 `extension`。

### 5.  攻击演示

```python
m = b"abc"
orig_len = len(m)
orig_hash = sm3_hash(m)
extension = b"def"

new_hash, padding1 = length_extension_attack(orig_hash, orig_len, extension)
forged_msg = m + padding1 + extension
verify_hash = sm3_hash(forged_msg)

print("原始 hash:     ", orig_hash.hex())
print("伪造 hash:     ", new_hash.hex())
print("验证用 hash:   ", verify_hash.hex())
assert new_hash == verify_hash
print("✅ 长度扩展攻击成功！")
```

##  四、实验结果

```bash
原始 hash:      66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0
伪造 hash:      db971139b8ccc58335a1e3702441daaf9fd32b42db157ce4bf745ad6fb95ac48
验证用 hash:    db971139b8ccc58335a1e3702441daaf9fd32b42db157ce4bf745ad6fb95ac48
✅ 长度扩展攻击成功！
```

- SM3 使用了 Merkle–Damgård 结构，因此容易受到长度扩展攻击。

- 如果应用场景中使用 SM3 进行认证（如 MAC）应使用 **HMAC** 而非 `hash(key || message)`。

- 该攻击无法伪造特定 hash，但能伪造合法扩展的数据拼接。
