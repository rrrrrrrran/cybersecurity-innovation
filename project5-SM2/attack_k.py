from ecdsa import SECP256k1, SigningKey
from hashlib import sha256
from random import randint

# === 曲线参数 ===
curve = SECP256k1
G = curve.generator
n = curve.order

# === 哈希函数 ===
def hash_msg(msg: bytes) -> int:
    return int(sha256(msg).hexdigest(), 16)

def sm2_sign(msg: bytes, d: int, k: int = None):
    e = hash_msg(msg)
    if k is None:
        k = randint(1, n - 1)
    R = k * G
    r = (e + R.x()) % n
    s = ((k - r * d) * pow(1 + d, -1, n)) % n
    return r, s, e, k

def recover_d_from_leak_k(r, s, k, n):
    return ((k - s) * pow(s + r, -1, n)) % n

# === 主函数模拟演示 ===
def demo():
    print("====== SM2 签名攻击演示 ======\n")
    # === 生成私钥 d ===
    d = randint(1, n - 1)
    print(f"[✔] 私钥 d: {d}")

    msg1 = b"hello world"
    r1, s1, e1, k1 = sm2_sign(msg1, d)
    d1 = recover_d_from_leak_k(r1, s1, k1, n)
    print("\n[🔓] 攻击 k 泄露")
    print(f"签名 (r, s) = ({r1}, {s1})")
    print(f"泄露的 k = {k1}")
    print(f"恢复的 d1 = {d1}")
    print(f"恢复成功？ {d1 == d}")

if __name__ == "__main__":
    demo()
