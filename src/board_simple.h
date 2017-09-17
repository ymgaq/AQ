#pragma once

#include <vector>

#include "board_config.h"
#include "distance.h"
#include "pattern3x3.h"
#include "zobrist.h"
#include "board.h"


/**************************************************************
 *
 *  変化した盤面情報を保管するクラス
 *  指標または（指標、元の値）を入力する
 *
 *  Class which stores difference of board from previous status.
 *  Inputs (index) or (index, original value).
 *
 ***************************************************************/
struct Diff {
	// 石の変化した座標
	// Positions where color changes.
	std::vector<int> color;

	// 空点の座標配列の変化した指標、元の値
	// (index, original value).
	std::vector<std::pair<int,int>> empty;

	// 各座標の空点配列指標の変化した指標、元の値
	// (index, original value).
	std::vector<std::pair<int,int>> empty_idx;

	// 元のコウ座標. Previous position of Ko.
	int ko;

	// 連指標に対応する連の変化した指標、元の値
	// (index, original value).
	std::vector<std::pair<int,Ren>> ren;

	// 同じ連に該当する次の座標の変化した指標、元の値
	// (index, original value).
	std::vector<std::pair<int,int>> next_ren_v;

	// 連指標の変化した指標、元の値
	// (index, original value).
	std::vector<std::pair<int,int>> ren_idx;

	// パターン情報の変化した指標、元のbit field
	// (index, original bf).
	std::vector<std::pair<int,int>> ptn;
};


/**************************************************************
 *
 *  高速盤面クラス
 *
 *  前の手の状態から差分を保持しておき、再帰的に1手前に戻せる.
 *  シチョウ探索など、確率を必要としない場合に使用する.
 *
 *  Class of quick board.
 *
 *  since storing difference from the previous board,
 *  it is possible to undo recursively.
 *  This class is used when the probability is not needed,
 *  such as the ladder search.
 *
 ***************************************************************/
class BoardSimple {
private:

	bool color_ch[EBVCNT];
	bool empty_ch[BVCNT];
	bool empty_idx_ch[EBVCNT];
	bool ren_ch[EBVCNT];
	bool next_ren_v_ch[EBVCNT];
	bool ren_idx_ch[EBVCNT];
	bool ptn_ch[EBVCNT];

	void SetAtari(int v);
	void SetPreAtari(int v);
	void CancelAtari(int v);
	void CancelPreAtari(int v);
	void PlaceStone(int v);
	void RemoveStone(int v);
	void MergeRen(int v_base, int v_add);
	void RemoveRen(int v);
	bool IsSelfAtariNakade(int v) const;

public:

	// 手番指標
	// Turn index. (0: white, 1: black)
	// if black's turn, (my, her) = (1, 0) else (0, 1).
	int my, her;

	// 座標の状態　空点->0　盤外->1　白->2　黒->3
	// Stone color.
	// empty->0, outer boundary->1, white->2, black->3
	int color[EBVCNT];

	// 空点の配列. [0, empty_cnt-1]の範囲で空点の座標を格納する
	// List of empty vertexes, containing their positions in range of [0, empty_cnt-1].
	// Ex. for(int i=0;i<empty_cnt;++i) v = empty[i]; ...
	int empty[BVCNT];

	// 各点における空点番号.
	// empty_idx[v] < empty_cnt ならば vは空点.
	// Empty-vertex index of each position.
	// if empty_idx[v] < empty_cnt, v is empty.
	int empty_idx[EBVCNT];


	// [0]: 白石の数　[1]: 黒石の数
	// [0]: number of white stones  [1]: number of black stones.
	int stone_cnt[2];

	// Number of empty vertexes.
	int empty_cnt;

	// コウの着手禁止点の座標
	// Position of the illegal move of Ko.
	int ko;

	// 連指標. Ren index.
	int ren_idx[EBVCNT];

	// 連指標に対応する連
	// Ren corresponding to the ren index.
	// Ex. ren[ren_idx[v]]
	Ren ren[EBVCNT];

	// 同じ連に該当する次の座標
	// Next position of another stone in the Ren.
	int next_ren_v[EBVCNT];

	// 手数. Number of the moves.
	int move_cnt;

	// 手順. History of the moves.
	std::vector<int> move_history;

	// 3x3 patterns.
	Pattern3x3 ptn[EBVCNT];

	// 差分情報
	// Information of Difference from initial status.
	std::vector<Diff> diff;

	// 差分情報の記録された手数
	// Number of stored Diff.
	int diff_cnt;
	
	BoardSimple();
	BoardSimple(const Board& other);
	BoardSimple& operator=(const Board& other);
	BoardSimple& operator=(const BoardSimple& other);
	void Clear();
	bool IsLegal(int pl, int v) const;
	bool IsEyeShape(int pl, int v) const;
	bool IsFalseEye(int v) const;
	bool IsSeki(int v) const;
	void PlayLegal(int v);
	void Undo();

};

