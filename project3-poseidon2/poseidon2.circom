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

	for (var r = 0; r < 65; r++) {
		
		if( r < fullRounds / 2 || r >= 65 - fullRounds / 2) {
			for(var i = 0;i < 3;i++){
				sstate[r][i] <== state[r][i] * 5;
			}
		} else {
			sstate[r][0] <== state[r][0] * 5;
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
