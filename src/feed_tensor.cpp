#include <algorithm>
#include <fstream>
#include <iostream>

#include "ladder.h"
#include "feed_tensor.h"


/**
 *  周囲の座標に同一処理をするマクロ
 *  Macro that executes the same processing on surrounding vertexes.
 */
#define forEach4Nbr(v_origin,v_nbr,block)			\
	int v_nbr;										\
	v_nbr = v_origin + 1;			block;			\
	v_nbr = v_origin - 1;			block;			\
	v_nbr = v_origin + EBSIZE;		block;			\
	v_nbr = v_origin - EBSIZE;		block;

#define forEach8Nbr(v_origin,v_nbr,d_opp,block)					\
	int v_nbr;	int d_opp;											\
	v_nbr = v_origin + EBSIZE;			d_opp = 2;		block;		\
	v_nbr = v_origin + 1;				d_opp = 3;		block;		\
	v_nbr = v_origin - EBSIZE;			d_opp = 0;		block;		\
	v_nbr = v_origin - 1;				d_opp = 1;		block;		\
	v_nbr = v_origin + EBSIZE - 1;		d_opp = 6;		block;		\
	v_nbr = v_origin + EBSIZE + 1;		d_opp = 7;		block;		\
	v_nbr = v_origin - EBSIZE + 1;		d_opp = 4;		block;		\
	v_nbr = v_origin - EBSIZE - 1;		d_opp = 5;		block;


FeedTensor::FeedTensor(){

	for(auto& i:feature){
		for(auto& j:i) j = 0.0;
		i[4] = 1.0;	//ones
	}
	next_move = PASS;
	color = 0;

}


FeedTensor& FeedTensor::operator=(const FeedTensor& other){

	feature = other.feature;
	next_move = other.next_move;
	color = other.color;

	return *this;

}


void FeedTensor::Clear(){

	for(auto& i:feature){
		for(auto& j:i) j = 0.0;
		i[4] = 1.0;	//ones
	}
	next_move = PASS;
	color = 0;

}


/**
 *  盤面から特徴を抽出してFeedTensorに入力する
 *  Set tensors from the board.
 */
void FeedTensor::Set(Board& b, int nv){

	Clear();		
	next_move = nv;
	color = b.my;

	// 1. turn since(5-12)を更新
	//    Update turn since (5-12).
	for(int i=0, i_max=std::min(8,int(b.move_cnt));i<i_max;++i){
		int v = b.move_history[b.move_cnt - 1 - i];
		if(v == PASS) continue;
		feature[etor[v]][5 + i] = 1.0;
	}

	for (int rv=0;rv<BVCNT;++rv) {
		int v = rtoe[rv];
		int v_color = b.color[v];
		int pl_color[2] = { 2 + int(b.my == 1), 2 + int(b.my == 0) };

		// 2. stone(0-3)を更新. Update stone (0-3).
		//    v_color == 0 -> empty,  2 -> white, 3 -> black
		feature[rv][int(v_color != 0)*(1 + int(b.my == int(v_color == 2)))] = 1.0;

		if (v_color >= 2) {

			// 3. liberty(13-20)を更新. Update number of liberty (13-20).
			//    [20] -> 8 or more liberty
			feature[rv][13 + std::min(7, (int)b.ren[b.ren_idx[v]].lib_cnt - 1)] = 1.0;

		}
		else if (b.IsLegal(b.my, v)) {

			// 4. sensibleness(47)を更新
			//    Update sensibleness (47).
			if(!b.IsEyeShape(b.my, v) && !b.IsSeki(v)){
				feature[rv][47] = 1.0;
			}

			// 5. vの周囲の連・空点を調べる
			//    Check surrounding Rens.
			std::vector<int> my_rens;

			int64 lib_bits[6] = {0,0,0,0,0,0};
			for (auto dv : VSHIFT) {
				int v_nbr = v + dv;
				if (b.color[v_nbr] == 0){
					lib_bits[etor[v_nbr]/64] |= (0x1ULL<<(etor[v_nbr]%64));
				}
				else if (b.color[v_nbr] == pl_color[0]){
					my_rens.push_back(b.ren_idx[v_nbr]);
				}
			}			
			sort(my_rens.begin(),my_rens.end());
			my_rens.erase(unique(my_rens.begin(),my_rens.end()),my_rens.end());

			// 6. vに隣接する連のサイズ・呼吸点を調べる
			//    Count size and number of liberty of the neighboring Rens.
			int cap_stone_cnt = 0;
			int my_stone_cnt = 1;
			std::vector<int> checked_ren_idxs;

			for (auto dv : VSHIFT) {
				int v_nbr = v + dv;

				// vの隣接交点が敵石. Opponent's stone.
				if (b.color[v_nbr] == pl_color[1]) {

					// アタリかつ調べてない連か. If it is in Atari and not checked.
					if (b.ren[b.ren_idx[v_nbr]].IsAtari() &&
						find(checked_ren_idxs.begin(), checked_ren_idxs.end(),b.ren_idx[v_nbr])==checked_ren_idxs.end())
					{

						checked_ren_idxs.push_back(b.ren_idx[v_nbr]);
						lib_bits[etor[v_nbr]/64] |= (0x1ULL<<(etor[v_nbr]%64));
						cap_stone_cnt += b.ren[b.ren_idx[v_nbr]].size;

						int v_tmp = v_nbr;
						do {
							for (auto k : VSHIFT) {
								if (b.color[v_tmp + k] == pl_color[0]) {
									if (find(my_rens.begin(),my_rens.end(),b.ren_idx[v_tmp + k]) != my_rens.end()){
										lib_bits[etor[v_tmp]/64] |= (0x1ULL<<(etor[v_tmp]%64));
									}
								}
							}
							v_tmp = b.next_ren_v[v_tmp];
						} while (v_tmp != v_nbr);

					}
				}
				// vの隣接交点が自石. Player's stone.
				else if (b.color[v_nbr] == pl_color[0]) {

					// 調べてない連か. If not checked.
					if (find(checked_ren_idxs.begin(),checked_ren_idxs.end(),b.ren_idx[v_nbr]) == checked_ren_idxs.end()) {

						checked_ren_idxs.push_back(b.ren_idx[v_nbr]);
						my_stone_cnt += b.ren[b.ren_idx[v_nbr]].size;
						for(int k=0;k<6;++k){
							lib_bits[k] |= b.ren[b.ren_idx[v_nbr]].lib_bits[k];
						}

					}

				}
			}

			// 7. capture size(21-28)を更新
			//    Update capture size (21-28).
			if(cap_stone_cnt != 0){
				feature[rv][21 + std::min(7, cap_stone_cnt - 1)] = 1.0;
			}

			lib_bits[etor[v]/64] &= ~(0x1ULL<<(etor[v]%64));	// vを除外. Exclude v.
			int lib_cnt = 0;
			for(int k=0;k<6;++k){
				if(lib_bits[k] != 0){
					lib_cnt += (int)popcnt64(lib_bits[k]);
				}
			}

			// 8. self atari size(29-36)を更新
			//    Update self Atari size (29-36).
			if (lib_cnt == 1) {
				feature[rv][29 + std::min(7, my_stone_cnt - 1)] = 1.0;
			}
			// 9. liberty after(37-44)を更新
			//    Update liberty after (37-44).
			feature[rv][37 + std::min(7, lib_cnt - 1)] = 1.0;
		}
		// 10. false eye(48)を更新
		//     Update false eye (48).
		if(v_color == 0 && b.IsFalseEye(v)){
			feature[rv][48] = 1.0;
		}
	}

	// 11. ladder escape/capture (45-46)を更新
	//     Update ladder escape/capture (45-46)
	std::vector<int> ladder_list[2];
	if (SearchLadder(b, ladder_list)) {
		for (auto v : ladder_list[0]) {
			feature[etor[v]][46] = 1.0; // Ladder escape.
		}
		for (auto v : ladder_list[1]) {
			feature[etor[v]][45] = 1.0; // Ladder capture.
		}
	}

}

/**
 *  FeedTensorの特徴から盤面を復元する
 *  Restore board from the features of FeedTensor.
 */
void FeedTensor::Load(Board& b) {

	b.Clear();
	b.my = color;
	b.her = int(color == 0);

	int move_history[8] = { PASS, PASS, PASS, PASS, PASS, PASS, PASS, PASS };

	for (int i=0;i<BVCNT;++i) {
		int v = rtoe[i];
		if (feature[i][0] == 0) {
			int pl = b.my;
			if (feature[i][2] != 0) pl = b.her;

			b.color[v] = pl + 2;
			++b.stone_cnt[pl];
			++b.move_cnt;
			b.move_history.push_back(VNULL);

			--b.empty_cnt;
			b.empty_idx[b.empty[b.empty_cnt]] = b.empty_idx[v];
			b.empty[b.empty_idx[v]] = b.empty[b.empty_cnt];

			forEach8Nbr(v, v_nbr8, d_opp, {
				b.ptn[v_nbr8].SetColor(d_opp, b.color[v]);
			});

			b.ReplaceProb(0, v, 0.0);
			b.ReplaceProb(1, v, 0.0);
			b.is_placed[pl][v] = true;
			b.ren_idx[v] = v;
			for(auto& lb:b.ren[v].lib_bits) lb = 0;
			b.ren[v].lib_cnt = 0;
			b.ren[v].size = 1;
			b.ren[v].lib_atr = VNULL;

			forEach4Nbr(v, v_nbr, {
				if (b.color[v_nbr] == 0){
					b.ren[b.ren_idx[v]].lib_bits[etor[v_nbr]/64] |= (0x1ULL<<(etor[v_nbr]%64));
					b.ren[b.ren_idx[v]].lib_cnt++;
					b.ren[b.ren_idx[v]].lib_atr = v_nbr;
				}
				else{
					if(b.ren[b.ren_idx[v_nbr]].lib_bits[etor[v]/64] & (0x1ULL<<(etor[v]%64))){
						b.ren[b.ren_idx[v_nbr]].lib_bits[etor[v]/64] ^= (0x1ULL<<(etor[v]%64));
						b.ren[b.ren_idx[v_nbr]].lib_cnt--;

						if(b.ren[b.ren_idx[v_nbr]].lib_cnt == 1){
							for(int i=0;i<6;++i){
								if(b.ren[b.ren_idx[v_nbr]].lib_bits[i] != 0){
									b.ren[b.ren_idx[v_nbr]].lib_atr = rtoe[NTZ(b.ren[b.ren_idx[v_nbr]].lib_bits[i]) + i * 64];
								}
							}
						}
					}
				}
			});

			// 自石と結合. Merge Rens.
			forEach4Nbr(v, v_nbr1, {
				if (b.color[v_nbr1] == b.color[v] && b.ren_idx[v_nbr1] != b.ren_idx[v]) {

					if (b.ren[b.ren_idx[v]].size > b.ren[b.ren_idx[v_nbr1]].size) {

						b.ren[b.ren_idx[v]].lib_cnt = 0;
						for(int i=0;i<6;++i){
							b.ren[b.ren_idx[v]].lib_bits[i] |= b.ren[b.ren_idx[v_nbr1]].lib_bits[i];
							b.ren[b.ren_idx[v]].lib_cnt += (int)popcnt64(b.ren[b.ren_idx[v]].lib_bits[i]);
						}
						if(b.ren[b.ren_idx[v]].lib_cnt == 1){
							for(int i=0;i<6;++i){
								if(b.ren[b.ren_idx[v]].lib_bits[i] != 0){
									b.ren[b.ren_idx[v]].lib_atr = rtoe[NTZ(b.ren[b.ren_idx[v]].lib_bits[i]) + i * 64];
								}
							}
						}
						b.ren[b.ren_idx[v]].size += b.ren[b.ren_idx[v_nbr1]].size;

						int v_tmp = v_nbr1;
						do {
							b.ren_idx[v_tmp] = b.ren_idx[v];
							v_tmp = b.next_ren_v[v_tmp];
						} while (v_tmp != v_nbr1);

						std::swap(b.next_ren_v[v], b.next_ren_v[v_nbr1]);
					}
					else {
						b.ren[b.ren_idx[v_nbr1]].lib_cnt = 0;
						for(int i=0;i<6;++i){
							b.ren[b.ren_idx[v_nbr1]].lib_bits[i] |= b.ren[b.ren_idx[v]].lib_bits[i];
							b.ren[b.ren_idx[v_nbr1]].lib_cnt += (int)popcnt64(b.ren[b.ren_idx[v_nbr1]].lib_bits[i]);
						}
						if(b.ren[b.ren_idx[v_nbr1]].lib_cnt == 1){
							for(int i=0;i<6;++i){
								if(b.ren[b.ren_idx[v_nbr1]].lib_bits[i] != 0){
									b.ren[b.ren_idx[v_nbr1]].lib_atr = rtoe[NTZ(b.ren[b.ren_idx[v_nbr1]].lib_bits[i]) + i * 64];
								}
							}
						}
						b.ren[b.ren_idx[v_nbr1]].size += b.ren[b.ren_idx[v]].size;

						int v_tmp = v;
						do {
							b.ren_idx[v_tmp] = b.ren_idx[v_nbr1];
							v_tmp = b.next_ren_v[v_tmp];
						} while (v_tmp != v);

						std::swap(b.next_ren_v[v_nbr1], b.next_ren_v[v]);
					}
				}
			});
		}

		for (int j=0;j<8;++j) {
			if (feature[i][j + 5] != 0) {
				if (j == 0) b.prev_move[b.her] = v;
				else if (j == 1) b.prev_move[b.my] = v;
				move_history[j] = v;
			}
		}
	}

	// 手数の偶奇を調整
	// Adjust move_cnt.
	if ((b.my == 1 && b.move_cnt % 2 != 0) ||
		(b.my == 0 && b.move_cnt % 2 == 0)) {
		++b.move_cnt;
		b.move_history.push_back(VNULL);
	}
	// move_historyに直近の8手を登録
	// Add last 8 moves to move_history.
	for (int i=0;i<8;++i) {
		int move_idx = b.move_cnt - 1 - i;
		if(move_idx < 0) break;
		b.move_history[move_idx] = move_history[i];
	}

	// 空点のアタリ・2呼吸点・確率値を更新
	// Update 3x3 pattern and probability of empty vertexes.
	for (int i=0, n=b.empty_cnt;i<n;++i) {
		int v = b.empty[i];
		b.ptn[v].SetAtari(
			b.ren[b.ren_idx[v + EBSIZE]].lib_cnt == 1,
			b.ren[b.ren_idx[v + 1]].lib_cnt == 1,
			b.ren[b.ren_idx[v - EBSIZE]].lib_cnt == 1,
			b.ren[b.ren_idx[v - 1]].lib_cnt == 1);
		b.ptn[v].SetPreAtari(
			b.ren[b.ren_idx[v + EBSIZE]].lib_cnt == 2,
			b.ren[b.ren_idx[v + 1]].lib_cnt == 2,
			b.ren[b.ren_idx[v - EBSIZE]].lib_cnt == 2,
			b.ren[b.ren_idx[v - 1]].lib_cnt == 2);
		b.ReplaceProb(0, v, prob_dist_base[v] * b.ptn[v].GetProb3x3(0, false));
		b.ReplaceProb(1, v, prob_dist_base[v] * b.ptn[v].GetProb3x3(1, false));

		if (feature[etor[v]][47] == 0 &&
			b.IsLegal(b.my, v) &&
			!b.IsEyeShape(b.my, v) &&
			!b.IsSeki(v))
		{
			// コウ地点を追加
			// Add Ko.
			b.ko = v;
		}
	}

}


#undef forEach4Nbr
#undef forEach8Nbr
