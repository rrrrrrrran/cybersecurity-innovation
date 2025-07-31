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

# 基于 SM3 的 RFC 6962 Merkle 树实现及验证证明

---

## 一、背景与应用场景

Merkle 树是一种广泛应用于区块链、透明日志（如 Google CT）、分布式存储中的认证数据结构。RFC 6962 是 Google Certificate Transparency 项目的核心规范，定义了如何使用 Merkle Tree 对证书日志进行高效、可验证地追踪。

本项目使用国密 SM3 哈希算法替代 SHA-256，实现了一个支持：
- 构造大规模 Merkle 树（10 万节点）
- 构建单个叶子的存在性证明
- 构建一个伪造的不存在性证明并验证失败

---

## 二、RFC 6962 Merkle 树结构原理

RFC 6962 中，Merkle 树的构造逻辑如下：

- 所有叶子节点为原始数据的哈希值（叶哈希）。
- 内部节点为两个子节点哈希值的拼接再哈希。
- 若叶子为奇数个，则复制最后一个叶子补齐。

**数学定义：**

给定叶子节点集合
$$
L = [l_0, l_1, \ldots, l_{n-1}]
$$
定义：

- 叶节点哈希：
  $$
  H_0(i) = 	ext{Hash}(l_i)
  $$
  

- 内部节点哈希递归定义为：

$$
H(i, j) = 
\begin{cases}
H_0(i), & \text{if } j - i = 1 \\
\text{Hash}(H(i, m) \| H(m, j)), & \text{if } j - i > 1,\ m = \lfloor\frac{i+j}{2}\rfloor
\end{cases}
$$

---

## 三、SM3 哈希函数介绍

我们使用开源 `gmssl` 库中的 `sm3_hash` 函数。SM3 是国密标准哈希算法，与 SHA-256 类似，输出 256 位摘要。

```python
from gmssl import sm3, func

def sm3_hash(data: bytes) -> str:
    return sm3.sm3_hash(func.bytes_to_list(data))
```

**输入**：`bytes` 类型数据  
**输出**：`str` 类型十六进制哈希值

---

## 四、Merkle 树构建函数讲解

### hash_pair 函数：拼接两个哈希后再哈希

```python
def hash_pair(left: str, right: str) -> str:
    return sm3_hash(bytes.fromhex(left + right))
```

解释：
- `left` 和 `right` 是两个十六进制的 SM3 哈希值
- 使用 `bytes.fromhex` 拼接成字节串后重新哈希

---

### MerkleTree 构造函数

```python
class MerkleTree:
    def __init__(self, leaves: list[bytes]):
        self.leaves = [sm3_hash(leaf) for leaf in leaves]
        self.levels = [self.leaves]
        self.build_tree()
```

解释：
- 将所有叶子转为哈希值，初始化第一层
- 构造 Merkle 树的各层节点

---

### build_tree 函数：自底向上构建整个树

```python
    def build_tree(self):
        current = self.leaves
        while len(current) > 1:
            next_level = []
            for i in range(0, len(current), 2):
                left = current[i]
                right = current[i + 1] if i + 1 < len(current) else left
                next_level.append(hash_pair(left, right))
            self.levels.append(next_level)
            current = next_level
```

说明：
- 每轮构造上一层的父节点
- 若节点数为奇数，则复制最后一个节点

---

## 五、存在性证明与验证

### get_proof 函数：为指定叶子节点生成路径证明

```python
    def get_proof(self, index: int) -> list[tuple[str, str]]:
        proof = []
        for level in self.levels[:-1]:
            if index % 2 == 0:
                sibling_index = index + 1 if index + 1 < len(level) else index
                proof.append(('R', level[sibling_index]))
            else:
                sibling_index = index - 1
                proof.append(('L', level[sibling_index]))
            index //= 2
        return proof
```

- 每层记录兄弟节点的方向（L 或 R）和哈希值
- 用于后续的路径验证

---

### verify_proof 函数：重建路径，验证根哈希是否一致

```python
    @staticmethod
    def verify_proof(leaf: bytes, proof: list[tuple[str, str]], root: str) -> bool:
        current = sm3_hash(leaf)
        for direction, sibling in proof:
            if direction == 'L':
                current = hash_pair(sibling, current)
            else:
                current = hash_pair(current, sibling)
        return current == root
```

说明：
- 从叶节点开始，与兄弟节点拼接再哈希
- 最终哈希值是否等于 Merkle 根

---

## 六、不存在性证明与验证

RFC 6962 中不存在性证明依赖于排序和 Merkle Audit Path。这里我们模拟一种攻击场景：

### 伪造不存在的 leaf，使用合法路径验证失败

```python
    fake_leaf = b"not_in_tree_leaf"
    fake_proof = proof  # 使用存在的路径
    is_valid = MerkleTree.verify_proof(fake_leaf, fake_proof, root)
    print(f"验证不存在的叶子 '{fake_leaf.decode()}' 是否存在于树中: {is_valid}")
```

- 使用与某叶子相同的路径（攻击者构造）
- 替换为另一个数据进行验证应返回 `False`

---

## 七、完整测试与实验结果

```python
if __name__ == "__main__":
    leaf_count = 100000
    leaves = [f"leaf{i}".encode() for i in range(leaf_count)]
    tree = MerkleTree(leaves)
    root = tree.get_root()
    print(f"✅ 构建 Merkle 树成功，根为: {root}")

    index = random.randint(0, leaf_count - 1)
    proof = tree.get_proof(index)
    leaf = leaves[index]
    result = MerkleTree.verify_proof(leaf, proof, root)
    print(f"验证 leaf[{index}] = {leaf.decode()} 是否存在: {result}")

    fake_leaf = b"not_in_tree_leaf"
    fake_proof = proof
    is_valid = MerkleTree.verify_proof(fake_leaf, fake_proof, root)
    print(f"验证不存在的叶子 '{fake_leaf.decode()}' 是否存在于树中: {is_valid}")
```

示例输出：

```
✅ 构建 Merkle 树成功，根为: 15ddfec7212cbdc02390a700f82ea5e2ba8d7c71e062d3b2c556f05c80bceeac
验证 leaf[93396] = leaf93396 是否存在: True
验证不存在的叶子 'not_in_tree_leaf' 是否存在于树中: False
```

---

## ✅ 总结

| 项目        | 内容                                     |
|-------------|------------------------------------------|
| 哈希算法    | SM3，输出长度 256 位                    |
| Merkle 树深度 | 对于 100000 个叶子，约为 log₂(100000) ≈ 17 |
| 路径长度    | 存在性证明约为 17 步                     |
| 安全性      | 依赖哈希抗碰撞性，无法伪造有效路径       |
| 不存在性验证 | 模拟攻击时验证失败                       |

---
