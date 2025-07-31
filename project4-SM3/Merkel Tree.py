from gmssl import sm3, func
import random

# ------ SM3 Merkle Tree ------

def sm3_hash(data: bytes) -> str:
    return sm3.sm3_hash(func.bytes_to_list(data))

def hash_pair(left: str, right: str) -> str:
    return sm3_hash(bytes.fromhex(left + right))

class MerkleTree:
    def __init__(self, leaves: list[bytes]):
        self.leaves = [sm3_hash(leaf) for leaf in leaves]
        self.levels = [self.leaves]
        self.build_tree()

    def build_tree(self):
        current = self.leaves
        while len(current) > 1:
            next_level = []
            for i in range(0, len(current), 2):
                left = current[i]
                if i + 1 < len(current):
                    right = current[i + 1]
                else:
                    right = left
                next_level.append(hash_pair(left, right))
            self.levels.append(next_level)
            current = next_level

    def get_root(self) -> str:
        return self.levels[-1][0]

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

    @staticmethod
    def verify_proof(leaf: bytes, proof: list[tuple[str, str]], root: str) -> bool:
        current = sm3_hash(leaf)
        for direction, sibling in proof:
            if direction == 'L':
                current = hash_pair(sibling, current)
            else:
                current = hash_pair(current, sibling)
        return current == root

# ------ 测试 ------

if __name__ == "__main__":
    leaf_count = 100000  # 可改为 2**17 = 131072
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
    print(f"验证不存在的叶子 '{fake_leaf.decode()}' 是否存在于树中: {is_valid} ")
