#pragma once

#include <cstring>
#include <vector>
#include <nmmintrin.h>

#include "board_config.h"
#include "distance.h"
#include "pattern3x3.h"
#include "zobrist.h"


/**
 *  符号なし64bit整数のNTZ（右から続く0の個数）を求める関数
 *  Function for finding an unsigned 64-integer NTZ.
 *  Ex. 0x01011000 -> 3
 *      0x0 -> 64
 */
constexpr auto magic64 = 0x03F0A933ADCBD8D1ULL;
constexpr int ntz_table64[127] = {
    64,  0, -1,  1, -1, 12, -1,  2, 60, -1, 13, -1, -1, 53, -1,  3,
    61, -1, -1, 21, -1, 14, -1, 42, -1, 24, 54, -1, -1, 28, -1,  4,
    62, -1, 58, -1, 19, -1, 22, -1, -1, 17, 15, -1, -1, 33, -1, 43,
    -1, 50, -1, 25, 55, -1, -1, 35, -1, 38, 29, -1, -1, 45, -1,  5,
    63, -1, 11, -1, 59, -1, 52, -1, -1, 20, -1, 41, 23, -1, 27, -1,
    -1, 57, 18, -1, 16, -1, 32, -1, 49, -1, -1, 34, 37, -1, 44, -1,
    -1, 10, -1, 51, -1, 40, -1, 26, 56, -1, -1, 31, 48, -1, 36, -1,
     9, -1, 39, -1, -1, 30, 47, -1,  8, -1, -1, 46,  7, -1,  6,
};

inline constexpr int NTZ(int64 x) noexcept {
    return ntz_table64[static_cast<int64>(magic64*static_cast<int64>(x&-x))>>57];
}


/**************************************************************
 *
 *  「連」のクラス
 *
 *  隣接している石同士は一つの連を形成する.
 *  連に隣接する空点を「呼吸点」と呼び、
 *  呼吸点が0になるとその連は取られる.
 *
 *  呼吸点の座標は実盤面(19x19)のビットボード(64bit整数x6)で保持し、
 *  隣接する座標で石が置かれる/除かれる場合に呼吸点を増減する.
 *
 *
 *  Class of Ren.
 *
 *  Adjacent stones form a single stone string (=Ren).
 *  Neighboring empty vertexes are call as 'liberty',
 *  and the Ren is captured when all liberty is filled.
 *
 *  Positions of the liberty are held in a bitboard
 *  (64-bit integer x 6), which covers the real board (19x19).
 *  The liberty number decrease (increase) when a stone is
 *  placed on (removed from) a neighboring position.
 *
 ***************************************************************/
class Ren {
public:

	// 呼吸点座標のビットボード
	// Bitboard of liberty positions.
	// [0] -> 0-63, [1] -> 64-127, ..., [5] -> 320-360
	int64 lib_bits[6];

	int lib_atr; 	// アタリの場合の呼吸点座標. The liberty position in case of Atari.
	int lib_cnt;	// 呼吸点数. Number of liberty.
	int size;		// 連を構成する石数. Number of stones.

	Ren(){ Clear(); }
	Ren(const Ren& other){ *this = other; }
	Ren& operator=(const Ren& other){
		lib_cnt = other.lib_cnt;
		size 	= other.size;
		lib_atr = other.lib_atr;
		std::memcpy(lib_bits, other.lib_bits, sizeof(lib_bits));

		return *this;
	}

	// 石のある地点の初期化
	// Initialize for stone.
	void Clear(){
		lib_cnt 	= 0;
		size 		= 1;
		lib_atr 	= VNULL;
		lib_bits[0] = lib_bits[1] = lib_bits[2] = \
		lib_bits[3] = lib_bits[4] = lib_bits[5] = 0;
	}

	// 石のない地点の初期化（無効値）
	// Initialize for empty and outer boundary.
	void SetNull(){
		lib_cnt 	= VNULL; //442
		size 		= VNULL;
		lib_atr 	= VNULL;
		lib_bits[0] = lib_bits[1] = lib_bits[2] = \
		lib_bits[3] = lib_bits[4] = lib_bits[5] = 0;
	}

	// 座標vに呼吸点を追加する
	// Add liberty at v.
	void AddLib(int v){
		if(size == VNULL) return;

		int bit_idx = etor[v] / 64;
		int64 bit_v = (0x1ULL << (etor[v] % 64));

		if(lib_bits[bit_idx] & bit_v) return;
		lib_bits[bit_idx] |= bit_v;
		++lib_cnt;
		lib_atr = v;
	}

	// 座標vの呼吸点を消去する
	// Delete liberty at v.
	void SubLib(int v){
		if(size == VNULL) return;

		int bit_idx = etor[v] / 64;
		int64 bit_v = (0x1ULL << (etor[v] % 64));
		if(lib_bits[bit_idx] & bit_v){
			lib_bits[bit_idx] ^= bit_v;
			--lib_cnt;

			if(lib_cnt == 1){
				for(int i=0;i<6;++i){
					if(lib_bits[i] != 0){
						lib_atr = rtoe[NTZ(lib_bits[i]) + i * 64];
					}
				}
			}
		}
	}

	// 別の連otherと連結する
	// Merge with another Ren.
	void Merge(const Ren& other){
		lib_cnt = 0;
		for(int i=0;i<6;++i){
			lib_bits[i] |= other.lib_bits[i];
			if(lib_bits[i] != 0){
				lib_cnt += (int)_mm_popcnt_u64(lib_bits[i]);
			}
		}
		if(lib_cnt == 1){
			for(int i=0;i<6;++i){
				if(lib_bits[i] != 0){
					lib_atr = rtoe[NTZ(lib_bits[i]) + i * 64];
				}
			}
		}
		size += other.size;
	}

	// 呼吸点数が0か
	// Return whether this Ren is captured.
	bool IsCaptured() const{ return lib_cnt == 0; }

	// 呼吸点数が1か
	// Return whether this Ren is Atari.
	bool IsAtari() const{ return lib_cnt == 1; }

	// 呼吸点数が2か
	// Return whether this Ren is pre-Atari.
	bool IsPreAtari() const{ return lib_cnt == 2; }

	// アタリの連の呼吸点を返す
	// Return the liberty position of Ren in Atari.
	int GetAtari() const{ return lib_atr; }

};


/*********************************************************************************
 *
 *  盤面のクラス
 *
 *  盤面のデータ構造は、盤外を含めた21x21=441点の一次元座標系で表現される.
 *  ある座標vは「石の種類」「3x3パターン」「連」「同一連の次の座標」の情報を持つ.
 *
 *  1. 石の種類 (例 color[v])
 *     座標vの石/空点/盤外の種類.
 *     空点->0, 盤外->1, 白->2, 黒->3
 *
 *  2. 3x3パターン (例 ptn[v])
 *     座標vを含む周囲3x3の石・呼吸点情報を持つPattern3x3クラス.
 *     合法手判定等をビット演算で高速に計算するために用いる.
 *
 *  3. 連（例 ren[ren_idx[v]]）
 *     座標vの連番号はren_idx[v]に格納される. 連番号は、連がmergeされるとき
 *     どちらかの番号に統一される. (初期値は ren_idx[v] = v)
 *     Renクラスは連の石数、呼吸点の管理に用いられる.
 *
 *  4. 同一連の次の座標（例 next_ren_v[v]）
 *     連の周囲の操作（隣接する3x3パターン更新など）に用いられる.
 *     次の座標情報は循環するので、どの点から参照してもよい.(v0->v1->...->v7->v0)
 *     サイズ1の連では　next_ren_v[v] = v　となる.
 *
 *
 *  Class of board.
 *
 *  The data structure of the board is treated as the 1D-coordinate system of 441 points.
 *  A position 'v' has information of "stone color", "3x3 pattern", "Ren" and
 *  "next position of its Ren".
 *
 *  1. stone color: color[v]
 *     empty->0, outer boundary->1, white->2, black->3.
 *
 *  2. 3x3 pattern: ptn[v]
 *     Pattern3x3 class which has information of surrounding stones and liberty.
 *     This is used to calculate legal judgment etc quickly by bit operation.
 *
 *  3. Ren: ren[ren_idx[v]]
 *     The ren index at v is stored in ren_idx[v]. When Rens are merged,
 *     their ren indexes are unified to either index.
 *     Ren class is used for managing stone counts and liberty.
 *
 *  4. next position of its Ren: next_ren_v[v]
 *     This is used for operation around the ren (3x3 pattern update etc.).
 *     Since position information circulates, it can be referred to from any
 *     vertex. (Ex. v0->v1->...->v7->v0)
 *     For a ren of size 1, next_ren_v[v] = v.
 *
 *********************************************************************************/
class Board {
private:
	// 3x3パターンの変更フラグ
	// Flag indicating whether 3x3 pattern has been updated.
	bool is_ptn_updated[EBVCNT];

	// 変更された3x3のパターンの座標　(座標、元のbf値)
	// List of (position, previous value of bf) of updated patterns.
	std::vector<std::pair<int,int>> updated_ptns;

	void AddUpdatedPtn(int v);
	void SetAtari(int v);
	void SetPreAtari(int v);
	void CancelAtari(int v);
	void CancelPreAtari(int v);
	void PlaceStone(int v);
	void RemoveStone(int v);
	void MergeRen(int v_base, int v_add);
	void RemoveRen(int v);
	bool IsSelfAtariNakade(int v) const;
	void UpdatePrevPtn(int v);
	void SubPrevPtn();
	void AddProbWeight(int pl, int v, double add_w_prob);
	void UpdateProbAll();
	void AddProbDist(int v);
	void SubProbDist();

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
	// Empty vertex index of each position.
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

	// [0]: 白の直前の着手　[1]: 黒の直前の着手
	// [0]: white's previous move [1]: black's previous move.
	int prev_move[2];

	// Previous position of illegal move of Ko.
	int prev_ko;

	// 今の着手で取られた石の座標.
	// List of stones removed in the current move.
	std::vector<int> removed_stones;

	// 各点の実確率
	// Probability of each vertex.
	double prob[2][EBVCNT];

	// Flag indicating whether a stone has been placed.
	bool is_placed[2][EBVCNT];

	// 3x3 patterns.
	Pattern3x3 ptn[EBVCNT];

	// 直前・2手前の石を置く前の12点パターン
	// Twelve-point patterns around last and two moves before moves.
	Pattern3x3 prev_ptn[2];

	// 直前に更新したレスポンスパターンの確率値
	double prev_ptn_prob;

	// ナカデやアタリを逃げる手など、高い確率がつきやすい手
	// Reflex move, such as Nakade or save stones in Atari.
	int response_move[4];

	// パスをした回数. (日本ルール用)
	// Number of pass. (for Japanese rule)
	int pass_cnt[2];
	
	// 確率パラメータ. Sum of probability parameters for each turn/vertex.
	double w_prob[2][EBVCNT];

	// 行ごとの確率の小計. Sum of probability for each rank.
	double sum_prob_rank[2][BSIZE];

	Board();
	Board(const Board& other);
	Board& operator=(const Board& other);
	void Clear();
	bool IsLegal(int pl, int v) const;
	bool IsEyeShape(int pl, int v) const;
	bool IsFalseEye(int v) const;
	bool IsSeki(int v) const;
	void PlayLegal(int v);
	void ReplaceProb(int pl, int v, double new_prob);
	void ReplaceProbWeight(int pl, int v, double new_w_prob);
	void RecalcProbAll();
	void AddProbPtn12();
	int SelectRandomMove();
	int SelectMove();
	bool IsMimicGo();

};

