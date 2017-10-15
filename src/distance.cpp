#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "distance.h"

using std::cerr;
using std::endl;


/**
 *  距離パラメータ
 *  Distance parameters.
 *
 *  axis=0: from [0]previous or [1]two moves before current one
 *  axis=1: distance
 *          [0]->(0,0), [1]->(1,0), ..., [16]->PASS/VNULL
 *  axis=2: [0]->value [1]->1/value (for restoring probability)
 */
std::array<std::array<std::array<double, 2>, 17>, 2> prob_dist;

/**
 *  盤端からの距離に基づく座標ごとの距離パラメータ(初期盤面)
 *  Distance parameter at each position for initial board.
 */
std::array<double, EBVCNT> prob_dist_base;


/**
 *  2地点間の距離を求める
 *  マンハッタン距離-1を返す
 *
 *  Calculate distance between two points.
 *  Return the Manhattan distance - 1.
 */
int DistBetween(int v1, int v2){

	// PASSが含まれる場合最大値（16）を返す
	// Return the maximum value (=16) if v1 or v2 is PASS/VNULL.
	if (v1 >= PASS || v2 >= PASS) return 16;

	int dx = std::abs(etox[v1] - etox[v2]);
	int dy = std::abs(etoy[v1] - etoy[v2]);

	// マンハッタン距離 - 1 = dx + dy + max(dx, dy) - 1
	// Manhattan distance - 1 = dx + dy + max(dx, dy) - 1
	// (dx,dy) = (1,0)->1, (1,1)->2, ..., (5,6)->16
	return std::max(0, std::min(dx + dy + std::max(dx, dy) - 1, 16));

}

/**
 *  盤端からの距離を求める
 *  Calculate distance from the outer boundary.
 */
int DistEdge(int v){

	// PASSは盤外扱い
	// Return 0 if v is PASS/VNULL.
	if (v >= PASS) return 0;

	// 盤外->0, 1線->1, 2線->2, ...
	// off-board->0, the 1st line->1, the 2nd line->2, ...
	return std::min( {	etox[v],
						EBSIZE - 1 - etox[v],
						etoy[v],
						EBSIZE - 1 - etoy[v] } );

}


/**
 *  ファイルからprob_distに読み込む
 *  Import distance parameters from prob_dist.txt.
 */
void ImportProbDist() {

	std::ifstream ifs("prob_dist.txt");
	if (ifs.fail()) cerr << "file could not be opened: prob_dist.txt" << endl;

	for(int i=0;i<2;++i){
		std::string str, prob_str;
		getline(ifs, str);
		std::istringstream iss(str);

		for (int j=0;j<17;j++) {
			for (int k=0;k<2;++k) {
				getline(iss, prob_str, ',');
				prob_dist[i][j][k] = stod(prob_str);
			}
		}
	}

	prob_dist_base.fill(0.0);

	// 学習から求めた盤端からの距離パラメータ
	// Distance parameters from the outer boundary.
	std::array<double, 10> prob_dist_edge =
		{	0.448862, 0.823956, 1.639304,
			1.257309, 0.959127, 1.007954,
			1.084042, 1.068953, 1.063100,
			1.101500	};

	for(auto i:rtoe){
		prob_dist_base[i] = prob_dist_edge[DistEdge(i) - 1];
	}

}
