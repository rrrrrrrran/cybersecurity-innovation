import hashlib
import random

# SM2 椭圆曲线参数（推荐标准参数）
p = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF
a = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC
b = 0x28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93
n = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123
Gx = 0x32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7
Gy = 0xBC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0

# 椭圆曲线点加法
def point_add(P, Q):
    if P is None:
        return Q
    if Q is None:
        return P

    if P == Q:
        # 点 doubling
        lam_num = (3 * P[0] * P[0] + a) % p
        lam_den = pow(2 * P[1], p - 2, p)
    else:
        if P[0] == Q[0]:
            return None  # P+(-P) = O
        lam_num = (Q[1] - P[1]) % p
        lam_den = pow(Q[0] - P[0], p - 2, p)

    lam = (lam_num * lam_den) % p
    x_r = (lam * lam - P[0] - Q[0]) % p
    y_r = (lam * (P[0] - x_r) - P[1]) % p
    return (x_r, y_r)

# 椭圆曲线标量乘法（double and add）
def scalar_mult(k, P):
    R = None
    N = P
    while k > 0:
        if k & 1:
            R = point_add(R, N)
        N = point_add(N, N)
        k >>= 1
    return R

# 计算 ZA，带用户ID
def calc_ZA(IDA):
    ENTLA = len(IDA) * 8
    a_bytes = a.to_bytes(32, 'big')
    b_bytes = b.to_bytes(32, 'big')
    Gx_bytes = Gx.to_bytes(32, 'big')
    Gy_bytes = Gy.to_bytes(32, 'big')
    p_bytes = p.to_bytes(32, 'big')
    IDA_bytes = IDA

    data = (ENTLA.to_bytes(2, 'big') + IDA_bytes + a_bytes + b_bytes + Gx_bytes + Gy_bytes)
    return hashlib.sha256(data).digest()

# SM3标准用SHA256替代
def sm3_hash(data):
    return hashlib.sha256(data).digest()

# 生成密钥对
def generate_keypair():
    d = random.randrange(1, n)
    P = scalar_mult(d, (Gx, Gy))
    return d, P

# 签名
def sign(msg, d, IDA=b'1234567812345678'):
    ZA = calc_ZA(IDA)
    M_ = ZA + msg
    e = int.from_bytes(sm3_hash(M_), 'big')

    while True:
        k = random.randrange(1, n)
        x1, y1 = scalar_mult(k, (Gx, Gy))
        r = (e + x1) % n
        if r == 0 or r + k == n:
            continue
        s = (pow(1 + d, -1, n) * (k - r * d)) % n
        if s != 0:
            break
    return (r, s)

# 验签
def verify(msg, signature, P, IDA=b'1234567812345678'):
    r, s = signature
    if not (1 <= r <= n - 1) or not (1 <= s <= n - 1):
        return False
    ZA = calc_ZA(IDA)
    M_ = ZA + msg
    e = int.from_bytes(sm3_hash(M_), 'big')
    t = (r + s) % n
    if t == 0:
        return False
    x1, y1 = point_add(scalar_mult(s, (Gx, Gy)), scalar_mult(t, P))
    R = (e + x1) % n
    return R == r

# 测试示例
if __name__ == "__main__":
    msg = b"hello sm2 pure python"
    d, P = generate_keypair()
    print("私钥 d:", hex(d))
    print("公钥 P:", (hex(P[0]), hex(P[1])))

    signature = sign(msg, d)
    print("签名 r, s:", signature)

    valid = verify(msg, signature, P)
    print("验签结果:", valid)
