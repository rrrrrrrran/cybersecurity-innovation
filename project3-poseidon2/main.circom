pragma circom 2.1.4;

include "poseidon2.circom";

template Main() {
	signal input in[2];
	signal input out;
	component poseidon = Poseidon2();
	poseidon.inputs[0] <== in[0];
	poseidon.inputs[1] <== in[1];
	poseidon.out === out;
}
component main = Main();
