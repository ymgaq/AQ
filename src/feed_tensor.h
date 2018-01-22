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
	
#ifdef USE_52FEATURE
	//  Input features for the neural net.
	//	[0]-[15]:	stones 0->my(t) 1->her(t) 2->my(t-1) ...
	//  [16]:		color
	//	[17]-[24]:	liberty
	//	[25]-[32]:	capture size
	//	[33]-[40]:	self Atari size
	//	[41]-[48]:	liberty after
	//	[49]:		ladder capture
	//	[50]:		ladder escape
	//	[51]:		sensibleness
	std::array<std::array<float, 52>, BVCNT> feature;
#else
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
#endif // USE_52FEATURE

	int color; //turn of white -> 0, black -> 1.
	int next_move;

	FeedTensor();
	FeedTensor& operator=(const FeedTensor& other);
	void Clear();
	void Set(Board& b, int nv);
	//void Load(Board& b);

};

enum FeatureIndex {
#ifdef USE_52FEATURE
	STONES = 0, COLOR = 16, LIBERTY = 17, CAPTURESIZE = 25, SELFATARI = 33,
	LIBERTYAFTER = 41, LADDERCAP = 49, LADDERESC = 50, SENSIBLENESS = 51,
#else
	TURNSINCE = 5, LIBERTY = 13, CAPTURESIZE = 21, SELFATARI = 29, LIBERTYAFTER = 37,
	LADDERCAP = 45, LADDERESC = 46, SENSIBLENESS = 47, FALSEEYE = 48,
#endif // USE_52FEATURE
};

std::string TrimString(const std::string& str, const char* trim_chars = " \t\v\r\n");
bool IsFlagOn(const std::string& str);
void MakeLearningData();
