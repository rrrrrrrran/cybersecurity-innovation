import rsa
import hashlib
import random
import math

class BloomFilter:
    def __init__(self, n, p):
        self.size = int(-(n * math.log(p)) / (math.log(2) ** 2))
        self.hash_count = int((self.size / n) * math.log(2))
        self.bit_array = [0] * self.size

    def _hashes(self, item):
        # item 是字符串
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

def int_from_bytes(b):
    return int.from_bytes(b, byteorder='big')

def bytes_from_int(i, length):
    return i.to_bytes(length, byteorder='big')

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
            if math.gcd(r, self.N) == 1:  # 用 math.gcd 替代 rsa.common.gcd
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

def int_to_hex_str(i):
    return hex(i)[2:]  # 去掉0x

def simulate_protocol():
    # --- Server 初始化 ---
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

    # 服务器生成盲签密钥对
    oprf_server = BlindSignatureOPRF()

    # 服务器用盲签计算泄露密码的 OPRF 值，插入 Bloom Filter
    bf = BloomFilter(n, p)

    print(f"Server public key N (hex): {hex(oprf_server.N)}")
    print(f"Server public key e: {oprf_server.e}\n")

    for pw in leaked_passwords:
        # 服务器直接计算：签名 hash(pw) = pow(hash(pw), d, N)
        h = oprf_server.hash_password(pw)
        sig = pow(h, oprf_server.d, oprf_server.N)
        sig_hex = int_to_hex_str(sig)
        bf.add(sig_hex)
        print(f"Server inserts leaked password '{pw}' with OPRF sig :{sig_hex}\n")

    print("\n--- Client 查询阶段 ---")
    client_passwords = [
        "password",   # 泄露密码，应该命中
        "securepass", # 安全密码，不命中
        "dragon",     # 泄露密码，命中
        "hello123"    # 安全密码，不命中
    ]

    for cpw in client_passwords:
        # 客户端盲化密码
        M_blind = oprf_server.blind(cpw)
        # 客户端请求服务器对盲化消息签名（模拟网络请求）
        S_blind = oprf_server.sign_blind(M_blind)
        # 客户端去盲获得 OPRF 输出
        oprf_val = oprf_server.unblind(S_blind)
        oprf_val_hex = int_to_hex_str(oprf_val)

        # 客户端查询 Bloom Filter
        leaked = bf.check(oprf_val_hex)
        print(f"Password '{cpw}' is {'LEAKED' if leaked else 'NOT leaked'}")

if __name__ == "__main__":
    simulate_protocol()
