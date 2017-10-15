#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include "board.h"
#include "print.h"

// ハッシュ関数の特殊化
// Specialization of hash function.
namespace std {
	template<typename T>
	struct hash<array<T, 4>> {
		size_t operator()(const array<T, 4>& p) const {
			size_t seed = 0;
			hash<T> h;
			seed ^= h(p[0]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= h(p[1]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= h(p[2]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= h(p[3]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};
}


struct Statistics{
	int game[3];
	int stone[3][EBVCNT];
	int owner[2][EBVCNT];

	Statistics(){ Clear(); }
	Statistics(const Statistics& other){ *this = other; }

	void Clear(){
		for(auto& g:game) g = 0;
		for(auto& si:stone) for(auto& s:si) s = 0;
		for(auto& oi:owner) for(auto& o:oi) o = 0;
	}

	Statistics& operator=(const Statistics& rhs){
		std::memcpy(game, rhs.game, sizeof(game));
		std::memcpy(stone, rhs.stone, sizeof(stone));
		std::memcpy(owner, rhs.owner, sizeof(owner));
		return *this;
	}

	Statistics& operator+=(const Statistics& rhs){
		for(int i=0;i<3;++i) game[i] += rhs.game[i];
		for(int i=0;i<3;++i){
			for(int j=0;j<EBVCNT;++j) stone[i][j] += rhs.stone[i][j];
		}
		for(int i=0;i<2;++i){
			for(int j=0;j<EBVCNT;++j) owner[i][j] += rhs.owner[i][j];
		}
		return *this;
	}

	Statistics& operator-=(const Statistics& rhs){
		for(int i=0;i<3;++i) game[i] -= rhs.game[i];
		for(int i=0;i<3;++i){
			for(int j=0;j<EBVCNT;++j) stone[i][j] -= rhs.stone[i][j];
		}
		for(int i=0;i<2;++i){
			for(int j=0;j<EBVCNT;++j) owner[i][j] -= rhs.owner[i][j];
		}
		return *this;
	}
};


struct LGR{

	// PolicyNetによって得られた最善手を保持する
	// Container that has LGR moves obtained by PolicyNet.
	//     key: { previous 12-point pattern, previous move, ... }
	//     value: best move
	std::array<std::unordered_map<std::array<int,4>, int>, 2> policy;

	// プレイアウトによって得られた手を保持する配列
	// Array that has LGR moves obtained by rollout.
	std::array<std::array<std::array<int, EBVCNT>, EBVCNT>, 2> rollout;

	LGR(){ Clear(); }
	LGR(const LGR& other){ *this = other; }

	void Clear(){
		for(auto& p1:policy) p1.clear();
		for(auto& r1:rollout) for(auto& r2:r1) for(auto& r3:r2){ r3 = VNULL; }
	}

	LGR& operator=(const LGR& rhs){
		policy[0] = rhs.policy[0];
		policy[1] = rhs.policy[1];
		for(int i=0;i<2;++i){
			for(int j=0;j<EBVCNT;++j){
				for(int k=0;k<EBVCNT;++k){
					rollout[i][j][k] = rhs.rollout[i][j][k];
				}
			}
		}
		return *this;
	}

	LGR& operator+=(const LGR& rhs){
		policy[0].insert(rhs.policy[0].begin(),rhs.policy[0].end());
		policy[1].insert(rhs.policy[1].begin(),rhs.policy[1].end());
		for(int i=0;i<2;++i){
			for(int j=0;j<EBVCNT;++j){
				for(int k=0;k<EBVCNT;++k){
					if(rhs.rollout[i][j][k] != VNULL)
						rollout[i][j][k] = rhs.rollout[i][j][k];
				}
			}
		}
		return *this;
	}
};


int Win(Board& b, int pl, double komi=KOMI);
int Win(Board& b, int pl, Statistics& stat, double komi=KOMI);

double Score(Board& b, double komi=KOMI);
int Playout(Board& b, double komi=KOMI);
int PlayoutRandom(Board&b, double komi=KOMI);

int PlayoutLGR(Board& b, LGR& lgr, double komi=KOMI);
int PlayoutLGR(Board& b, LGR& lgr, Statistics& stat, double komi=KOMI);

extern bool japanese_rule;
