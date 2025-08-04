# 项目报告：Google Password Checkup 验证协议简化实现

---

## 1. 项目背景

Google Password Checkup 旨在保护用户隐私的前提下检测密码是否泄露。核心技术是**私密集合交集（Private Set Intersection，PSI）**，允许客户端检测其密码是否出现在服务端泄露密码数据库中，而不暴露任何其他信息。

本项目基于论文 [Private Set Membership from Algebraic PRFs (ePrint 2019/723)](https://eprint.iacr.org/2019/723.pdf) 中 Section 3.1（即 Figure 2）的协议思想，使用盲签名形式的 Oblivious Pseudorandom Function (OPRF) 与 Bloom Filter 结合，模拟密码泄露检测协议。

---

## 2. 协议描述与数学基础

### 2.1 协议角色

- **服务器 (Server)**  
  拥有泄露密码集合 \( D = \{d_1, d_2, \ldots, d_n\} \)，并持有盲签名私钥。

- **客户端 (Client)**  
  拥有待检测密码 \( p \)，想检测 \( p \in D \) 且不泄露 \( p \) 给服务器。

---

### 2.2 核心密码学工具

#### 2.2.1 盲签名（Blind Signature）

使用 RSA 盲签名实现 OPRF，步骤如下：

- Server 生成 RSA 公私钥对 \((N, e), d\)；
- Client 对密码哈希值 \( H = \mathrm{Hash}(p) \in \mathbb{Z}_N^* \) 盲化：

\[
M_{\text{blind}} = H \cdot r^e \pmod{N}
\]

其中 \( r \in \mathbb{Z}_N^* \) 是随机盲因子，且 \( \gcd(r, N) = 1 \)。

- Server 对盲化消息签名：

\[
S_{\text{blind}} = M_{\text{blind}}^d \pmod{N}
\]

- Client 去盲得到签名值：

\[
S = S_{\text{blind}} \cdot r^{-1} \pmod{N}
\]

签名值 \( S \) 是 OPRF 输出，具有如下性质：

\[
S = H^d \pmod{N}
\]

该值对 Server 私钥敏感，但 Server 不知道客户端输入 \( p \)。

---

#### 2.2.2 Bloom Filter

- Bloom Filter 是一种空间高效的概率型数据结构，用于集合元素测试（存在性判断），支持快速插入与查询；
- 插入时将元素通过多哈希函数映射到 bit 数组对应位置置 1；
- 查询时检查对应 bit 是否全为 1，若是则可能存在（存在误判），否则不存在。

---

### 2.3 协议流程

1. **初始化（服务器端）**

   - 对每个泄露密码 \( d \in D \)，计算其哈希值 \( H_d = \mathrm{Hash}(d) \)；
   - 服务器用私钥签名哈希值 \( S_d = H_d^d \pmod{N} \)；
   - 将签名值 \( S_d \)（以字符串形式）插入 Bloom Filter。

2. **客户端查询**

   - 客户端对密码 \( p \) 计算哈希值 \( H_p = \mathrm{Hash}(p) \)；
   - 生成随机盲因子 \( r \)，计算盲化消息 \( M_{\text{blind}} = H_p \cdot r^e \pmod{N} \)；
   - 请求服务器签名盲化消息，获得 \( S_{\text{blind}} \)；
   - 客户端去盲计算 \( S = S_{\text{blind}} \cdot r^{-1} \pmod{N} \)；
   - 客户端查询 Bloom Filter，判断 \( S \) 是否存在。

---

## 3. 算法实现思路

- **盲签名部分**  
  使用 `rsa` Python 库生成 RSA 密钥，模拟盲签签名。客户端计算盲化消息，服务器计算盲签名，客户端去盲获取最终 OPRF 输出。

- **Bloom Filter**  
  自定义 Bloom Filter 类，使用 SHA256 和 MD5 双哈希组合生成多个哈希函数，支持插入和查询。

- **协议模拟**  
  - 服务器端将泄露密码经哈希和盲签生成的 OPRF 值插入 Bloom Filter；
  - 客户端对查询密码执行盲签协议得到 OPRF 值，查询 Bloom Filter ；
  - 输出密码是否泄露。

---

## 4. 代码结构

- `BlindSignatureOPRF` 类：封装盲签名操作（盲化、签名、去盲）
- `BloomFilter` 类：实现 Bloom Filter 的插入与查询
- `simulate_protocol()` 函数：模拟服务器初始化和客户端查询流程

---

## 5. 代码示例

代码主要分为三个类和一个模拟函数，每个类负责协议中不同的模块功能：

------

### 5.1 BloomFilter 类

- 实现了概率型集合表示结构，支持插入和查询。
- 通过多哈希函数映射元素到位数组。

```python
import hashlib
import math

class BloomFilter:
    def __init__(self, n, p):
        self.size = int(-(n * math.log(p)) / (math.log(2) ** 2))
        self.hash_count = int((self.size / n) * math.log(2))
        self.bit_array = [0] * self.size

    def _hashes(self, item):
        hashes = []
        item_bytes = item.encode('utf-8')
        for i in range(self.hash_count):
            h1 = int(hashlib.sha256(item_bytes).hexdigest(), 16)
            h2 = int(hashlib.md5(item_bytes).hexdigest(), 16)
            combined = (h1 + i * h2) % self.size
            hashes.append(combined)
        return hashes

    def add(self, item):
        for pos in self._hashes(item):
            self.bit_array[pos] = 1

    def check(self, item):
        return all(self.bit_array[pos] == 1 for pos in self._hashes(item))
```

------

### 5.2 BlindSignatureOPRF 类

- 生成 RSA 密钥对。
- 对密码进行哈希、盲化、盲签、去盲操作，完成 OPRF 功能。

```python
import rsa
import hashlib
import random
import math

def int_from_bytes(b):
    return int.from_bytes(b, byteorder='big')

class BlindSignatureOPRF:
    def __init__(self, key_size=2048):
        self.pubkey, self.privkey = rsa.newkeys(key_size)
        self.N = self.pubkey.n
        self.e = self.pubkey.e
        self.d = self.privkey.d

    def hash_password(self, password):
        h = hashlib.sha256(password.encode('utf-8')).digest()
        return int_from_bytes(h)

    def blind(self, password):
        H = self.hash_password(password)
        while True:
            r = random.randrange(2, self.N - 1)
            if math.gcd(r, self.N) == 1:
                break
        M_blind = (H * pow(r, self.e, self.N)) % self.N
        self.r = r
        self.H = H
        return M_blind

    def sign_blind(self, M_blind):
        S_blind = pow(M_blind, self.d, self.N)
        return S_blind

    def unblind(self, S_blind):
        r_inv = pow(self.r, -1, self.N)
        S = (S_blind * r_inv) % self.N
        return S
```

------

### 5.3 模拟协议流程函数 simulate_protocol

- 服务器生成盲签密钥，构造 Bloom Filter 存储泄露密码的盲签名值。
- 客户端盲签密码查询，获得 OPRF 输出后查询 Bloom Filter 判断泄露。

```python
def int_to_hex_str(i):
    return hex(i)[2:]  # 去掉0x

def simulate_protocol():
    leaked_passwords = [
        "123456",
        "password",
        "qwerty",
        "abc123",
        "letmein",
        "dragon"
    ]
    n = len(leaked_passwords)
    p = 0.01  # 误判率

    oprf_server = BlindSignatureOPRF()
    bf = BloomFilter(n, p)

    print(f"Server public key N (hex): {hex(oprf_server.N)}")
    print(f"Server public key e: {oprf_server.e}\n")

    for pw in leaked_passwords:
        h = oprf_server.hash_password(pw)
        sig = pow(h, oprf_server.d, oprf_server.N)
        sig_hex = int_to_hex_str(sig)
        bf.add(sig_hex)
        print(f"Server inserts leaked password '{pw}' with OPRF sig :{sig_hex}\n")

    print("\n--- Client 查询阶段 ---")
    client_passwords = [
        "password",
        "securepass",
        "dragon",
        "hello123"
    ]

    for cpw in client_passwords:
        M_blind = oprf_server.blind(cpw)
        S_blind = oprf_server.sign_blind(M_blind)
        oprf_val = oprf_server.unblind(S_blind)
        oprf_val_hex = int_to_hex_str(oprf_val)
        leaked = bf.check(oprf_val_hex)
        print(f"Password '{cpw}' is {'LEAKED' if leaked else 'NOT leaked'}")

if __name__ == "__main__":
    simulate_protocol()
```

## 6. 运行结果示例
```python
Server public key N (hex): 0x8c2fde4002c41e02a57aaa831cb622e905fcab3af8edcbc8bf174830e38a6f0fe9078330a43a97d97cd75bc12ffe2b7b35f96dc4418b011a75a509612d2fcde8e1ee79e1b2bf43019ebf0c25a63d90c8d7107f717aee06d639d82b8c7b413f0e69afa843a032ed2f1c0dbdb9ef58c0cb5314ceb172cbb68dfb99b4b658be2f254a41976b97071e014ecb4533b0fc60f201d57e1eaedc9be62e1e46632cd5ac95f72230512364d86574f4f7888dd3b1e25e59170555c26131a1504c8ab0e2032838fc888dde27b7bb833dc5640cf8fdaa534877ea46ea026534b9e1e2342bb201f9163a31d474de9d17ac1b3cd0e06f03acea42dd47ce0e5fd14fbd77d13884c7
Server public key e: 65537

Server inserts leaked password '123456' with OPRF sig :11fdfc73be9146f3914937bc4af4d5f4a80014c884be6725b2ede942800ece1c0c707e06ba2b6d8d181918ad84f72d37151b5417e25bde0987e68aac05556081049bb72fb3dce68574117ec529e5f41027f7861aca6e7029255bd547a62c0260c233f3b5326bf093695fbd587504d402c9773a1bcaf043330a21e3ae1959fba9d10043d020c918aea4c2041d99cb5fd0a7dd8275ce1cf0195d804aa0168db3bfe8cd2dbb47cffef62a43437e50c518c6c7fdf2f7a86024ca8244d134beb90c9f822ee4d3304df1066b72a8201d18ce756dbfba468eedf122986ebaf2f9b2cd74d914ae5681ffa007372151a24c886edcb2f0ba600e5bdcd6a3236e9265c479bf

Server inserts leaked password 'password' with OPRF sig :334a589efee91c9d7b5548f0eaaed1527bc42442a185953b8fbff7a395af18ae3ab54ba6fcb0e297ad40969ad1886393fb2149333041c35c45b737f0f3c6cffa92a46bd0633a49429f9effd3f12b9068b1a4f86694951bf892d123bb4250d691dadc50786ef355db4d305b92e56028bfde87c5bda0be0cb598316a3ce851edfbb03a331bb172f31f52b37300d970fa73d9d0162bb8001ca765a6164ab4244417ebe9341089442ecedf4e7931d343c5abe1bb19333a4307966570f976355701f9c783800c2890c4bb8f58b56be2aeeb68a34773fd9863d21739cf99c6c053cac81dc8625b01e5645992a59aaad545481bd175068e4a4c4d75d9142e07887e822b

Server inserts leaked password 'qwerty' with OPRF sig :75383052a520ac45a3f30b441deebf57ea4100ce5dfeae9a93271a2f29b6987fca5d80b4a34bca48a73a01c83fc38cd2a60c8f639adcd0229130910e1289870a2810c14ba89308854b069565e5fb355df0804e630596c33d7cc9a417146b9aba8bbfa69b20aad8758ab8055547d736fd83236237df0520889ccd8f06116ce3a84b8f44f572764e3c1ba7d29c8325eec2d81ab8674976df51c833bb3e5539f581345ffc3f53cd0b856ed0b83250a9d1b62c058ad45da1acfd0979d931638728e3d5d61ad8af5794bd573b715dc2390cdfe559cd41926d3f2a710767a020e96a5544757239f69a0cf9a0cc3b896b0a751a31a711392cfa8bcfc72179d56bc9d341

Server inserts leaked password 'abc123' with OPRF sig :12cea7ede3f42e2a426f8562f5a07ab08e33c67e83d0162f3256e48b7b243f133bd30e34d7f3bbbaf2b82d58b63ad8a1cd45122a7449c2f14d99923efd3974f630e4057e65109e832c602117b4dc2102fa7d5bf818bb5d192f27ce286660e2393e59333111fd965977e82e0e77bd12a2cdd358abef63b182a7121aa1517b914fb601b9b3b14d67ef8785ce4306f2e13195de7de2fbfa5f99e101daca2e4798770f771400d61a816a55f9d5cfb16ab8168f25563e9f99b76312cca20d4625d2c4f88757298ad5695530b55df661bbd067a9e2354d45e67fea33bac32591aa65421a6f7ad2e55621168b38d330c64af7414585cf24611f849dca9e2af011113437

Server inserts leaked password 'letmein' with OPRF sig :39adb1f4e1ea25a5f8b436535e9574c334b6faf33dc3b7763f2011e71117c69df02b6bb0388b94fa1646ac27c1bf0b08deb7fb6d96b04526b89e8e33d7deb6a7ae5991a562f3ca7d6ca56f33031146b52a2d26b083864d4d2e6321159aecbae72ef373336b455b0837c8b7eaa4101a2dea6c70d02871071e1c549608ef737875d1c8bcfc6537ad858857f56a67cdecbfb6d790cf9819add088210da1cacd33d811c52dd3b3527e6679f5b2fc5fa729ba82ecc5d340482d74ab4425595887e84502ed8a99cbf3e87b2d8298ab1310b593d04390359ca1f743f002a5410211c04b358e41b434c556ddf2f10ca33772de94d636a8652d26294b534068d1f40524bd

Server inserts leaked password 'dragon' with OPRF sig :53e9f9405c22989457c493b01003d54d5e3f90398d135e2acacf1127f427f7943340cbf851d208d4fdccb10a6ecd7b190754cb12690eff297640564b47da7c741b48715f68fb5d9c0a03cf6b44df846abc4d37d0ed768ad16629364baa1b6dcd0fc9b0dabb4d66624155bb8f73ab3106f106347e2f039f2785a9167f7e107b9c912166367cfd3b887fe6c1bc30caae298df1c8afe4f56c53eb0eacbaef76ac40f08021e96d5e728e2b245c68d25b5ae945dcdcc15aeaf69bf93d0c62f06a4c3e2782cbecb7b7965600031f84d57e89aa1e19c08c1e4128f3a905eb5869e703e439483b8610cca5ff0f97674ff5af7c1e4f1b8555b5d5ec4e3226ecfa5282f4f6


--- Client 查询阶段 ---
Password 'password' is LEAKED
Password 'securepass' is NOT leaked
Password 'dragon' is LEAKED
Password 'hello123' is NOT leaked
```
## 7. 总结与展望
- 本项目用 Python 简单模拟了基于盲签名 OPRF 和 Bloom Filter 的密码泄露检测协议；

- 该协议有效保护用户密码隐私，服务器无法获知客户端查询的密码明文；

- 未来可扩展为网络交互版本，支持批量查询和更复杂的密码库；

- 进一步可采用椭圆曲线盲签名替代 RSA，提高性能与安全性。




