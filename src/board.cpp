#include <algorithm>
#include <assert.h>
#include <unordered_set>

#include "board.h"

/**
 *  周囲の座標に同一処理をするマクロ
 *  Macro that executes the same processing on surrounding vertexes.
 */
#define forEach4Nbr(v_origin,v_nbr,block) \
	int v_nbr;										\
	v_nbr = v_origin + 1;			block;			\
	v_nbr = v_origin - 1;			block;			\
	v_nbr = v_origin + EBSIZE;		block;			\
	v_nbr = v_origin - EBSIZE;		block;

#define forEach4Diag(v_origin,v_diag,block) \
	int v_diag;									\
	v_diag = v_origin + EBSIZE + 1;	block;		\
	v_diag = v_origin + EBSIZE - 1;	block;		\
	v_diag = v_origin - EBSIZE + 1;	block;		\
	v_diag = v_origin - EBSIZE - 1;	block;		

#define forEach8Nbr(v_origin,v_nbr,d_nbr,d_opp,block) \
	int v_nbr;	int d_nbr; int d_opp;											\
	v_nbr = v_origin + EBSIZE;			d_nbr = 0; d_opp = 2;		block;		\
	v_nbr = v_origin + 1;				d_nbr = 1; d_opp = 3;		block;		\
	v_nbr = v_origin - EBSIZE;			d_nbr = 2; d_opp = 0;		block;		\
	v_nbr = v_origin - 1;				d_nbr = 3; d_opp = 1;		block;		\
	v_nbr = v_origin + EBSIZE - 1;		d_nbr = 4; d_opp = 6;		block;		\
	v_nbr = v_origin + EBSIZE + 1;		d_nbr = 5; d_opp = 7;		block;		\
	v_nbr = v_origin - EBSIZE + 1;		d_nbr = 6; d_opp = 4;		block;		\
	v_nbr = v_origin - EBSIZE - 1;		d_nbr = 7; d_opp = 5;		block;

#define forEach12Nbr(v_origin,v_nbr,block)	 \
	int v_nbr;									\
	v_nbr = v_origin + EBSIZE;	block;			\
	v_nbr = v_origin + 1;		block;			\
	v_nbr = v_origin - EBSIZE;	block;			\
	v_nbr = v_origin - 1;		block;			\
	v_nbr = v_origin + EBSIZE - 1;	block;		\
	v_nbr = v_origin + EBSIZE + 1;	block;		\
	v_nbr = v_origin - EBSIZE + 1;	block;		\
	v_nbr = v_origin - EBSIZE - 1;	block;		\
	v_nbr = std::min(v_origin + 2 * EBSIZE, EBVCNT - 1);	block;	\
	v_nbr = v_origin + 2;									block;	\
	v_nbr = std::max(v_origin - 2 * EBSIZE, 0);				block;	\
	v_nbr = v_origin - 2;									block;


/**
 *  N次元配列の初期化。第2引数の型のサイズごとに初期化していく。
 *  Initialize N-d array.
 */
template<typename A, size_t N, typename T>
inline void FillArray(A (&array)[N], const T &val){

    std::fill( (T*)array, (T*)(array+N), val );

}

#ifdef USE_SEMEAI
const double response_w[4][2] = {	{14.639, 1/14.639},
									{57.0406, 1/57.0406},
									{18.1951, 1/18.1951},
									{2.9047, 1/2.9047}};
#else
const double response_w[4][2] = {	{140.648, 1/140.648},
									{1241.99, 1/1241.99},
									{21.7774, 1/21.7774},
									{7.09814, 1/7.09814}};
#endif //USE_SEMEAI

const double semeai_w[2][2] = {{5.19799, 1/5.19799},
								{3.86838, 1/3.86838} };

Board::Board() {

	Clear();

}

Board::Board(const Board& other) {

	*this = other;

}

Board& Board::operator=(const Board& other) {

	my = other.my;
	her = other.her;
	std::memcpy(color, other.color, sizeof(color));
	std::memcpy(prev_color, other.prev_color, sizeof(prev_color));
	std::memcpy(empty, other.empty, sizeof(empty));
	std::memcpy(empty_idx, other.empty_idx, sizeof(empty_idx));
	std::memcpy(stone_cnt, other.stone_cnt, sizeof(stone_cnt));
	empty_cnt = other.empty_cnt;
	ko = other.ko;

	std::memcpy(ren, other.ren, sizeof(ren));
	std::memcpy(next_ren_v, other.next_ren_v, sizeof(next_ren_v));
	std::memcpy(ren_idx, other.ren_idx, sizeof(ren_idx));
	move_cnt = other.move_cnt;
	move_history.clear();
	copy(other.move_history.begin(), other.move_history.end(), back_inserter(move_history));
	std::memcpy(prev_move, other.prev_move, sizeof(prev_move));
	prev_ko = other.prev_ko;
	removed_stones.clear();
	copy(other.removed_stones.begin(), other.removed_stones.end(), back_inserter(removed_stones));
	std::memcpy(prob, other.prob, sizeof(prob));
	std::memcpy(ptn, other.ptn, sizeof(ptn));
	prev_ptn[0].bf = other.prev_ptn[0].bf;
	prev_ptn[1].bf = other.prev_ptn[1].bf;
	prev_ptn_prob = other.prev_ptn_prob;
	std::memcpy(response_move, other.response_move, sizeof(response_move));
	for(int i=0;i<2;++i){
		semeai_move[i].clear();
		copy(other.semeai_move[i].begin(), other.semeai_move[i].end(), back_inserter(semeai_move[i]));
	}
	std::memcpy(is_ptn_updated, other.is_ptn_updated, sizeof(is_ptn_updated));
	updated_ptns.clear();
	copy(other.updated_ptns.begin(), other.updated_ptns.end(), back_inserter(updated_ptns));
	std::memcpy(sum_prob_rank, other.sum_prob_rank, sizeof(sum_prob_rank));
	std::memcpy(pass_cnt, other.pass_cnt, sizeof(pass_cnt));

	return *this;

}


/**
 *  初期化
 *  Initialize the board.
 */
void Board::Clear() {

	// 黒番-> (my, her) = (1, 0), 白番 -> (0, 1).
	// if black's turn, (my, her) = (1, 0) else (0, 1).
	my = 1;
	her = 0;
	empty_cnt = 0;
	FillArray(sum_prob_rank, 0.0);
	FillArray(is_ptn_updated, false);

	for (int i=0;i<EBVCNT;++i) {

		next_ren_v[i] = i;
		ren_idx[i] = i;

		ren[i].SetNull();

		int ex = etox[i];
		int ey = etoy[i];

		// outer boundary
		if (ex == 0 || ex == EBSIZE - 1 || ey == 0 || ey == EBSIZE - 1) {
			color[i] = 1;
			for(auto& pc : prev_color) pc[i] = 1;
			empty_idx[i] = VNULL;	//442
			ptn[i].SetNull();		//0xffffffff

			prob[0][i] = prob[1][i] = 0;
		}
		// real board
		else {
			// empty vertex
			color[i] = 0;
			for(auto& pc : prev_color) pc[i] = 0;
			empty_idx[i] = empty_cnt;
			empty[empty_cnt] = i;
			++empty_cnt;
		}

	}

	for(int i=0;i<BVCNT;++i){

		int v = rtoe[i];

		ptn[v].Clear();	// 0x00000000

		// 周囲8点のcolorを登録する
		// Set colors around v.
		forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
			ptn[v].SetColor(d_nbr, color[v_nbr8]);
		});

		// 確率分布を初期化
		// Initialize probability distribution.

		// 評価値.着手確率 = prob[pl][v]/sum(prob[pl])
		// Probability. Selected probability = prob[pl][v]/sum(prob[pl])
		prob[0][v] = prob_dist_base[v] * ptn[v].GetProb3x3(0,false);
		prob[1][v] = prob_dist_base[v] * ptn[v].GetProb3x3(1,false);

		// 実盤面のn段目のprobの和
		// Sum of prob[pl][v] in the Nth rank on the real board.
		sum_prob_rank[0][rtoy[i]] += prob[0][v];
		sum_prob_rank[1][rtoy[i]] += prob[1][v];

	}

	stone_cnt[0] = stone_cnt[1] = 0;
	empty_cnt = BVCNT;
	ko = VNULL;
	move_cnt = 0;
	move_history.clear();
	removed_stones.clear();
	prev_move[0] = prev_move[1] = VNULL;
	prev_ko = VNULL;
	updated_ptns.clear();
	prev_ptn[0].SetNull();
	prev_ptn[1].SetNull();
	prev_ptn_prob = 0.0;
	for(auto& i: response_move) i = VNULL;
	for(auto& i: semeai_move) i.clear();
	pass_cnt[0] = pass_cnt[1] = 0;

}


/**
 *  座標vへの着手が合法手か
 *  Return whether pl's move on v is legal.
 */
bool Board::IsLegal(int pl, int v) const{

	assert(v <= PASS);

	if (v == PASS) return true;
	if (color[v] != 0 || v == ko) return false;

	return ptn[v].IsLegal(pl);

}


/**
 *  座標vがpl側の眼形か
 *  Return whether v is an eye shape for pl.
 */
bool Board::IsEyeShape(int pl, int v) const{

	assert(color[v] == 0);

	if(ptn[v].IsEnclosed(pl)){

		// 斜め隣接する数 {空点,盤外,白石,黒石}
		// Counter of {empty, outer boundary, white, black} in diagonal positions.
		int diag_cnt[4] = {0, 0, 0, 0};

		for (int i=4;i<8;++i) {
			//4=NW, 5=NE, 6=SE, 7=SW
			++diag_cnt[ptn[v].ColorAt(i)];
		}

		// 斜め位置の敵石数 + 盤外の個数
		// 2以上で欠け目になる
		// False eye if opponent's stones + outer boundary >= 2.
		int wedge_cnt = diag_cnt[int(pl==0) + 2] + int(diag_cnt[1] > 0);

		// 斜め位置の敵石がすぐ取れるとき、欠け目から除外
		// Return true if an opponent's stone can be taken immediately.
		if(wedge_cnt == 2){
			forEach4Diag(v, v_diag, {
				if(color[v_diag] == (int(pl==0) + 2)){
					if(	ren[ren_idx[v_diag]].IsAtari() &&
						ren[ren_idx[v_diag]].lib_atr != ko)
					{
						return true;
					}
				}
			});
		}
		// 欠け目でないとき、眼形とみなす
		// Return true if it is not false eye.
		else return wedge_cnt < 2;
	}

	return false;
}


/**
 *  座標vが欠け目か
 *  Return whether v is a false eye.
 */
bool Board::IsFalseEye(int v) const{

	assert(color[v] == 0);

	// 空点が隣接するときは欠け目でない
	// Return false when empty vertexes adjoin.
	if(ptn[v].EmptyCnt() > 0) return false;

	// 隣接点がすべて敵石or盤端でないときは欠け目でない
	// Return false when it is not enclosed by opponent's stones.
	if(!ptn[v].IsEnclosed(0) && !ptn[v].IsEnclosed(1)) return false;

	int pl = ptn[v].IsEnclosed(0)? 0 : 1;
	// Counter of {empty, outer boundary, white, black} in diagonal positions.
	int diag_cnt[4] = {0, 0, 0, 0};
	for (int i = 4; i < 8; ++i) {
		//4=NW, 5=NE, 6=SE, 7=SW
		++diag_cnt[ptn[v].ColorAt(i)];
	}

	// 斜め位置の敵石数 + 盤外の個数
	// 2以上で欠け目になる
	// False eye if opponent's stones + outer boundary >= 2.
	int wedge_cnt = diag_cnt[int(pl==0) + 2] + int(diag_cnt[1] > 0);

	if(wedge_cnt == 2){
		forEach4Diag(v, v_diag, {
			if(color[v_diag] == (int(pl==0) + 2)){

				// 斜め位置の敵石がすぐ取れるとき、false
				// Not false eye if an opponent's stone can be taken immediately.
				if(ren[ren_idx[v_diag]].IsAtari()) return false;
			}
		});
	}

	return wedge_cnt >= 2;

}


/**
 *  座標vがセキかどうか
 *  Return whether v is Seki.
 */
bool Board::IsSeki(int v) const{

	assert(color[v] == 0);

	// 隣接する空点が2以上 or 両方の石が隣接しないとき -> false
	// Return false when empty vertexes are more than 2 or
	// both stones are not in neighboring positions.
	if(	!ptn[v].IsPreAtari()	||
		ptn[v].EmptyCnt() > 1 	||
		ptn[v].StoneCnt(0) == 0 ||
		ptn[v].StoneCnt(1) == 0	) return false;

	int64 lib_bits_tmp[6] = {0,0,0,0,0,0};
	std::vector<int> nbr_ren_idxs;
	for(int i=0;i<4;++i){
		int v_nbr = v + VSHIFT[i];	// neighboring position

		// when white or black stone
		if(color[v_nbr] > 1){

			// 呼吸点が2でない or サイズが1のとき -> false
			// Return false when the liberty number is not 2 or the size if 1.
			if(!ptn[v].IsPreAtari(i)) return false;
			else if(ren[ren_idx[v_nbr]].size == 1 &&
					ptn[v].StoneCnt(color[v_nbr] - 2) == 1){
				for(int j=0;j<4;++j){
					int v_nbr2 = v_nbr + VSHIFT[j];
					if(v_nbr2 != v && color[v_nbr2] == 0)
					{
						int nbr_cnt = ptn[v_nbr2].StoneCnt(color[v_nbr] - 2);
						if(nbr_cnt == 1) return false;
					}
				}
			}

			nbr_ren_idxs.push_back(ren_idx[v_nbr]);
		}
		else if(color[v_nbr] == 0){
			lib_bits_tmp[etor[v_nbr]/64] |= (0x1ULL << (etor[v_nbr] % 64));
		}
	}

	//int64 lib_bits_tmp[6] = {0,0,0,0,0,0};
	for(auto nbr_idx: nbr_ren_idxs)	{
		for(int i=0;i<6;++i){
			lib_bits_tmp[i] |= ren[nbr_idx].lib_bits[i];
		}
	}

	int lib_cnt = 0;
	for(auto lbt: lib_bits_tmp){
		if(lbt != 0){
			lib_cnt += (int)popcnt64(lbt);
		}
	}

	if(lib_cnt == 2){

		// ナカデのホウリコミができるか
		// Check whether it is Self-Atari of Nakade.
		for(int i=0;i<6;++i){
			while(lib_bits_tmp[i] != 0){
				int ntz = NTZ(lib_bits_tmp[i]);
				int v_seki = rtoe[ntz + i * 64];
				if(IsSelfAtariNakade(v_seki)) return false;

				lib_bits_tmp[i] ^= (0x1ULL << ntz);
			}
		}
		return true;

	}
	else if(lib_cnt == 3){
		// 双方に目がある特殊セキか
		// Check whether Seki where both Rens have an eye.
		int eye_cnt = 0;
		for(int i=0;i<6;++i){
			while(lib_bits_tmp[i] != 0){
				int ntz = NTZ(lib_bits_tmp[i]);
				int v_seki = rtoe[ntz + i * 64];
				if(IsEyeShape(0, v_seki) || IsEyeShape(1, v_seki)) ++eye_cnt;
				if(eye_cnt >= 2 || IsFalseEye(v_seki)) return true;

				lib_bits_tmp[i] ^= (0x1ULL << ntz);
			}
		}
	}

	return false;

}


/**
 *  座標vをupdate_ptnに追加する
 *  Add v to the list of updated patterns.
 */
inline void Board::AddUpdatedPtn(int v) {

	if (!is_ptn_updated[v]) {
		//確率更新フラグを立てる
		is_ptn_updated[v] = true;

		//座標と現在のptnをupdate_ptnsに登録
		updated_ptns.push_back(std::make_pair(v, ptn[v].bf));
	}

}


/**
 *  座標vを含む連のアタリ地点を更新する
 *  Set Atari of the Ren including v.
 */
inline void Board::SetAtari(int v) {

	assert(color[v] > 1);

	int v_atari = ren[ren_idx[v]].lib_atr;

	AddUpdatedPtn(v_atari);
	ptn[v_atari].SetAtari(
		ren_idx[v_atari + EBSIZE] 	== 	ren_idx[v],
		ren_idx[v_atari + 1] 		== 	ren_idx[v],
		ren_idx[v_atari - EBSIZE]	== 	ren_idx[v],
		ren_idx[v_atari - 1] 		== 	ren_idx[v]
	);

}


/**
 *  座標vを含む連の2呼吸地点を更新する
 *  Set pre-Atari of the Ren including v.
 */
inline void Board::SetPreAtari(int v) {

	assert(color[v] > 1);

	int64 lib_bit = 0;
	for(int i=0;i<6;++i){
		lib_bit = ren[ren_idx[v]].lib_bits[i];
		while(lib_bit != 0){
			int ntz = NTZ(lib_bit);
			int v_patr = rtoe[ntz + i * 64];

			AddUpdatedPtn(v_patr);
			ptn[v_patr].SetPreAtari(
				ren_idx[v_patr + EBSIZE] 	== 	ren_idx[v],
				ren_idx[v_patr + 1] 		== 	ren_idx[v],
				ren_idx[v_patr - EBSIZE] 	== 	ren_idx[v],
				ren_idx[v_patr - 1] 		== 	ren_idx[v]
			);

			lib_bit ^= (0x1ULL << ntz);
		}
	}

}


/**
 *  座標vを含む連のアタリ地点を解消する
 *  Cancel Atari of the Ren including v.
 */
inline void Board::CancelAtari(int v) {

	assert(color[v] > 1);

	int v_atari = ren[ren_idx[v]].lib_atr;
	AddUpdatedPtn(v_atari);

	ptn[v_atari].CancelAtari(
		ren_idx[v_atari + EBSIZE] 	== 	ren_idx[v],
		ren_idx[v_atari + 1] 		== 	ren_idx[v],
		ren_idx[v_atari - EBSIZE] 	== 	ren_idx[v],
		ren_idx[v_atari - 1]		== 	ren_idx[v]
	);

}


/**
 *  座標vを含む連の2呼吸地点を解消する
 *  Cancel pre-Atari of the Ren including v.
 */
inline void Board::CancelPreAtari(int v) {

	assert(color[v] > 1);

	int64 lib_bit = 0;
	for(int i=0;i<6;++i){
		lib_bit = ren[ren_idx[v]].lib_bits[i];
		while(lib_bit != 0){
			int ntz = NTZ(lib_bit);
			int v_patr = rtoe[ntz + i * 64];

			AddUpdatedPtn(v_patr);
			ptn[v_patr].CancelPreAtari(
				ren_idx[v_patr + EBSIZE] 	== 	ren_idx[v],
				ren_idx[v_patr + 1] 		== 	ren_idx[v],
				ren_idx[v_patr - EBSIZE] 	== 	ren_idx[v],
				ren_idx[v_patr - 1] 		== 	ren_idx[v]
			);

			lib_bit ^= (0x1ULL << ntz);
		}
	}

}


/**
 *  座標vに石を置く
 *  Place a stone on position v.
 */
inline void Board::PlaceStone(int v) {

	assert(color[v] == 0);

	// 1. 周囲8点の3x3パターンを更新
	//    Update 3x3 patterns around v.
	color[v] = my + 2;
	forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
		if (color[v_nbr8] == 0) {
			is_ptn_updated[v_nbr8] = true;
			updated_ptns.push_back(std::make_pair(v_nbr8, ptn[v_nbr8].bf));
		}
		ptn[v_nbr8].SetColor(d_opp, color[v]);
	});

	// 2. 石数、確率を更新
	//    Update stone number and probability at v.
	++stone_cnt[my];
	--empty_cnt;
	empty_idx[empty[empty_cnt]] = empty_idx[v];
	empty[empty_idx[v]] = empty[empty_cnt];
	ReplaceProb(my, v, 0.0);
	ReplaceProb(her, v, 0.0);

	// 3. vを含む連indexの更新
	//    Update Ren index including v.
	ren_idx[v] = v;
	ren[ren_idx[v]].Clear();

	// 4. 周囲4点の呼吸点を更新
	//    Update liberty on neighboring positions.
	forEach4Nbr(v, v_nbr, {
		// 隣が空点のときは自分の呼吸点を増やす
		// Add liberty when v_nbr is empty.
		if (color[v_nbr] == 0)	ren[ren_idx[v]].AddLib(v_nbr);
		// それ以外は隣の連の呼吸点を減らす
		// Subtracts liberty in other cases.
		else  ren[ren_idx[v_nbr]].SubLib(v);
	});

}

/**
 *  座標vの石を消去
 *  Remove the stone on the position v.
 */
inline void Board::RemoveStone(int v) {

	assert(color[v] > 1);

	// 1. 周囲8点の3x3パターンを更新
	//    Update 3x3 patterns around v.
	color[v] = 0;
	is_ptn_updated[v] = true;
	ptn[v].ClearAtari();
	ptn[v].ClearPreAtari();
	forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
		if(color[v_nbr8] == 0) AddUpdatedPtn(v_nbr8);
		ptn[v_nbr8].SetColor(d_opp, 0);
	});

	// 2. 石数、確率を更新
	//    Update stone number and probability at v.
	--stone_cnt[her];
	empty_idx[v] = empty_cnt;
	empty[empty_cnt] = v;
	++empty_cnt;
	ReplaceProb(my, v, prob_dist_base[v]);
	ReplaceProb(her, v, prob_dist_base[v]);
	ren_idx[v] = v;
	removed_stones.push_back(v);

}

/**
 *  座標v_base、v_addを含む連を結合
 *  v_addを含む連のindexを書き換える
 *  Merge the Rens including v_base and v_add.
 *  Replace index of the Ren including v_add.
 */
inline void Board::MergeRen(int v_base, int v_add) {

	// 1. Renクラスの結合
	//    Merge of Ren class.
	ren[ren_idx[v_base]].Merge(ren[ren_idx[v_add]]);

	// 2. ren_idxをv_baseに書き換える
	//    Replace ren_idx of the Ren including v_add.
	int v_tmp = v_add;
	do {
		ren_idx[v_tmp] = ren_idx[v_base];
		v_tmp = next_ren_v[v_tmp];
	} while (v_tmp != v_add);

	// 3. next_ren_vを交換する
	//    Swap positions of next_ren_v.
	//
	//    (before)
	//    v_base: 0->1->2->3->0
	//    v_add : 4->5->6->4
	//    (after)
	//    v_base: 0->5->6->4->1->2->3->0
	std::swap(next_ren_v[v_base], next_ren_v[v_add]);

}

/**
 *  座標vを含む連を消去
 *  Remove the Ren including v.
 */
inline void Board::RemoveRen(int v) {

	// 1. すべての石を消去
	//    Remove all stones of the Ren.
	std::vector<int> spaces;	//抜いた石の座標を格納する
	int v_tmp = v;
	do {
		RemoveStone(v_tmp);
		spaces.push_back(v_tmp);
		v_tmp = next_ren_v[v_tmp];
	} while (v_tmp != v);

	// 2. 取られた石がナカデか確認
	//    Check whether the space after removing stones is a Nakade shape.
	if(spaces.size()>=3 && spaces.size()<=6){
		unsigned long long space_hash = 0;
		for(auto v_space: spaces){

			// a. vからの相対座標を盤中央に移動したzobristハッシュを求める
			//    Calculate Zobrist hash relative to the center position.
			space_hash ^= zobrist.hash[0][0][v_space - v + EBVCNT / 2];
		}
		if(nakade.vital.find(space_hash) != nakade.vital.end()){
			int v_nakade = v + nakade.vital.at(space_hash);
			if(v_nakade < EBVCNT){

				// b. ナカデの急所を登録する
				//    Registers the vital of Nakade.
				response_move[0] = v_nakade;
			}
		}
	}

	// 3. 隣接する連の呼吸点情報を更新
	//    Update liberty of neighboring Rens.
	std::vector<int> may_patr_list;		//アタリが解消された連indexを格納する
	do {
		forEach4Nbr(v_tmp, v_nbr, {
			if(color[v_nbr] >= 2){

				// 呼吸点が必ず増えるのでアタリ・2呼吸点を解消
				// PlayLegal()でこの後にアタリ・2呼吸点を再度計算する
				// Cancel Atari or pre-Atari because liberty positions are added.
				// Final status of Atari or pre-Atari will be calculated in PlayLegal().
				if(ren[ren_idx[v_nbr]].IsAtari()){
					CancelAtari(v_nbr);
					may_patr_list.push_back(ren_idx[v_nbr]);
				}
				else if(ren[ren_idx[v_nbr]].IsPreAtari()) CancelPreAtari(v_nbr);

			}
			ren[ren_idx[v_nbr]].AddLib(v_tmp);
		});

		int v_next = next_ren_v[v_tmp];
		next_ren_v[v_tmp] = v_tmp;
		v_tmp = v_next;
	}while (v_tmp != v);

	// 4. 隣接する連の呼吸点情報を更新
	//    Update liberty of Neighboring Rens.

	// 重複するindexを除去
	// Remove duplicated indexes.
	sort(may_patr_list.begin(), may_patr_list.end());
	may_patr_list.erase(unique(may_patr_list.begin(),may_patr_list.end()),may_patr_list.end());

	for(auto mpl_idx : may_patr_list){

		// 呼吸点数が2のときに周囲のパターンを更新する
		// Update ptn[] when liberty number is 2.
		if(ren[mpl_idx].IsPreAtari()){
			SetPreAtari(mpl_idx);
		}

	}

}

/**
 *  自己アタリの着手vによってナカデ形の連ができるかを調べる
 *  セキの形でホウリコミをするかの判断に使う
 *
 *  Return whether move on position v is self-Atari and forms Nakade.
 *  Use for checking whether Hourikomi is effective in Seki.
 */
inline bool Board::IsSelfAtariNakade(int v) const{

	// 周囲4点の連が大きさ<=4のとき、ナカデになるかを調べる
	// Check whether it will be Nakade shape when size of urrounding Ren is <= 4.
	std::vector<int> checked_idx[2];
	int64 space_hash[2] = {zobrist.hash[0][0][EBVCNT/2], zobrist.hash[0][0][EBVCNT/2]};
	bool under5[2] = {true, true};
	int64 lib_bits[2][6] = {{0,0,0,0,0,0}, {0,0,0,0,0,0}};

	forEach4Nbr(v,v_nbr,{
		if(color[v_nbr] >= 2){
			int pl = color[v_nbr] - 2;
			if(ren[ren_idx[v_nbr]].size < 5){
				if(find(checked_idx[pl].begin(), checked_idx[pl].end(), ren_idx[v_nbr])
					== checked_idx[pl].end())
				{
					checked_idx[pl].push_back(ren_idx[v_nbr]);

					for(int i=0;i<6;++i)
						lib_bits[pl][i] |= ren[ren_idx[v_nbr]].lib_bits[i];

					int v_tmp = v_nbr;
					do{
						// 盤中央からの相対座標のzobristハッシュ
						// Calculate Zobrist Hash relative to the center position.
						space_hash[pl] ^= zobrist.hash[0][0][v_tmp - v + EBVCNT/2];
						forEach4Nbr(v_tmp, v_nbr2, {
							if(	color[v_nbr2] == int(pl == 0) + 2){
								if(ren[ren_idx[v_nbr2]].lib_cnt != 2){
									under5[pl] = false;
									break;
								}
								else{
									for(int i=0;i<6;++i)
										lib_bits[pl][i] |= ren[ren_idx[v_nbr2]].lib_bits[i];
								}
							}
						});

						v_tmp = next_ren_v[v_tmp];
					} while(v_tmp != v_nbr);
				}
			}
			else under5[pl] = false;
		}
	});

	if(under5[0] && nakade.vital.find(space_hash[0]) != nakade.vital.end())
	{
		int lib_cnt = 0;
		for(auto lb:lib_bits[0]) lib_cnt += (int)popcnt64(lb);
		if(lib_cnt == 2) return true;
	}
	else if(under5[1] && nakade.vital.find(space_hash[1]) != nakade.vital.end())
	{
		int lib_cnt = 0;
		for(auto lb:lib_bits[1]) lib_cnt += (int)popcnt64(lb);
		if(lib_cnt == 2) return true;
	}

	return false;

}


inline bool Board::IsSelfAtari(int pl, int v) const{

	if(ptn[v].EmptyCnt() >= 2) return false;

	int64 lib_bits[6] = {0,0,0,0,0,0};
	int lib_cnt = 0;

	// vに隣接する連の呼吸点を調べる
	// Count number of liberty of the neighboring Rens.
	forEach4Nbr(v, v_nbr, {

		// 空点. Empty vertex.
		if(color[v_nbr] == 0){
			lib_bits[etor[v_nbr]/64] |= (0x1ULL<<(etor[v_nbr]%64));
		}
		// vの隣接交点が敵石. Opponent's stone.
		else if (color[v_nbr] == int(pl == 0)) {
			if (ren[ren_idx[v_nbr]].IsAtari()){
				if(ren[ren_idx[v_nbr]].size > 1) return false;
				lib_bits[etor[v_nbr]/64] |= (0x1ULL<<(etor[v_nbr]%64));
			}
		}
		// vの隣接交点が自石. Player's stone.
		else if (color[v_nbr] == pl) {
			if(ren[ren_idx[v_nbr]].lib_cnt > 2) return false;
			for(int k=0;k<6;++k){
				lib_bits[k] |= ren[ren_idx[v_nbr]].lib_bits[k];
			}
		}

	});

	lib_bits[etor[v]/64] &= ~(0x1ULL<<(etor[v]%64));	// vを除外. Exclude v.
	for(int k=0;k<6;++k){
		if(lib_bits[k] != 0){
			lib_cnt += (int)popcnt64(lib_bits[k]);
		}
	}

	return (lib_cnt <= 1);
}


/**
 *  呼吸点>2の石の周囲に攻め合いできる石がないか調べる．
 */
inline bool Board::Semeai2(std::vector<int>& patr_rens, std::vector<int>& her_libs){

	her_libs.clear();
	if(patr_rens.size() == 0) return false;
	int my_color = my + 2;

	// 1. 重複要素を削除
	//    Remove duplicated indexes.
	sort(patr_rens.begin(), patr_rens.end());
	patr_rens.erase(unique(patr_rens.begin(),patr_rens.end()),patr_rens.end());

	for(auto patr_idx: patr_rens){

		if(ren[ren_idx[patr_idx]].size < 2) continue;
		int v_tmp = patr_idx;
		std::vector<int> tmp_her_libs;

		// 2. 2呼吸点にされた連に隣接する敵石が取れるか調べる
		//    Check whether it is possible to reduce liberty of neighboring stones of the Ren in pre-Atari.
		do {
			forEach4Nbr(v_tmp, v_nbr3,{

				Ren* ren_nbr = &ren[ren_idx[v_nbr3]];

				if(color[v_nbr3] == my_color){

					if(ren_nbr->IsAtari()){
						tmp_her_libs.clear();
						break;
					}
					else if(ren_nbr->IsPreAtari()){
						int64 lib_bit = 0;
						for(int i=0;i<6;++i){
							lib_bit = ren_nbr->lib_bits[i];
							while(lib_bit != 0){
								int ntz = NTZ(lib_bit);
								int v_patr = rtoe[ntz + i * 64];
								if(!IsSelfAtari(her, v_patr)){
									tmp_her_libs.push_back(v_patr);
								}

								lib_bit ^= (0x1ULL << ntz);
							}
						}
					}

				}

			});
			v_tmp = next_ren_v[v_tmp];
		} while (v_tmp != patr_idx);

		if(tmp_her_libs.size() > 0){
			for(auto thl:tmp_her_libs) her_libs.push_back(thl);
		}
	}

	return (her_libs.size() > 0);

}


/**
 *  呼吸点3の石の周囲に攻め合いできる石がないか調べる．
 */
inline bool Board::Semeai3(std::vector<int>& lib3_rens, std::vector<int>& her_libs){

	her_libs.clear();
	if(lib3_rens.size() == 0) return false;
	int my_color = my + 2;

	// 1. 重複要素を削除
	//    Remove duplicated indexes.
	sort(lib3_rens.begin(), lib3_rens.end());
	lib3_rens.erase(unique(lib3_rens.begin(),lib3_rens.end()),lib3_rens.end());

	for(auto patr_idx: lib3_rens){

		if(ren[ren_idx[patr_idx]].size < 3) continue;
		int v_tmp = patr_idx;
		std::vector<int> tmp_her_libs;

		// 2. 3呼吸点にされた連に隣接する敵石が取れるか調べる
		//    Check whether it is possible to reduce liberty of neighboring stones of the Ren with 3 liberties.
		do {
			forEach4Nbr(v_tmp, v_nbr3,{

				Ren* ren_nbr = &ren[ren_idx[v_nbr3]];

				if(color[v_nbr3] == my_color){

					if(ren_nbr->IsAtari()){
						tmp_her_libs.clear();
						break;
					}
					else if(ren_nbr->lib_cnt <= 3){
						int64 lib_bit = 0;
						for(int i=0;i<6;++i){
							lib_bit = ren_nbr->lib_bits[i];
							while(lib_bit != 0){
								int ntz = NTZ(lib_bit);
								int v_patr = rtoe[ntz + i * 64];
								if(!IsSelfAtari(her, v_patr)){
									tmp_her_libs.push_back(v_patr);
								}

								lib_bit ^= (0x1ULL << ntz);
							}
						}
					}

				}

			});
			v_tmp = next_ren_v[v_tmp];
		} while (v_tmp != patr_idx);

		if(tmp_her_libs.size() > 0){
			for(auto thl:tmp_her_libs) her_libs.push_back(thl);
		}
	}

	return (her_libs.size() > 0);

}


/**
 *  座標vに着手する
 *  着手が合法手かは事前に評価しておく必要がある.
 *  Update the board with the move on position v.
 *  It is necessary to confirm in advance whether the move is legal.
 */
void Board::PlayLegal(int v) {

	assert(v <= PASS);
	assert(v == PASS || color[v] == 0);
	assert(v != ko);

	// 1. 棋譜情報を更新
	//    Update history.
	int prev_empty_cnt = empty_cnt;
	bool is_in_eye = ptn[v].IsEnclosed(her);
	prev_ko = ko;
	ko = VNULL;
	move_history.push_back(v);
	++move_cnt;
	removed_stones.clear();
#ifdef USE_52FEATURE
	for(int i=6;i>0;--i){
		std::memcpy(prev_color[i], prev_color[i - 1], sizeof(prev_color[i]));
	}
	std::memcpy(prev_color[0], color, sizeof(prev_color[0]));
#endif

	// 2. 前の着手、12点パターンの確率を解消
	//    Restore probability of distance and 12-point pattern
	//    of the previous move.
	SubProbDist();
	SubPrevPtn();

	// 3. response_moveの確率を解消
	//    Restore probability of response_move.
	response_move[0] = VNULL;	//ナカデの急所. Vital of Nakade.
	if(response_move[1] != VNULL){
		AddProb(my, response_move[1], response_w[1][1]);
		response_move[1] = VNULL;	//アタリの周囲の石を取る. Save Atari stones by taking opponent's stones.
	}
	if(response_move[2] != VNULL){
		AddProb(my, response_move[2], response_w[2][1]);
		response_move[2] = VNULL;	//アタリを逃げる. Save Atari stones by escaping move.
	}
	if(response_move[3] != VNULL){
		AddProb(my, response_move[3], response_w[3][1]);
		response_move[3] = VNULL;	//直前の石を取る. Take a stone placed previously.
	}
#ifdef USE_SEMEAI
	if(semeai_move[0].size() > 0){
		for(auto sm: semeai_move[0]) AddProb(my, sm, semeai_w[0][1]);
		semeai_move[0].clear();
	}
	if(semeai_move[1].size() > 0){
		for(auto sm: semeai_move[1]) AddProb(my, sm, semeai_w[1][1]);
		semeai_move[1].clear();
	}
#endif

	if (v == PASS) {
		++pass_cnt[my];
		prev_move[my] = v;
		prev_ptn[1] = prev_ptn[0];
		prev_ptn[0].SetNull();

		// 手番を入れ替える
		// Exchange the turn indexes.
		my = int(my == 0);
		her = int(her == 0);

		return;
	}

	// 4. 確率更新フラグを初期化
	//    Initialize the updating flag of 3x3 pattern.
	FillArray(is_ptn_updated, false);
	updated_ptns.clear();

	// 5. responseパターンを更新し、石を置く
	//    Update response pattern and place a stone.
	UpdatePrevPtn(v);
	PlaceStone(v);

	// 6. 自石と結合
	//    Merge the stone with other Rens.
	int my_color = my + 2;
	forEach4Nbr(v, v_nbr1, {

		// a. 自石かつ異なるren_idxのとき
		//    When v_nbr1 is my stone color and another Ren.
		if(color[v_nbr1] == my_color && ren_idx[v_nbr1] != ren_idx[v]){

			// b. アタリになったとき、プレアタリを解消
			//    Cancel pre-Atari when it becomes in Atari.
			if(ren[ren_idx[v_nbr1]].lib_cnt == 1) CancelPreAtari(v_nbr1);

			// c. 石数が多い方をベースに結合
			//    Merge them with the larger size of Ren as the base.
			if (ren[ren_idx[v]].size > ren[ren_idx[v_nbr1]].size) {
				MergeRen(v, v_nbr1);
			}
			else MergeRen(v_nbr1, v);

		}

	});

	// 7. 敵石の呼吸点を減らす
	//    Reduce liberty of opponent's stones.
	int her_color = int(my == 0) + 2;
	std::vector<int> atari_rens;
	std::vector<int> patr_rens;
	std::vector<int> lib3_rens;

	forEach4Nbr(v, v_nbr2, {

		// 敵石のとき. If an opponent stone.
		if (color[v_nbr2] == her_color) {
			switch(ren[ren_idx[v_nbr2]].lib_cnt)
			{
			case 0:
				RemoveRen(v_nbr2);
				break;
			case 1:
				SetAtari(v_nbr2);
				atari_rens.push_back(ren_idx[v_nbr2]);
				break;
			case 2:
				SetPreAtari(v_nbr2);
				patr_rens.push_back(ren_idx[v_nbr2]);
				break;
			case 3:
				lib3_rens.push_back(ren_idx[v_nbr2]);
				break;
			default:
				break;
			}
		}

	});

	// 8. コウを更新
	//    Update Ko.
	if (is_in_eye && prev_empty_cnt == empty_cnt) {
		ko = empty[empty_cnt - 1];
	}

	// 9. 着手連のアタリor２呼吸点情報を更新
	//    Update Atari/pre-Atari of the Ren including v.
	switch(ren[ren_idx[v]].lib_cnt){
	case 1:
		SetAtari(v);
		if(ren[ren_idx[v]].lib_atr != ko){
			response_move[3] = ren[ren_idx[v]].lib_atr;
		}
		break;
	case 2:
		SetPreAtari(v);
		break;
	default:
		break;
	}

	// 10. アタリになった石を助ける手を更新
	//     Update response_move which saves stones in Atari.
	if(atari_rens.size() > 0){

		// a. 重複要素を削除
		//    Remove duplicated indexes.
		sort(atari_rens.begin(), atari_rens.end());
		atari_rens.erase(unique(atari_rens.begin(),atari_rens.end()),atari_rens.end());

		for(auto atr_idx: atari_rens){

			int v_atari = ren[atr_idx].lib_atr;
			int v_tmp = atr_idx;
			int max_stone_cnt = 0;

			// b. アタリにされた連に隣接する敵石が取れるか調べる
			//    Check whether it is possible to take neighboring stones of the Ren in Atari.
			do {
				forEach4Nbr(v_tmp, v_nbr3,{

					Ren* ren_nbr = &ren[ren_idx[v_nbr3]];

					if(color[v_nbr3] == my_color &&
						ren_nbr->IsAtari() &&
						ren_nbr->lib_atr != ko)
					{
						bool inner_cap = false;

						if(ren_nbr->size == 1){
							int cap_cnt = 0;
							int v_s = ren_nbr->lib_atr;
							for(int i=0;i<4;++i){
								if(ptn[v_s].IsAtari(i) && ptn[v_s].ColorAt(i) == my_color) ++cap_cnt;
							}
							if(cap_cnt <= 1 && v_s == ren[atr_idx].lib_atr) inner_cap = true;
						}

						if(!inner_cap && ren_nbr->size > max_stone_cnt){
							response_move[1] = ren_nbr->lib_atr;
							max_stone_cnt = ren_nbr->size;
						}
					}

				});
				v_tmp = next_ren_v[v_tmp];
			} while (v_tmp != atr_idx);

			// c. アタリから逃げれるか調べる
			//    Check whether it is possible to save stones by escaping.
			if(ptn[v_atari].EmptyCnt() == 2){
				if(ladder_ptn[her].find(ptn[v_atari].bf) == ladder_ptn[her].end()){
					response_move[2] = v_atari;
				}
			}
			else if(ptn[v_atari].EmptyCnt() > 2 || !IsSelfAtari(her, v_atari)){
				response_move[2] = v_atari;
			}
		}
	}

	// 10-1. 攻め合いの点を調べる.
#ifdef USE_SEMEAI
	if(patr_rens.size() > 0) Semeai2(patr_rens, semeai_move[0]);
	if(lib3_rens.size() > 0) Semeai3(lib3_rens, semeai_move[1]);
#endif

	// 11. パターンに変更があった地点の確率を更新
	//     Update probability on all vertexes where 3x3 patterns were changed.
	UpdateProbAll();

	// 12. レスポンスパターンの確率を更新
	//     Update probability of the response pattern.
	prev_ptn_prob = 0.0;
	if(prob_ptn_rsp.find(prev_ptn[0].bf) != prob_ptn_rsp.end()){
		auto add_prob = prob_ptn_rsp.at(prev_ptn[0].bf);
		prev_ptn_prob = add_prob[1];	//レスポンス状態から元に戻す確率パラメータ
		forEach12Nbr(v, v_nbr12, {
			AddProb(her, v_nbr12, add_prob[0]);
		});
	}

	// 13. その他の確率を更新
	//     Update probability based on distance and response moves.
	AddProbDist(v);

	if(response_move[1] != VNULL){
		AddProb(her, response_move[1], response_w[1][0]);
	}
	if(response_move[2] != VNULL){
		AddProb(her, response_move[2], response_w[2][0]);
	}
	if(response_move[3] != VNULL){
		AddProb(her, response_move[3], response_w[3][0]);
	}
#ifdef USE_SEMEAI
	if(semeai_move[0].size() > 0){
		for(auto sm: semeai_move[0]) AddProb(her, sm, semeai_w[0][0]);
	}
	if(semeai_move[1].size() > 0){
		for(auto sm: semeai_move[1]) AddProb(her, sm, semeai_w[1][0]);
	}
#endif

	// 14. 手番変更
	//     Exchange the turn indexes.
	prev_move[my] = v;
	my = int(my == 0);
	her = int(her == 0);

}


/**
 *  v周辺の12点パターンをprev_ptnに登録する
 *  Register 12-point pattern around v.
 */
inline void Board::UpdatePrevPtn(int v){

	// 1. prev_ptn[1]に12点パターン情報をコピー
	//    Copy the pattern to prev_ptn[1].
	prev_ptn[1] = prev_ptn[0];

	// 2. 3x3パターンから(2,0)位置の石情報を追加
	//    Add stones in position of the Manhattan distance = 4.
	prev_ptn[0] = ptn[v];
	prev_ptn[0].SetColor(8, color[std::min(v + EBSIZE * 2, EBVCNT - 1)]);
	prev_ptn[0].SetColor(9, color[v + 2]);
	prev_ptn[0].SetColor(10, color[std::max(v - EBSIZE * 2, 0)]);
	prev_ptn[0].SetColor(11, color[v - 2]);

	// 3. 黒番のとき石の色を反転させる
	//    Flip color if it is black's turn.
	if(my == 1) prev_ptn[0].FlipColor();

}

/**
 *  レスポンスパターンによる確率指標を解消する
 *  Restore probability of response pattern.
 */
inline void Board::SubPrevPtn(){

	if(prev_ptn_prob == 0.0 || prev_move[her] >= PASS) return;

	// 周囲12点の確率を元に戻す
	// Restore the probability of 12 surroundings.
	forEach12Nbr(prev_move[her], v_nbr, {
		AddProb(my, v_nbr, prev_ptn_prob);
	});

}

/**
 *  座標vのpl側の実確率をnew_probで置き換える
 *  Replace the probability on position v with new_prob.
 */
void Board::ReplaceProb(int pl, int v, double new_prob) {

	sum_prob_rank[pl][etoy[v] - 1] += new_prob - prob[pl][v];
	prob[pl][v] = new_prob;

}

/**
 *  座標vのpl側の確率パラメータにadd_probを加算する
 *  Multiply probability.
 */
inline void Board::AddProb(int pl, int v, double add_prob) {

	if(v >= PASS || prob[pl][v] == 0) return;

	sum_prob_rank[pl][etoy[v] - 1] += (add_prob - 1) * prob[pl][v];
	prob[pl][v] *= add_prob;

}


/**
 *  盤面全体で12点パターンと距離パラメータを更新する
 *  Update probability based on parameters of 12-point patterns and long distance
 *  on the all vertexes.
 */
void Board::AddProbPtn12() {

	for(int i=0;i<empty_cnt;++i){
		int v = empty[i];

		int prev_move_ = (move_cnt >= 3)? move_history[move_cnt - 3] : PASS;

		// 1. 距離パラメータを追加
		//    Add probability of long distance.
		AddProb(my, v, prob_dist[0][DistBetween(v, prev_move[her])][0]);
		AddProb(my, v, prob_dist[1][DistBetween(v, prev_move[my])][0]);
		AddProb(her, v, prob_dist[0][DistBetween(v, prev_move[my])][0]);
		AddProb(her, v, prob_dist[1][DistBetween(v, prev_move_)][0]);

		// 2. 12点パターンの確率を追加
		//    Add probability of 12-point patterns.
		Pattern3x3 tmp_ptn = ptn[v];
		tmp_ptn.SetColor(8, color[std::min(EBVCNT - 1, (v + EBSIZE * 2))]);
		tmp_ptn.SetColor(9, color[v + 2]);
		tmp_ptn.SetColor(10, color[std::max(0, (v - EBSIZE * 2))]);
		tmp_ptn.SetColor(11, color[v - 2]);
		if(prob_ptn12.find(tmp_ptn.bf) != prob_ptn12.end()){
			AddProb(my, v, prob_ptn12[tmp_ptn.bf][my]);
			AddProb(her, v, prob_ptn12[tmp_ptn.bf][her]);
		}
	}

}

/**
 *  盤面全体3x3でパターンを再計算する
 *  Recalculate 3x3-pattern probability on the all vertexes.
 */
void Board::RecalcProbAll(){

	for(int i=0;i<empty_cnt;++i){
		int v = empty[i];
		for(int j=0;j<2;++j){
			ReplaceProb(j, v, prob_dist_base[v] * ptn[v].GetProb3x3(j,false));
		}
	}

}

/**
 *  3x3パターンに変更があった点の確率を更新する
 *  Update probability on all vertexes where 3x3 patterns were changed.
 */
inline void Board::UpdateProbAll() {

	// 1. 3x3パターンに変更があった空点の確率を更新
	//    Update probability on empty vertexes where 3x3 pattern was changed.
	for (auto i: updated_ptns) {
		for (int j=0;j<2;++j) {
			int v = i.first;
			int bf = i.second;
			Pattern3x3 ptn_(bf);
			double ptn_prob = ptn[v].GetProb3x3(j,false);
			double prob_diff = ptn_.GetProb3x3(j,true) * ptn_prob;

			if(prob[j][v] == 0){
				ReplaceProb(j, v, prob_dist_base[v] * ptn_prob);
			}
			else{
				AddProb(j, v, prob_diff);
			}
		}
	}

	//2. 石が抜かれた地点の確率を更新
	//   prob_dist_baseはRemoveStoneで追加済
	//   Update probability on positions where a stone was removed.
	//   Probability of prob_dist_base has been already added in RemoveStone().
	for (auto i: removed_stones) {
		AddProb(my, i, ptn[i].GetProb3x3(my,false));
		AddProb(her, i, ptn[i].GetProb3x3(her,false));
	}

}

/**
 *  距離パラメータに基づく確率指標を解消する
 *  近傍8点のみ
 *  Subtract probability weight based on short distance.
 *  Neighbor 8 points.
 */
inline void Board::SubProbDist() {

	if (prev_move[her] < PASS) {
		forEach8Nbr(prev_move[her],v_nbr,d_nbr,d_opp,{
			AddProb(my, v_nbr, response_w[0][1]);
		});
	}

}

/**
 *  距離パラメータに基づく確率指標を追加する
 *  近傍8点のみ
 *  Add probability weight based on short distance.
 *  Neighbor 8 points.
 */
inline void Board::AddProbDist(int v) {

	forEach8Nbr(v,v_nbr,d_nbr,d_opp,{
		AddProb(her, v_nbr, response_w[0][0]);
	});

}

/**
 *  ランダムに合法手を選択・着手->着手を返す
 *  Select and play a legal move randomly.
 *  Return the selected move.
 */
int Board::SelectRandomMove() {

	int i0 = int(mt_double(mt_32) * empty_cnt);	 // [0, empty_cnt)
	int i = i0;
	int next_move = PASS;

	for (;;) {
		next_move = empty[i];

		// 眼を埋めずセキでもない合法手か
		// Break if next_move is legal and dosen't fill an eye and Seki.
		if ( !IsEyeShape(my, next_move) 	&&
			 IsLegal(my, next_move) 	&&
			 !IsSeki(next_move)			) break;

		++i;
		if(i == empty_cnt) i = 0;

		// 全ての空点を調べるまで繰り返す
		// Repeat until all empty vertexes are checked.
		if (i == i0) {
			next_move = PASS;
			break;
		}
	}

	PlayLegal(next_move);

	return next_move;

}

/**
 *  確率に基づき合法手を選択・着手->着手を返す
 *  Select and play a legal move based on probability distribution.
 *  Return the selected move.
 */
int Board::SelectMove() {

	int next_move = PASS;
	double rand_double = mt_double(mt_32); // [0.0, 1.0)
	double prob_rank[BSIZE];
	double prob_v[EBVCNT];
	int i, x, y;
	double rand_move, tmp_sum;

	// 1. 確率をコピー
	//    Copy probability.
	std::memcpy(prob_rank, sum_prob_rank[my], sizeof(prob_rank));
	std::memcpy(prob_v, prob[my], sizeof(prob_v));
	double total_sum = 0.0;
	for (auto rs: prob_rank) total_sum += rs;

	// 2. 確率分布から次の手を選択する
	//    Select next move based on probability distribution.
	for (i=0;i<empty_cnt;++i) {
		if (total_sum <= 0) break;
		rand_move = rand_double * total_sum;
		tmp_sum = 0;

		// a. y方向の確率値の合計がrand_moveを超える地点を調べる
		//    Find the rank where sum of prob_rank exceeds rand_move.
		for (y=0;y<BSIZE;++y) {
			tmp_sum += prob_rank[y];
			if (tmp_sum > rand_move) break;
		}
		tmp_sum -= prob_rank[y];
		assert(y < BSIZE);

		// b. x方向の確率値の合計がrand_moveを超える地点を調べる
		//    Find the position where sum of probability exceeds rand_move.
		next_move = xytoe[1][y + 1];
		for (x=0;x<BSIZE;++x) {
			tmp_sum += prob_v[next_move];
			if (tmp_sum > rand_move){
				break;
			}
			++next_move;
		}
		//assert(x < BSIZE);

		// c. 眼を埋めずセキでもない合法手か
		//    Break if next_move is legal and dosen't fill an eye or Seki.
		if (IsLegal(my, next_move) 		&&
			!IsEyeShape(my, next_move) 	&&
			!IsSeki(next_move)			) break;

		// d. 合法手でないとき、next_moveの確率を除いて再計算する
		//    Recalculate after subtracting probability of next_move.
		prob_rank[y] -= prob_v[next_move];
		total_sum -= prob_v[next_move];
		prob_v[next_move] = 0;
		next_move = PASS;
	}

	// 3. next_moveを着手
	//    Play next_move.
	PlayLegal(next_move);

	return next_move;

}


/**
 *  10手目でマネ碁されているか
 *  Return whether it is mimic go at the 10th move.
 */
bool Board::IsMimicGo(){

	if(	move_cnt != 10 ||
		move_history.size() != 10) return false;

	for(int i=0;i<10;i=i+2){
		if(move_history[i] == PASS || move_history[i+1] == PASS) return false;
		int x = EBSIZE - 1 - etox[move_history[i]];
		int y = EBSIZE - 1 - etoy[move_history[i]];
		int v_on_diag = xytoe[x][y];
		if(color[v_on_diag] != 2) return false;
	}

	return true;

}

#undef forEach4Nbr
#undef forEach4Diag
#undef forEach8Nbr
#undef forEach12Nbr
