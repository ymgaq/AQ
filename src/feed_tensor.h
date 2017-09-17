#pragma once

#include "board.h"


/**************************************************************
 *
 *  入力テンソルのクラス
 *
 *  ニューラルネットに入力する盤面の特徴量を格納する.
 *  12種類49個の特徴を各座標(361)に持つ.
 *
 *  Class of the input tensor.
 *
 *  This class has the features of board for feed of the neural net.
 *  Each vertex on the real board (19x19) contains 49 features,
 *  which are classified into 12 types.
 *
 ***************************************************************/
class FeedTensor {

public:

	//  Input features for the neural net.
	//	[0]-[2]:	stone 0->empty 1->player 2->opponent
	//	[3]:		zeros
	//	[4]:		ones
	//	[5]-[12]:	turn_since 5->previous move, 6->two moves before, ...
	//	[13]-[20]:	liberty
	//	[21]-[28]:	capture size
	//	[29]-[36]:	self Atari size
	//	[37]-[44]:	liberty after
	//	[45]:		ladder capture
	//	[46]:		ladder escape
	//	[47]:		sensibleness
	//	[48]:		false eye
	std::array<std::array<float, 49>, BVCNT> feature;

	int color; //turn of white -> 0, black -> 1.
	int next_move;

	FeedTensor();
	FeedTensor& operator=(const FeedTensor& other);
	void Clear();
	void Set(Board& b, int nv);
	void Load(Board& b);

};
