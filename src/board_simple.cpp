#include <algorithm>
#include <assert.h>
#include <unordered_set>

#include "board_simple.h"

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


BoardSimple::BoardSimple() {

	Clear();

}


BoardSimple::BoardSimple(const Board& other) {

	*this = other;

}


BoardSimple& BoardSimple::operator=(const Board& other) {

	my = other.my;
	her = other.her;
	std::memcpy(color, other.color, sizeof(color));
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
	std::memcpy(ptn, other.ptn, sizeof(ptn));

	diff.clear();
	diff_cnt = 0;

	return *this;

}


BoardSimple& BoardSimple::operator=(const BoardSimple& other) {

	my = other.my;
	her = other.her;
	std::memcpy(color, other.color, sizeof(color));
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
	std::memcpy(ptn, other.ptn, sizeof(ptn));

	diff.clear();
	copy(other.diff.begin(), other.diff.end(), back_inserter(diff));
	diff_cnt = other.diff_cnt;

	return *this;

}

/**
 *  初期化
 *  Initialize the board.
 */
void BoardSimple::Clear() {

	// 黒番-> (my, her) = (1, 0), 白番 -> (0, 1).
	// if black's turn, (my, her) = (1, 0) else (0, 1).
	my = 1;
	her = 0;
	empty_cnt = 0;

	for (int i=0;i<EBVCNT;++i) {

		next_ren_v[i] = i;
		ren_idx[i] = i;

		ren[i].SetNull();

		int ex = etox[i];
		int ey = etoy[i];

		// outer baundary
		if (ex == 0 || ex == EBSIZE - 1 || ey == 0 || ey == EBSIZE - 1) {
			//空点->0, 盤外->1, 白石->2, 黒石->3
			color[i] = 1;
			empty_idx[i] = VNULL;	//442
			ptn[i].SetNull();		//0xffffffff
		}
		// real board
		else {
			// empty vertex
			color[i] = 0;
			empty_idx[i] = empty_cnt;
			empty[empty_cnt] = i;
			++empty_cnt;
		}

	}

	for(int i=0;i<BVCNT;++i){

		int v = rtoe[i];

		ptn[v].Clear();	//0x00000000

		// 周囲8点のcolorを登録する
		// Set colors around v.
		forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
			ptn[v].SetColor(d_nbr, color[v_nbr8]);
		});

	}

	stone_cnt[0] = stone_cnt[1] = 0;
	empty_cnt = BVCNT;
	ko = VNULL;
	move_cnt = 0;
	move_history.clear();

	diff.clear();
	diff_cnt = 0;

	memset(&color_ch, false, sizeof(color_ch));
	memset(&empty_ch, false, sizeof(empty_ch));
	memset(&empty_idx_ch, false, sizeof(empty_idx_ch));
	memset(&ren_ch, false, sizeof(ren_ch));
	memset(&next_ren_v_ch, false, sizeof(next_ren_v_ch));
	memset(&ren_idx_ch, false, sizeof(ren_idx_ch));
	memset(&ptn_ch, false, sizeof(ptn_ch));
}

/**
 *  座標vへの着手が合法手か
 *  Return whether pl's move on v is legal.
 */
bool BoardSimple::IsLegal(int pl, int v) const{

	assert(v <= PASS);

	if (v == PASS) return true;
	if (color[v] != 0 || v == ko) return false;

	return ptn[v].IsLegal(pl);

}

/**
 *  座標vがpl側の眼形か
 *  Return whether v is an eye shape for pl.
 */
bool BoardSimple::IsEyeShape(int pl, int v) const{

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
bool BoardSimple::IsFalseEye(int v) const{

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
bool BoardSimple::IsSeki(int v) const{

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
 *  座標vを含む連のアタリ地点を更新する
 *  Set Atari of the Ren including v.
 */
inline void BoardSimple::SetAtari(int v) {

	assert(color[v] > 1);

	int v_atari = ren[ren_idx[v]].lib_atr;

	if(!ptn_ch[v_atari]){
		diff[diff_cnt-1].ptn.push_back(std::make_pair(v_atari, ptn[v_atari].bf));
		ptn_ch[v_atari] = true;
	}

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
inline void BoardSimple::SetPreAtari(int v) {

	assert(color[v] > 1);

	int64 lib_bit = 0;
	for(int i=0;i<6;++i){
		lib_bit = ren[ren_idx[v]].lib_bits[i];
		while(lib_bit != 0){
			int ntz = NTZ(lib_bit);
			int v_patr = rtoe[ntz + i * 64];

			if(!ptn_ch[v_patr]){
				diff[diff_cnt-1].ptn.push_back(std::make_pair(v_patr, ptn[v_patr].bf));
				ptn_ch[v_patr] = true;
			}

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
inline void BoardSimple::CancelAtari(int v) {

	assert(color[v] > 1);

	int v_atari = ren[ren_idx[v]].lib_atr;

	if(!ptn_ch[v_atari]){
		diff[diff_cnt-1].ptn.push_back(std::make_pair(v_atari, ptn[v_atari].bf));
		ptn_ch[v_atari] = true;
	}

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
inline void BoardSimple::CancelPreAtari(int v) {

	assert(color[v] > 1);

	int64 lib_bit = 0;
	for(int i=0;i<6;++i){
		lib_bit = ren[ren_idx[v]].lib_bits[i];
		while(lib_bit != 0){
			int ntz = NTZ(lib_bit);
			int v_patr = rtoe[ntz + i * 64];

			if(!ptn_ch[v_patr]){
				diff[diff_cnt-1].ptn.push_back(std::make_pair(v_patr, ptn[v_patr].bf));
				ptn_ch[v_patr] = true;
			}

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
inline void BoardSimple::PlaceStone(int v) {

	assert(color[v] == 0);

	// 1. 周囲8点の3x3パターンを更新
	//    Update 3x3 patterns around v.
	diff[diff_cnt-1].color.push_back(v);
	color[v] = my + 2;

	forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
		if(!ptn_ch[v_nbr8]){
			diff[diff_cnt-1].ptn.push_back(std::make_pair(v_nbr8, ptn[v_nbr8].bf));
			ptn_ch[v_nbr8] = true;
		}
		ptn[v_nbr8].SetColor(d_opp, color[v]);
	});

	// 2. 石数、確率を更新
	//    Update stone number and probability at v.
	++stone_cnt[my];
	--empty_cnt;
	if(!empty_idx_ch[empty[empty_cnt]]){
		diff[diff_cnt-1].empty_idx.push_back(std::make_pair(empty[empty_cnt], empty_idx[empty[empty_cnt]]));
		empty_idx_ch[empty[empty_cnt]] = true;
	}
	empty_idx[empty[empty_cnt]] = empty_idx[v];
	if(!empty_ch[empty_idx[v]]){
		diff[diff_cnt-1].empty.push_back(std::make_pair(empty_idx[v], empty[empty_idx[v]]));
		empty_ch[empty_idx[v]] = true;
	}
	empty[empty_idx[v]] = empty[empty_cnt];

	// 3. vを含む連indexの更新
	//    Update Ren index including v.
	if(!ren_idx_ch[v]){
		diff[diff_cnt-1].ren_idx.push_back(std::make_pair(v, ren_idx[v]));
		ren_idx_ch[v] = true;
	}
	ren_idx[v] = v;
	if(!ren_ch[ren_idx[v]]){
		diff[diff_cnt-1].ren.push_back(std::make_pair(ren_idx[v], ren[ren_idx[v]]));
		ren_ch[ren_idx[v]] = true;
	}
	ren[ren_idx[v]].Clear();

	// 4. 周囲4点の呼吸点を更新
	//    Update liberty on neighboring positions.
	forEach4Nbr(v, v_nbr, {
		// 隣が空点のときは自分の呼吸点を増やす
		// Add liberty when v_nbr is empty.
		if (color[v_nbr] == 0){
			ren[ren_idx[v]].AddLib(v_nbr);
		}
		// それ以外は隣の連の呼吸点を減らす
		// Subtract liberty in other cases.
		else{
			if(!ren_ch[ren_idx[v_nbr]]){
				diff[diff_cnt-1].ren.push_back(std::make_pair(ren_idx[v_nbr], ren[ren_idx[v_nbr]]));
				ren_ch[ren_idx[v_nbr]] = true;
			}
			ren[ren_idx[v_nbr]].SubLib(v);
		}
	});

}

/**
 *  座標vの石を消去
 *  Remove the stone on the position v.
 */
inline void BoardSimple::RemoveStone(int v) {

	assert(color[v] > 1);

	// 1. 周囲8点の3x3パターンを更新
	//    Update 3x3 patterns around v.
	diff[diff_cnt-1].color.push_back(v);
	color[v] = 0;
	if(!ptn_ch[v]){
		diff[diff_cnt-1].ptn.push_back(std::make_pair(v, ptn[v].bf));
		ptn_ch[v] = true;
	}
	ptn[v].ClearAtari();
	ptn[v].ClearPreAtari();
	forEach8Nbr(v, v_nbr8, d_nbr, d_opp, {
		if(!ptn_ch[v_nbr8]){
			diff[diff_cnt-1].ptn.push_back(std::make_pair(v_nbr8, ptn[v_nbr8].bf));
			ptn_ch[v_nbr8] = true;
		}
		ptn[v_nbr8].SetColor(d_opp, 0);
	});

	// 2. 石数、確率を更新
	//    Update stone number and probability at v.
	--stone_cnt[her];
	if(!empty_idx_ch[v]){
		diff[diff_cnt-1].empty_idx.push_back(std::make_pair(v, empty_idx[v]));
		empty_idx_ch[v] = true;
	}
	empty_idx[v] = empty_cnt;
	if(!empty_ch[empty_cnt]){
		diff[diff_cnt-1].empty.push_back(std::make_pair(empty_cnt, empty[empty_cnt]));
		empty_ch[empty_cnt] = true;
	}
	empty[empty_cnt] = v;
	++empty_cnt;
	if(!ren_idx_ch[v]){
		diff[diff_cnt-1].ren_idx.push_back(std::make_pair(v, ren_idx[v]));
		ren_idx_ch[v] = true;
	}
	ren_idx[v] = v;

}

/**
 *  座標v_base、v_addを含む連を結合
 *  v_addを含む連のindexを書き換える
 *  Merge the Rens including v_base and v_add.
 *  Replace index of the Ren including v_add.
 */
inline void BoardSimple::MergeRen(int v_base, int v_add) {

	// 1. Renクラスの結合
	//    Merge of Ren class.
	if(!ren_ch[ren_idx[v_base]]){
		diff[diff_cnt-1].ren.push_back(std::make_pair(ren_idx[v_base], ren[ren_idx[v_base]]));
		ren_ch[ren_idx[v_base]] = true;
	}
	if(!ren_ch[ren_idx[v_add]]){
		diff[diff_cnt-1].ren.push_back(std::make_pair(ren_idx[v_add], ren[ren_idx[v_add]]));
		ren_ch[ren_idx[v_add]] = true;
	}
	ren[ren_idx[v_base]].Merge(ren[ren_idx[v_add]]);

	// 2. ren_idxをv_baseに書き換える
	//    Replace ren_idx of the Ren including v_add.
	int v_tmp = v_add;
	do {
		if(!ren_idx_ch[v_tmp]){
			diff[diff_cnt-1].ren_idx.push_back(std::make_pair(v_tmp, ren_idx[v_tmp]));
			ren_idx_ch[v_tmp] = true;
		}
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
	if(!next_ren_v_ch[v_base]){
		diff[diff_cnt-1].next_ren_v.push_back(std::make_pair(v_base, next_ren_v[v_base]));
		next_ren_v_ch[v_base] = true;
	}
	if(!next_ren_v_ch[v_add]){
		diff[diff_cnt-1].next_ren_v.push_back(std::make_pair(v_add, next_ren_v[v_add]));
		next_ren_v_ch[v_add] = true;
	}
	std::swap(next_ren_v[v_base], next_ren_v[v_add]);

}

/**
 *  座標vを含む連を消去
 *  Remove the Ren including v.
 */
inline void BoardSimple::RemoveRen(int v) {

	// 1. すべての石を消去
	//    Remove all stones of the Ren.
	int v_tmp = v;
	do {
		RemoveStone(v_tmp);
		v_tmp = next_ren_v[v_tmp];
	} while (v_tmp != v);

	// 2. 隣接する連の呼吸点情報を更新
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
			if(!ren_ch[ren_idx[v_nbr]]){
				diff[diff_cnt-1].ren.push_back(std::make_pair(ren_idx[v_nbr], ren[ren_idx[v_nbr]]));
				ren_ch[ren_idx[v_nbr]] = true;
			}
			ren[ren_idx[v_nbr]].AddLib(v_tmp);
		});

		int v_next = next_ren_v[v_tmp];
		if(!next_ren_v_ch[v_tmp]){
			diff[diff_cnt-1].next_ren_v.push_back(std::make_pair(v_tmp, next_ren_v[v_tmp]));
			next_ren_v_ch[v_tmp] = true;
		}
		next_ren_v[v_tmp] = v_tmp;
		v_tmp = v_next;
	}while (v_tmp != v);

	// 3. 隣接する連の呼吸点情報を更新
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
 *  Returns whether move on position v is self-Atari and forms Nakade.
 *  Used for checking whether Hourikomi is effective in Seki.
 */
inline bool BoardSimple::IsSelfAtariNakade(int v) const{

	// 周囲4点の連が大きさ2～4のとき、ナカデになるかを調べる
	// Check whether it will be Nakade shape when size of urrounding Ren is 2~4.
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

/**
 *  座標vに着手する
 *  着手が合法手かは事前に評価しておく必要がある.
 *  Update the board with the move on position v.
 *  It is necessary to confirm in advance whether the move is legal.
 */
void BoardSimple::PlayLegal(int v) {

	assert(v <= PASS);
	assert(v == PASS || color[v] == 0);
	assert(v != ko);

	// 1.  差分情報を初期化
	//     Initialize difference information.
	memset(&color_ch, false, sizeof(color_ch));
	memset(&empty_ch, false, sizeof(empty_ch));
	memset(&empty_idx_ch, false, sizeof(empty_idx_ch));
	memset(&ren_ch, false, sizeof(ren_ch));
	memset(&next_ren_v_ch, false, sizeof(next_ren_v_ch));
	memset(&ren_idx_ch, false, sizeof(ren_idx_ch));
	memset(&ptn_ch, false, sizeof(ptn_ch));
	Diff df;
	df.ko = ko;
	diff.push_back(df);
	++diff_cnt;

	// 2. 棋譜情報を更新
	//    Update history.
	int prev_empty_cnt = empty_cnt;
	bool is_in_eye = ptn[v].IsEnclosed(her);
	ko = VNULL;
	move_history.push_back(v);
	++move_cnt;

	if (v == PASS) {
		// 手番を入れ替える
		// Exchange the turn indexes.
		my = int(my == 0);
		her = int(her == 0);
		return;
	}

	// 3. 石を置く
	//    Place a stone.
	PlaceStone(v);

	// 4. 自石と結合
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

	// 5. 敵石の呼吸点を減らす
	//    Reduce liberty of opponent's stones.
	int her_color = int(my == 0) + 2;

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
				break;
			case 2:
				SetPreAtari(v_nbr2);
				break;
			default:
				break;
			}
		}

	});

	// 6. コウを更新
	//    Update Ko.
	if (is_in_eye && prev_empty_cnt == empty_cnt) {
		ko = empty[empty_cnt - 1];
	}

	// 7. 着手連のアタリor２呼吸点情報を更新
	//    Update Atari/pre-Atari of the Ren including v.
	switch(ren[ren_idx[v]].lib_cnt){
	case 1:
		SetAtari(v);
		break;
	case 2:
		SetPreAtari(v);
		break;
	default:
		break;
	}

	// 8. 手番変更
	//     Exchange the turn indexes.
	my = int(my == 0);
	her = int(her == 0);

}

/**
 *  局面を1手戻す
 *  Return the board phase to the previous state.
 */
void BoardSimple::Undo() {

	Diff* df = &diff[diff_cnt-1];
	for(auto& v: df->color){
		if(color[v] == 0){
			// 自石が取られた
			// Position where the stone was removed.
			color[v] = my + 2;
			--empty_cnt;
			++stone_cnt[my];
		}
		else{
			// 石を置いた
			// Position where the stone was placed.
			color[v] = 0;
			++empty_cnt;
			--stone_cnt[her];
		}
	}
	ko = df->ko;

	for(auto& em: df->empty){
		empty[em.first] = em.second;
	}
	for(auto& ei: df->empty_idx){
		empty_idx[ei.first] = ei.second;
	}
	for(auto& r: df->ren){
		ren[r.first] = r.second;
	}
	for(auto& nrv: df->next_ren_v){
		next_ren_v[nrv.first] = nrv.second;
	}
	for(auto& ri: df->ren_idx){
		ren_idx[ri.first] = ri.second;
	}
	for(auto& pt: df->ptn){
		ptn[pt.first].bf = pt.second;
	}

	my = int(my == 0);
	her = int(her == 0);
	move_history.pop_back();
	--move_cnt;

	diff.pop_back();
	--diff_cnt;

}


#undef forEach4Nbr
#undef forEach4Diag
#undef forEach8Nbr
