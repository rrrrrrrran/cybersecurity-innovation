# SM2 椭圆曲线数字签名算法

---

## 一、椭圆曲线密码学简介

椭圆曲线密码学（ECC）基于定义在有限域上的椭圆曲线的数学结构。ECC算法核心运算是曲线上的点加法和标量乘法。

SM2 是中国国家密码算法标准，基于椭圆曲线密码学，包含公钥加密、签名和密钥交换等。本文主要介绍SM2的数字签名算法。

---

## 二、 SM2 标准曲线参数

SM2 使用的是定义在素数域 $ \mathbb{F}_p $ 上的椭圆曲线，参数如下：

| 参数 | 含义 | 值（十六进制） |
|-|-|-|
| $p$ | 素数域模数 | `FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF` |
| $a$ | 曲线系数 | `FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC` |
| $b$ | 曲线系数 | `28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93` |
| $n$ | 基点阶数 | `FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123` |
| $G=(G_x, G_y)$ | 基点坐标 | `32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7`,<br>`BC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0` |

---

## 三、 椭圆曲线点加法与标量乘法

### 3.1 点加法原理

给定椭圆曲线上的两点 $P = (x_1, y_1) $，$Q = (x_2, y_2) $，计算 $R = P + Q = (x_3, y_3) $：

$$
\lambda =
\begin{cases}
\frac{3x_1^2 + a}{2 y_1} \mod p, & P=Q \\
\frac{y_2 - y_1}{x_2 - x_1} \mod p, & P \neq Q
\end{cases}
$$

$$
x_3 = \lambda^2 - x_1 - x_2 \mod p
$$

$$
y_3 = \lambda(x_1 - x_3) - y_1 \mod p
$$

模逆计算使用费马小定理：

$$
d^{-1} \equiv d^{p-2} \mod p
$$

### 3.2 标量乘法（Double-and-Add）

通过二进制展开对点做加倍和相加，计算 $kP $：

- 初始化 $ R = O $（无穷远点）
- 遍历 $ k $ 的二进制位，遇1则 $ R = R + P $，每轮 $ P = 2P $
- 返回 $ R $

---

## 四、 SM2 密钥生成

- 私钥 $ d \in [1, n-1] $ 随机选取
- 公钥 $ P = dG $，通过标量乘法计算

---

## 五、 SM2 签名算法详解

### 5.1 计算ZA值（用户身份绑定）

计算 ZA：

$$
Z_A = H(\text{ENTLA} || ID_A || a || b || G_x || G_y)
$$

- $ID_A$ 为用户标识字节串，$ENTLA4 为其比特长度。
- $a,b,G_x,G_y$ 皆为大端字节表示。
- 哈希函数通常为 SM3（本例用SHA256替代）。

### 5.2 消息摘要计算

计算消息摘要：

$$
e = H(Z_A || M)
$$
其中 $M$ 是待签消息。

### 5.3 随机数 $ k $ 选取

随机选 $k \in [1, n-1]$。

### 5.4 计算签名值

计算点：

$$
(x_1, y_1) = kG
$$
计算

$$
r = (e + x_1) \mod n
$$
若 $r=0$ 或 $r+k = n$，重选 $k$。

计算

$$
s = ((1 + d)^{-1} (k - r d)) \mod n
$$
若 $s=0$，重选 $k$。

签名为 $(r, s)$。

---

## 六、 SM2 验签算法详解

给定签名 $(r, s)$ 和公钥 $P$，验证步骤：

- 检查 $r,s \in [1, n-1]$
- 计算消息摘要 $e = H(Z_A || M)$
- 计算

$$
t = (r + s) \mod n
$$

若 $t=0$，验签失败。

- 计算点：

$$
(x_1, y_1) = sG + tP
$$

- 计算

$$
R = (e + x_1) \mod n
$$

- 验签成功当且仅当 $R = r$。

---

## 七、代码讲解

### 7.1 椭圆曲线点加法 `point_add`

实现前述点加法的数学公式，包含点倍和两点相加。

### 7.2 标量乘法 `scalar_mult`

采用 double-and-add 法，计算大整数标量乘法。

### 7.3 ZA计算 `calc_ZA`

将用户ID及曲线参数编码后，计算哈希绑定。

### 7.4 签名 `sign`

实现签名流程：

- 计算 $Z_A$ 和摘要 $e$
- 随机选择 $k$，计算 $r,s$
- 重试直到满足条件
- 返回 $(r,s)$

### 7.5 验签 `verify`

- 验证输入范围
- 计算摘要 $e$
- 计算 $t$和点 $sG + tP$
- 比较 $R$和 $r$判断签名有效性

---

## 八、 测试示例

```python
if __name__ == "__main__":
    msg = b"hello sm2 pure python"
    d, P = generate_keypair()
    print("私钥 d:", hex(d))
    print("公钥 P:", (hex(P[0]), hex(P[1])))

    signature = sign(msg, d)
    print("签名 r, s:", signature)

    valid = verify(msg, signature, P)
    print("验签结果:", valid)
```

### 结果：

```bash
私钥 d: 0xa4222a66be7a9905799894d825b69616097f8be73d817243826702177ec5b5e8
公钥 P: ('0x3bd748928e01d50ae6c195f17aca677ef0e4c7b752b4927ebd37923e50dc362a', '0x2f66be80702fb89cf97b879637eb1e75037bf4f44f5588bf8b470f010ad3b857')
签名 r, s: (81322600772036967457437874088167261786937449823974641870044265916097329593313, 103063290200785085820996101658889702103727663073129966627344323394389473015726)
验签结果: True
```

## 九、总结

- SM2 通过定义在有限域的椭圆曲线提供高安全性数字签名。

- 点加法和标量乘法是SM2核心运算。

- ZA实现了身份绑定防止公钥伪造攻击。

- 签名和验签流程保证了消息完整性和身份认证。

# SM2 签名中随机数 \(k\) 泄露攻击详解

---

## 一、攻击背景与动机

在 SM2 签名算法中，每次签名都必须使用唯一且不可预测的随机数 $k$。  
如果随机数 $k$被泄露，攻击者即可根据签名信息计算出私钥 $d$，从而完全破坏签名安全性。

---

## 二、数学推导：如何利用 \(k\) 泄露恢复私钥 \(d\)

已知：

- 签名 $(r, s)$
- 消息哈希值 $e$
- 泄露的随机数 $k$

从签名公式：

$$
s \equiv (1 + d)^{-1} (k - r d) \pmod{n}
$$

两边乘以 \((1 + d)\)：

$$
s (1 + d) \equiv k - r d \pmod{n}
$$

展开：

$$
s + s d \equiv k - r d \pmod{n}
$$

整理 \(d\) 相关项：

$$
s d + r d \equiv k - s \pmod{n}
$$

$$
d (s + r) \equiv k - s \pmod{n}
$$

解得私钥 \(d\):

$$
\boxed{
d \equiv (k - s) \cdot (s + r)^{-1} \pmod{n}
}
$$

---

## 三、 代码实现

```python
from ecdsa import SECP256k1
from hashlib import sha256
from random import randint

# 曲线参数
curve = SECP256k1
G = curve.generator
n = curve.order

# 简单哈希函数，模拟 SM3
def hash_msg(msg: bytes) -> int:
    return int(sha256(msg).hexdigest(), 16)

# SM2签名
def sm2_sign(msg: bytes, d: int, k: int = None):
    e = hash_msg(msg)
    if k is None:
        k = randint(1, n - 1)
    R = k * G
    r = (e + R.x()) % n
    s = ((k - r * d) * pow(1 + d, -1, n)) % n
    return r, s, e, k

# 攻击函数：利用泄露的k恢复私钥d
def recover_d_from_leak_k(r, s, k, n):
    return ((k - s) * pow(s + r, -1, n)) % n

# 演示
def demo():
    print("====== SM2 k泄露攻击演示 ======\n")
    d = randint(1, n - 1)
    print(f"私钥 d: {d}")

    msg = b"hello world"
    r, s, e, k = sm2_sign(msg, d)
    print(f"\n签名 (r, s): ({r}, {s})")
    print(f"泄露的随机数 k: {k}")

    d_recovered = recover_d_from_leak_k(r, s, k, n)
    print(f"恢复的私钥 d_recovered: {d_recovered}")
    print(f"恢复成功？ {d_recovered == d}")

if __name__ == "__main__":
    demo()
```

## 四、总结

- SM2签名安全的关键是随机数 $k$ 的唯一和保密。

- 𝑘 一旦泄露，私钥 $ d $ 可通过简单的模逆计算被恢复。

- 实际应用中，应确保 𝑘 生成安全，且不被重复或泄露。
