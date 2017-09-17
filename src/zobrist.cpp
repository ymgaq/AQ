#include "zobrist.h"
#include "board.h"

std::random_device rd;
std::mt19937 mt_32(rd());
std::mt19937_64 mt_64(rd());

/**
 * [0.0, 1.0)の一様乱数生成器
 * double rand_d = mt_double(mt_32); のように使う.
 *
 * uniform real distribution of [0.0, 1.0).
 * Ex. double rand_d = mt_double(mt_32);
 */
std::uniform_real_distribution<double> mt_double(0.0, 1.0);

/**
 * [0, 8)の一様乱数生成器
 * int rand_i = mt_int8(mt_32) のように使う.
 *
 * uniform int distribution of [0, 8).
 * Ex. int rand_i = mt_int8(mt_32)
 */
std::uniform_int_distribution<int> mt_int8(0, 7);


/**
 *  64bit整数のハッシュ値を生成する
 *  Generate 64-bit hash value of the board.
 */
int64 BoardHash(Board& b) {

	// 盤面のzobristハッシュ値
	// Zobrist hash value of the board.
	int64 b_hash = 0;

	// 石をXOR
	// XOR with stone hashes.
	for (auto i: rtoe) {
		switch (b.color[i]) {
			case 2:	b_hash ^= zobrist.hash[0][2][i]; break;
			case 3: b_hash ^= zobrist.hash[0][3][i]; break;
			default:	break;
		}
	}

	// コウをXOR
	// XOR with Ko position.
	if (b.ko != VNULL) b_hash ^= zobrist.hash[0][1][b.ko];

	// 手番をXOR. 黒番のとき ^1
	// XOR with turn.
	// black: b_hash ^= 1, white: b_hash ^= 0
	b_hash ^= b.my;

	return b_hash;

}


/**
 *  ハッシュ値を更新する
 *  Update 64-bit hash of board from the previous hash.
 */
int64 UpdateBoardHash(Board& b, int64 prev_hash) {

	// 盤面のzobristハッシュ値
	// Zobrist hash value of the board.
	int64 b_hash = prev_hash;

	int my = b.my;
	int her = b.her;
	int prev_move = b.prev_move[her];

	if (prev_move < PASS) {
		// 着手を更新
		// Update hash of the previous move.
		b_hash ^= zobrist.hash[0][her + 2][prev_move];

		// 取られた石を更新
		// Update hashes of removed stones.
		for(auto i: b.removed_stones){
			b_hash ^= zobrist.hash[0][my + 2][i];
		}

		// コウを更新
		// Update hash of Ko.
		if(b.prev_ko != VNULL) b_hash ^= zobrist.hash[0][1][b.prev_ko];
		if (b.ko != VNULL) b_hash ^= zobrist.hash[0][1][b.ko];
	}

	// 手番を更新
	// Update turn.
	b_hash ^= 1;

	return b_hash;

}
