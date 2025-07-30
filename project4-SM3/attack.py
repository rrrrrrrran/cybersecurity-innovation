import struct
import copy

# ======== SM3算法实现 ===========
IV = [
    0x7380166F,
    0x4914B2B9,
    0x172442D7,
    0xDA8A0600,
    0xA96F30BC,
    0x163138AA,
    0xE38DEE4D,
    0xB0FB0E4E,
]

T_j = [0x79CC4519] * 16 + [0x7A879D8A] * 48
def left_rotate(x, n):
    n = n % 32  # 保证位移量合法
    return ((x << n) & 0xFFFFFFFF) | (x >> (32 - n))


def FF(x, y, z, j):
    return x ^ y ^ z if j < 16 else (x & y) | (x & z) | (y & z)

def GG(x, y, z, j):
    return x ^ y ^ z if j < 16 else (x & y) | (~x & z)

def P0(x):
    return x ^ left_rotate(x, 9) ^ left_rotate(x, 17)

def P1(x):
    return x ^ left_rotate(x, 15) ^ left_rotate(x, 23)

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

def sm3_cf(v_i, b_i):
    W = []
    for i in range(16):
        W.append(int.from_bytes(b_i[i*4:(i+1)*4], byteorder='big'))
    for i in range(16, 68):
        W.append(P1(W[i-16] ^ W[i-9] ^ left_rotate(W[i-3], 15)) ^ left_rotate(W[i-13], 7) ^ W[i-6])
    W_ = [W[i] ^ W[i+4] for i in range(64)]

    A, B, C, D, E, F, G, H = v_i

    for j in range(64):
        SS1 = left_rotate((left_rotate(A, 12) + E + left_rotate(T_j[j], j)) & 0xFFFFFFFF, 7)
        SS2 = SS1 ^ left_rotate(A, 12)
        TT1 = (FF(A, B, C, j) + D + SS2 + W_[j]) & 0xFFFFFFFF
        TT2 = (GG(E, F, G, j) + H + SS1 + W[j]) & 0xFFFFFFFF
        A, B, C, D = TT1, A, left_rotate(B, 9), C
        E, F, G, H = P0(TT2), E, left_rotate(F, 19), G

    return [(a ^ b) & 0xFFFFFFFF for a, b in zip(v_i, [A, B, C, D, E, F, G, H])]

def sm3_hash(msg):
    m = padding(msg)
    v = IV.copy()
    for i in range(0, len(m), 64):
        b = m[i:i+64]
        v = sm3_cf(v, b)
    return b''.join(x.to_bytes(4, 'big') for x in v)

# ========== 长度扩展攻击 ==============

def sm3_continue_hash(orig_hash_bytes, append_data, total_len):
    v = [int.from_bytes(orig_hash_bytes[i*4:(i+1)*4], 'big') for i in range(8)]
    new_data = padding(append_data, total_len + len(append_data))
    for i in range(0, len(new_data), 64):
        v = sm3_cf(v, new_data[i:i+64])
    return b''.join(x.to_bytes(4, 'big') for x in v)

def length_extension_attack(orig_hash, orig_len, extension):
    padding_bytes = padding(b'A'*orig_len)[orig_len:]  # 计算真实的 padding
    new_hash = sm3_continue_hash(orig_hash, extension, orig_len + len(padding_bytes))
    return new_hash, padding_bytes

# ========== 攻击演示 ==============

m = b"abc"
orig_len = len(m)
orig_hash = sm3_hash(m)
extension = b"def"

# 攻击阶段：伪造 hash 和构造 payload
new_hash, padding1 = length_extension_attack(orig_hash, orig_len, extension)
forged_msg = m + padding1 + extension
verify_hash = sm3_hash(forged_msg)

print("原始 hash:     ", orig_hash.hex())
print("伪造 hash:     ", new_hash.hex())
print("验证用 hash:   ", verify_hash.hex())

# 最终验证
assert new_hash == verify_hash
print("✅ 长度扩展攻击成功！")
