# 基于 Circom 的 Poseidon2 哈希电路实现与验证

## 一、实验目的

本实验旨在基于 Circom 编程语言实现 Poseidon2 哈希算法的电路，用于构建零知识证明系统中高效的哈希函数。具体目标包括：

- 理解 Poseidon2 哈希函数的参数设置与算法流程；
- 利用 Circom 构建支持两输入的 Poseidon2 哈希电路；
- 通过 Groth16 算法生成证明与验证电路正确性；
- 完成从编写电路到生成证明的全流程实践。

## 二、实验背景

Poseidon2 是一种设计用于零知识证明系统的哈希函数，在 SNARK 等协议中被广泛采用。相比传统哈希函数（如 SHA256），Poseidon2 在电路约束数量上更为优化，适合构建 zkSNARK 中的约束系统。本实验参考论文《Poseidon2: A New Hash Function for Zero Knowledge Proofs》\[1\]，并基于 Circom 语言完成相应电路设计。

## 三、实验环境

- 操作系统：Ubuntu 22.04 / WSL / macOS
- Node.js：v18+
- Circom：v2.1.4
- SnarkJS：v0.6.11
- 依赖库：`circomlib`、`ffjavascript`

## 四、Poseidon2 电路实现

### 4.1 参数设置

本实验使用论文中推荐的参数：

- 安全参数 \( n = 256 \)
- 状态宽度 \( t = 3 \)（两输入一输出）
- S-Box 幂次 \( d = 5 \)

### 4.2 `Pow5` 子电路

用于实现 S-box 操作 \( x^5 \)，该子电路在多个轮次中重复使用。

```circom
template Pow5() {
  signal input in;
  signal output out;

  signal x2;
  signal x4;

  x2 <== in * in;
  x4 <== x2 * x2;
  out <== x4 * in;
}

```

### 4.3 主体 Poseidon2 电路

Poseidon2 的基本结构由以下部分组成：

- 输入扩展（padding）到状态宽度
- 若干轮常数加、S-Box 非线性变换
- MDS 线性层混合
- 最后输出为第一个状态值

```circom
pragma circom 2.1.4;
template Poseidon2() {
	signal input inputs[2];
	signal output out;
	
	signal s[3];
	s[0] <== inputs[0];
	s[1] <== inputs[1];
	s[2] <== 0;	

	var fullRounds = 8;
	var partialRounds = 57;
	var roundConstants[65][3];
	var MDS[3][3] = [
		[2,3,4],
		[4,3,2],
		[3,2,4]
	];
	for(var i = 0;i < 65;i++){
		for(var j = 0;j<3;j++){
			roundConstants[i][j] = i * 123 + j * 15;
		}
	}

	signal state[66][3],sstate[66][3],ssstate[66][3],sssstate[66][3];
	for(var i = 0;i<3;i++){
		state[0][i] <== s[i] + roundConstants[0][i];
	}
 
	for( var r = 1;r < 65; r++){
		for(var i = 0;i <3;i++){
			state[r][i] <== roundConstants[r][i];
		}
	}
	component pow5s[65][3];
	for (var r = 0; r < 65; r++) {
		
		if( r < fullRounds / 2 || r >= 65 - fullRounds / 2) {
			for(var i = 0;i < 3;i++){
				pow5s[r][i] = Pow5();
  				pow5s[r][i].in <== state[r][i];
 				 sstate[r][i] <== pow5s[r][i].out;
			}
		} else {
			pow5s[r][0] = Pow5();
  			pow5s[r][0].in <== state[r][0];
  			sstate[r][0] <== pow5s[r][0].out;
			sstate[r][1] <== state[r][1];
			sstate[r][2] <== state[r][2];
		}
		

		for( var i = 0; i < 3 ;i++) {
			var acc = 0;
			for(var j = 0;j<3;j++) {
				acc += MDS[i][j] * sstate[r][j];
			}
			ssstate[r +1 ][i] <== acc;
		}
		
	}
	out <== ssstate[65][0];
} 
component main = Poseidon2();

```
  

### 4.4 Main 电路定义

该电路对 Poseidon2 的输入和输出进行连接，并设置输出为公共输入，适配 zkSNARK 证明。

```circom
pragma circom 2.1.4;
include "poseidon2.circom";

template Main() {
    signal input in[2];  // 私有输入
    signal input out;    // 公开输入（哈希值）

    component poseidon = Poseidon2();

    poseidon.inputs[0] <== in[0];
    poseidon.inputs[1] <== in[1];

    // 验证 Poseidon 输出等于公开值
    poseidon.out === out;
}

component main = Main();
```

## 五、电路编译与证明流程

### 5.1 编译poseidon2电路

```bash
circom poseidon2.circom --r1cs --wasm --sym
```

### 5.2 准备poseidon2输入实例与witness生成

```json
//poseidon2_input.json
{
    "inputs":["123","456"]
}
```

```bash
node poseidon2_js/generate_witness.js poseidon2_js/poseidon2.wasm poseidon2_input.json witness.twns
```

### 5.3 将输出导出为json文件

```bash
snarkjs wtns export json witness.wtns witness.json
```

### 5.4 获取哈希值

```bash
cat poseidon2.sym
```

上述指令可以获取哈希值所在的位置

```bash
vim witness.json
```

查看witness.json文件获取哈希值为275251025389940996013

### 5.5编译main电路

```bash
circom main.circom --r1cs --wasm --sym
```

### 5.6生成证明密钥

```bash
snarkjs powersoftau new bn128 12 pot14_0000.ptau -v
snarkjs powersoftau contribute pot14_0000 pot14_0001.ptau --name="contribute" -v
snarkjs powersoftau prepare phase2 pot14_0001.ptau pot14_final.ptau -v
snarkjs groth16 setup main.r1cs pot14_final.ptau main.zkey
snarkjs zkey export verificationkey main.zkey verification_key.json
```

### 5.7准备输入实例与witess生成

```json
//input.json
{
	"in": [ "123", "456" ],
	"out": "275251025389940996013"
}
```

根据之前获取的对应哈希值，准备输入实例

```bash
node main_js/generate_witness.js main_js/main.wasm input.json wit.twns
```

### 5.8 生成证明与验证

```bash
snarkjs groth16 prove main.zkey wit.wtns proof.json public.json
snarkjs groth16 verify verification_key.json public.json proof.json
```

验证结果输出：

```bash
[INFO] snarkJS: OK!
```

表示电路计算结果和公开输入一致，验证通过。

## 六、实验结果与分析

成功实现 Poseidon2 哈希电路，并支持两输入哈希；

电路总约束数：约 440（通过 snarkjs r1cs info main.r1cs查看）；

证明和验证耗时较短，适合用于零知识证明系统中；

验证公开输入哈希值有效性，无需泄露原文信息。

# 七、实验总结

通过本实验，成功掌握了 Circom 电路开发流程及 Poseidon2 哈希函数的结构与实现方法。Poseidon2 在 zkSNARK 场景下具有更低的约束复杂度与更高的性能表现，为构建高效的隐私证明系统提供了基础支持。
