#pragma once

#include <array>
#include <unordered_map>
#include <unordered_set>

extern double prob_ptn3x3[65536][256][4];
extern std::unordered_map<int, std::array<double, 4>> prob_ptn12;
extern std::unordered_map<int, std::array<double, 2>> prob_ptn_rsp;
extern std::unordered_set<int> ladder_ptn[2];

class Pattern3x3 {
public:
	int bf;

	Pattern3x3(){ bf = 0x00000000; }
	Pattern3x3(int ibf){ bf = ibf; }
	Pattern3x3& operator=(const Pattern3x3& rhs){ bf = rhs.bf; return *this; }

	// 初期化.
	// Initialize
	void Clear(){ bf = 0x00000000; }

	// Null初期化
	// Set null value. =UINT_MAX.
	void SetNull(){ bf = 0xffffffff; }

	// 一方向の石を返す
	// Return stone color in a direction.
	// dir: 0(N),1(E),2(S),3(W),4(NW),5(NE),6(SE),7(SW),
	//      8(NN),9(EE),10(SS),11(WW)
	int ColorAt(int dir) const{ return (bf >> (2 * dir)) & 3; }

	// 一方向の石を更新
	// Update stone color in a direction.
	// dir: 0(N),1(E),2(S),3(W),4(NW),5(NE),6(SE),7(SW)
	//      8(NN),9(EE),10(SS),11(WW)
	void SetColor(int dir, int color)
	{
		bf &= ~(3 << (2 * dir));
		bf |= color << (2 * dir);
	}

	// すべての石の色を反転させる
	// Flip color of all black/white stones.
	void FlipColor()
	{
		// 0xa = 1010
		// 1. bf & 0x00aaaaaa -> 石の上位bit
		// 2. 1bitシフトで下位bitマスクを生成
		// 3. 石の下位bitとXORを取り、11（黒）<->10（白）を変換
		bf ^= ((bf & 0x00aaaaaa) >> 1);
	}

	// 上下左右のpl側の石数を調べる
	// Return count of pl's neighbor(NSEW) stones.
	// pl: 0->white, 1->black.
	int StoneCnt(int pl) const
	{
		return 	int(((bf     ) & 3) == (pl + 2)) +
				int(((bf >> 2) & 3) == (pl + 2)) +
				int(((bf >> 4) & 3) == (pl + 2)) +
				int(((bf >> 6) & 3) == (pl + 2));
	}

	// 上下左右の空点数を調べる
	// Return count of neighbor (NSEW) blank.
	int EmptyCnt() const
	{
		return 	int(((bf     ) & 3) == 0) +
				int(((bf >> 2) & 3) == 0) +
				int(((bf >> 4) & 3) == 0) +
				int(((bf >> 6) & 3) == 0);
	}

	// 上下左右の盤外点数を調べる
	// Return count of neighbor (NSEW) outer boundary.
	int EdgeCnt() const
	{
		return 	int(((bf     ) & 3) == 1) +
				int(((bf >> 2) & 3) == 1) +
				int(((bf >> 4) & 3) == 1) +
				int(((bf >> 6) & 3) == 1);
	}

	// pl側の石に囲まれているか
	// Return whether it is surrounded by pl's stones.
	bool IsEnclosed(int pl) const
	{
		return (StoneCnt(pl) + EdgeCnt()) == 4;
	}

	// 上下左右のアタリを更新
	// Set Atari in each direction (NSEW).
	void SetAtari(bool bn, bool be, bool bs, bool bw){
		//1. アタリにする石の2呼吸点bit(10)を解消
		bf &= ~((bn << 25) | (be << 27) | (bs << 29) | (bw << 31));
		//2. アタリbit(01)を追加
		bf |= (bn << 24) | (be << 26) | (bs << 28) | (bw << 30);
	}

	// 上下左右のアタリを消去
	// Cancel Atari in each direction (NSEW).
	void CancelAtari(bool bn, bool be, bool bs, bool bw)
	{
		bf &= ~((bn << 24) | (be << 26) | (bs << 28) | (bw << 30));
	}

	// 全てのアタリの石を解消する
	// Cancel Atari in all directions.
	void ClearAtari()
	{
		//0xa = 1010, 0xf = 1111
		bf &= 0xaaffffff;
	}

	// 一方向のアタリの有無を返す
	// Return whether the next stone is Atari.
	bool IsAtari(int dir) const
	{
		return (bf >> (24 + 2 * dir)) & 1;
	}

	// いずれかがアタリかを調べる
	// Return whether any of next stones is Atari.
	bool IsAtari() const
	{
		//5 5 = 0101 0101
		return ((bf >> 24) & 0x00000055) != 0;
	}

	// 上下左右の２呼吸点を更新
	// Set pre-Atari (liberty = 2) in each direction (NSEW).
	void SetPreAtari(bool bn, bool be, bool bs, bool bw)
	{
		//1. 2呼吸点にする石のアタリbit(01)を解消
		bf &= ~((bn << 24) | (be << 26) | (bs << 28) | (bw << 30));
		//2. 2呼吸点bit(10)を追加
		bf |= (bn << 25) | (be << 27) | (bs << 29) | (bw << 31);
	}

	// 上下左右の２呼吸点を消去
	// Cancel pre-Atari in each direction (NSEW).
	void CancelPreAtari(bool bn, bool be, bool bs, bool bw)
	{
		bf &= ~((bn << 25) | (be << 27) | (bs << 29) | (bw << 31));
	}

	// 全ての２呼吸点の石を解消する
	// Cancel pre-Atari in all directions.
	void ClearPreAtari()
	{
		//0x5 = 0101, 0xf = 1111
		bf &= 0x55ffffff;
	}

	// 一方向の２呼吸点の有無を返す
	// Return whether the next stone is pre-Atari.
	bool IsPreAtari(int dir) const
	{
		return (bf >> (24 + 2 * dir)) & 2;
	}

	// いずれかが２呼吸点かを調べる
	// Return whether any of next stones is pre-Atari.
	bool IsPreAtari() const
	{
		//0xaa = 1010 1010
		return ((bf >> 24) & 0x000000aa) != 0;
	}

	// pl側において合法手であるかを調べる
	// Return whether pl's move into here is legal.
	bool IsLegal(int pl) const
	{
		// 1. 空点が隣接するときは合法手
		//    Legal if a blank vertex is adjacent.
		if(EmptyCnt() != 0) return true;

		int color_cnt[2] = {0, 0};	//0->white, 1->black
		int atari_cnt[2] = {0, 0};
		int pl_i;

		// 2. 上下左右の石情報を調べる
		//    Count neighbor stones and Atari.
		for (int i=0;i<4;++i) {
			pl_i = ColorAt(i) - 2;	//0->白石, 1->黒石
			if(pl_i >= 0){
				++color_cnt[pl_i];
				if(IsAtari(i))	++atari_cnt[pl_i];
			}
		}

		// 3. 敵石がアタリのとき、または
		//    アタリでない自石があるとき合法手
		//    Legal if opponent's stone is Atari,
		//    or any of my stones is NOT Atari.
		return (	atari_cnt[int(pl==0)] != 0 ||
					atari_cnt[pl] < color_cnt[pl]	);
	}

	// pl側において眼形であるかを調べる
	// Return whether here is pl's eye shape.
	bool IsEyeShape(int pl) const
	{
		// 1. pl側の石に囲まれていないときは眼形でない
		//    Return false if here is not enclosed by pl's stones.
		if(!IsEnclosed(pl)) return false;

		int diag_cnt[4] = {0, 0, 0, 0};

		// 2. ナナメ位置の石を調べる
		//    Count stones in diagonal directions.
		for (int i=4;i<8;++i) {
			++diag_cnt[ColorAt(i)];
		}

		// 3. 欠け目でないとき眼形
		//    ナナメ位置の敵石＋盤外が2以上のとき、欠け目となる
		//    Return true if not false-eye pattern as follows.
		//
		//	  XO.  XO#
		//    O.O  O.#
		//    XO.  .O#
		return (diag_cnt[int(pl==0) + 2] + int(diag_cnt[1] > 0)) < 2;
	}

	// pl側においてシチョウの可能性のあるパターンかを調べる
	// Return whether the pattern matches 'Ladder'. (quick judgment)
	bool IsLadder(int pl) const
	{
		// シチョウの可能性のある頻出パターン
		// Frequent pattern that may be Ladder.
		//
		// (1)盤端の特殊パターン
		//    Edge ladder.
		// #.X
		// #.O <- Atari
		// #.X
		//
		// (2)階段状になる通常パターン
		//    Normal ladder.
		// ..X
		// ..O <- Atari
		// .XO

		//1. 隣接する空点数が2でない or 自石数が1でない場合、該当しない
		//   Return false if blank count is 2,
		//   or my stone count is 1 in neighbor position.
		if(EmptyCnt() != 2 || StoneCnt(pl) != 1) return false;

		int my_color = pl + 2;
		int her_color = int(pl == 0) + 2;

		// 2. (1)のパターンか
		//    Check whether it matches pattern (1).
		if(EdgeCnt() != 0){
			for(int i=0;i<4;++i){
				// 自石がアタリで、反対側が盤端のときはシチョウ
				if(	ColorAt(i) == my_color &&
					IsAtari(i) &&
					ColorAt(i + 2 * (2 * int(i < 2) - 1)) == 1)	//iの反対側が盤端か
				{
					return true;
				}
			}
		}
		// 2. (2)のパターンか
		//    Check whether it matches pattern (2).
		else{
			int color_at[8];
			int atari_at[4];
			for(int i=0;i<8;++i){
				color_at[i] = (bf >> (2 * i)) & 3;
			}
			for(int i=0;i<4;++i){
				atari_at[i] = (bf >> (24 + 2 * i)) & 3;
			}

			const int ladder_ptn[8][5] = {	{0,1,4,6,7}, {0,3,5,6,7},
											{1,0,6,4,7}, {1,2,5,4,7},
											{2,1,7,4,5}, {2,3,6,4,5},
											{3,0,7,5,6}, {3,2,4,5,6}	};

			for(int i=0;i<8;++i){
				for(int j=0;j<5;++j){
					int dir = ladder_ptn[i][j];
					if(j == 0){
						if(	color_at[dir] != my_color ||
							atari_at[dir] != 1) break;
					}
					else if(j == 1){
						if(	color_at[dir] != her_color ||
							atari_at[dir] == 2) break;
					}
					else if(j == 2){
						if(color_at[dir] != her_color) break;
					}
					else{
						if(color_at[dir] == my_color) break;
					}

					if(j == 4) return true;
				}
			}
		}

		// 3. シチョウパターンに該当しない
		//    Return false in other cases.
		return false;
	}

	// pl側の確率パラメータを求める
	// Returns pl's probability.
	double GetProb3x3(int pl, bool is_restored) const
	{
		int stone_bf = bf & 0x0000ffff;
		int atari_bf = bf >> 24;

		return prob_ptn3x3[stone_bf][atari_bf][pl + 2 * (int)is_restored];
	}

	// 石・アタリ情報を時計回りに90°回転する
	// Return Pattern3x3 which is rotated clockwise by 90 degrees.
	Pattern3x3 Rotate() const
	{
		//3 = 0011, c = 1100, f = 1111
		Pattern3x3 rot_ptn(
			((bf << 2) & 0xfcfcfcfc) |
			((bf >> 6) & 0x03030303)
		);

		return rot_ptn;
	}

	// 石・アタリ情報を鏡映反転する
	// Return Pattern3x3 which is horizontally inverted.
	Pattern3x3 Invert() const
	{
		//3 = 0011, c = 1100
		Pattern3x3 mir_ptn(
			(bf & 0x33330033) 			|
			((bf << 4) & 0xc0c000c0)	|
			((bf >> 4) & 0x0c0c000c)	|
			((bf << 2) & 0x0000cc00)	|
			((bf >> 2) & 0x00003300)
		);

		return mir_ptn;
	}

	// 対称操作により最小のハッシュ値を求める
	// Return Pattern3x3 which has the minimum bf.
	Pattern3x3 GetMin() const
	{
		Pattern3x3 tmp_ptn(bf);
		Pattern3x3 min_ptn = tmp_ptn;

		for (int i=0;i<2;++i) {
			for (int j=0;j<4;++j) {
				if (tmp_ptn.bf < min_ptn.bf){
					min_ptn = tmp_ptn;
				}
				tmp_ptn = tmp_ptn.Rotate();
			}
			tmp_ptn = tmp_ptn.Invert();
		}

		return min_ptn;
	}

};

void ImportProbPtn3x3();
