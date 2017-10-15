#include "playout.h"

#define forEach4Nbr(v_origin,v_nbr,block)		\
	int v_nbr;									\
	v_nbr = v_origin + 1;			block;		\
	v_nbr = v_origin - 1;			block;		\
	v_nbr = v_origin + EBSIZE;		block;		\
	v_nbr = v_origin - EBSIZE;		block;

bool japanese_rule = false;

/**
 *  終局図の勝敗を返す
 *  黒勝->±1、白勝->0
 *  すべての石を打ち上げている前提
 *
 *  Return the result of the board in the end of the game.
 *  white wins: 0, black wins: 1 or -1.
 *  Before calling this function, both players need to play
 *  until legal moves cease to exist.
 */
int Win(Board& b, int pl, double komi) {

	double score[2] = {0.0, 0.0};
	std::array<bool, EBVCNT> visited;
	std::fill(visited.begin(), visited.end(), false);

	// セキがあるか確認. Check Seki.
	for(int i=0,i_max=b.empty_cnt;i<i_max;++i){
		int v = b.empty[i];
		if(b.IsSeki(v) && !visited[v]){
			// 隅の曲がり四目か確認.
			// Check whether it is corner bent fours.
			int ren_idx[2] = {0,0};
			forEach4Nbr(v, v_nbr2, {
				if(b.color[v_nbr2] > 1){
					ren_idx[b.color[v_nbr2] - 2] = b.ren_idx[v_nbr2];
				}
			});
			bool is_bent4 = false;
			for(int j=0;j<2;++j){
				if(b.ren[ren_idx[j]].size == 3){
					int v_tmp = ren_idx[j];
					bool is_edge = true;
					bool is_conner = false;

					do{
						is_edge &= (DistEdge(v_tmp) == 1);
						if(!is_edge) break;
						if (	v_tmp == rtoe[0] 					||
								v_tmp == rtoe[BSIZE - 1] 			||
								v_tmp == rtoe[BSIZE * (BSIZE - 1)] 	||
								v_tmp == rtoe[BVCNT - 1]	){
							bool is_not_bnt = false;
							forEach4Nbr(v_tmp, v_nbr1, {
								// 敵石のとき、曲がり4目ではない
								// If the neighboring stone is an opponnent's one.
								is_not_bnt |= (b.color[v_nbr1] == int(j==0) + 2);
							});

							is_conner = !is_not_bnt;
						}

						v_tmp = b.next_ren_v[v_tmp];
					}while(v_tmp != ren_idx[j]);

					if(is_edge && is_conner){
						// 曲がり四目のとき、4目側の地とする
						// Count all stones as that of the player of the bent fours.
						score[j] += b.ren[ren_idx[0]].size + b.ren[ren_idx[1]].size + 2.0;
						is_bent4 = true;
					}
				}
			}
			// visitedを更新. Update visited.
			int64 lib_bit;
			for(int i=0;i<6;++i){
				lib_bit = b.ren[ren_idx[0]].lib_bits[i];
				while(lib_bit != 0){
					int ntz = NTZ(lib_bit);
					int lib = rtoe[ntz + i * 64];
					visited[lib] = true;

					lib_bit ^= (0x1ULL << ntz);
				}
			}
			// 曲がり四目のとき. If it bent fours exist.
			if(is_bent4){
				int v_tmp = ren_idx[0];
				do{
					visited[v_tmp] = true;
					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[0]);
				v_tmp = ren_idx[1];
				do{
					visited[v_tmp] = true;
					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[1]);
			}
		}
	}

	for (auto i: rtoe) {
		int stone_color = b.color[i] - 2;
		if (!visited[i] && (stone_color >= 0)) {
			visited[i] = true;
			++score[stone_color];
			forEach4Nbr(i, v_nbr, {
				if (!visited[v_nbr] && b.color[v_nbr] == 0) {
					visited[v_nbr] = true;
					++score[stone_color];
				}
			});
		}
	}

	// 白黒パス回数の差、黒が最後に着手で+1
	// Correction factor of PASS. Add one if the last move is black.
	int pass_corr = b.pass_cnt[0] - b.pass_cnt[1] + int((b.move_cnt%2)!=0);
	double abs_score = score[1] - score[0] - komi - pass_corr * int(japanese_rule);

	// 白勝ち->0, 黒番黒勝ち->1、白番黒勝ち-> -1を返す
	// Return 0 if white wins, 1 if black wins and it's black's turn and else -1.
	return int(abs_score > 0)*(int(pl == 1) - int(pl == 0));

}

/**
 *  終局図の勝敗を返す
 *  黒勝->±1、白勝->0
 *  盤面の占有率も更新する
 *
 *  Return the result of the board in the end of the game.
 *  white wins: 0, black wins: 1 or -1.
 *  Before calling this function, both players need to play
 *  until legal moves cease to exist.
 *  Updates game_cnt, stone_cnt and owner_cnt at the same time.
 */
int Win(Board& b, int pl, Statistics& stat, double komi) {

	double score[2] = {0.0, 0.0};
	std::array<bool, EBVCNT> visited;
	std::fill(visited.begin(), visited.end(), false);
	std::array<bool, EBVCNT> is_stone[2];
	std::fill(is_stone[0].begin(), is_stone[0].end(), false);
	std::fill(is_stone[1].begin(), is_stone[1].end(), false);

	// セキがあるか確認. Check Seki.
	for(int i=0,i_max=b.empty_cnt;i<i_max;++i){
		int v = b.empty[i];
		if(b.IsSeki(v) && !visited[v]){
			// 隅の曲がり四目か確認.
			// Check whether it is corner bent fours.
			int ren_idx[2] = {0,0};
			forEach4Nbr(v, v_nbr2, {
				if(b.color[v_nbr2] > 1){
					ren_idx[b.color[v_nbr2] - 2] = b.ren_idx[v_nbr2];
				}
			});
			int bnd_pl = -1;
			for(int j=0;j<2;++j){
				if(b.ren[ren_idx[j]].size == 3){
					int v_tmp = ren_idx[j];
					bool is_edge = true;
					bool is_conner = false;

					do{
						is_edge &= (DistEdge(v_tmp) == 1);
						if(!is_edge) break;
						if (	v_tmp == rtoe[0] 					||
								v_tmp == rtoe[BSIZE - 1] 			||
								v_tmp == rtoe[BSIZE * (BSIZE - 1)] 	||
								v_tmp == rtoe[BVCNT - 1]	){

							bool is_not_bnt = false;
							forEach4Nbr(v_tmp, v_nbr1, {
								// 敵石のとき、曲がり4目ではない
								// If the neighboring stone is an opponnent's one.
								is_not_bnt |= (b.color[v_nbr1] == int(j==0) + 2);
							});

							is_conner = !is_not_bnt;
						}

						v_tmp = b.next_ren_v[v_tmp];
					}while(v_tmp != ren_idx[j]);

					if(is_edge && is_conner){
						// 曲がり四目のとき、4目側の地とする
						// Count all stones as that of the player of the bent fours.
						score[j] += b.ren[ren_idx[0]].size + b.ren[ren_idx[1]].size + 2.0;
						bnd_pl = j;
					}
				}
			}
			// visitedを更新. Update visited.
			int64 lib_bit;
			for(int i=0;i<6;++i){
				lib_bit = b.ren[ren_idx[0]].lib_bits[i];
				while(lib_bit != 0){
					int ntz = NTZ(lib_bit);
					int lib = rtoe[ntz + i * 64];
					visited[lib] = true;
					if(bnd_pl != -1) {
						++stat.owner[bnd_pl][lib];
					}

					lib_bit ^= (0x1ULL << ntz);
				}
			}

			if(bnd_pl != -1){
				int v_tmp = ren_idx[0];
				do{
					visited[v_tmp] = true;
					++stat.owner[bnd_pl][v_tmp];
					++stat.stone[bnd_pl][v_tmp];
					is_stone[bnd_pl][v_tmp] = true;

					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[0]);
				v_tmp = ren_idx[1];
				do{
					visited[v_tmp] = true;
					++stat.owner[bnd_pl][v_tmp];
					++stat.stone[bnd_pl][v_tmp];
					is_stone[bnd_pl][v_tmp] = true;

					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[1]);
			}
		}
	}

	for (auto i: rtoe) {
		int stone_color = b.color[i] - 2;
		if (!visited[i] && stone_color >= 0) {
			visited[i] = true;
			++score[stone_color];
			++stat.owner[stone_color][i];
			++stat.stone[stone_color][i];
			is_stone[stone_color][i] = true;
			forEach4Nbr(i, v_nbr, {
				if (!visited[v_nbr] && b.color[v_nbr] == 0) {
					visited[v_nbr] = true;
					++score[stone_color];
					++stat.owner[stone_color][v_nbr];
				}
			});
		}
	}

	// 白黒パス回数の差、黒が最後に着手で+1
	// Correction factor of PASS. Add one if the last move is black.
	int pass_corr = b.pass_cnt[0] - b.pass_cnt[1] + int((b.move_cnt%2)!=0);
	double abs_score = score[1] - score[0] - komi - pass_corr * int(japanese_rule);

	if(abs_score > 0){
		++stat.game[1];
		for(auto i: rtoe){
			if(is_stone[1][i]){
				++stat.stone[2][i];
			}
		}
	}
	else{
		++stat.game[0];
		for(auto i: rtoe){
			if(is_stone[0][i]) ++stat.stone[2][i];
		}
	}
	++stat.game[2];

	// 白勝ち->0, 黒番黒勝ち->1、白番黒勝ち-> -1を返す
	// Returns 0 if white wins, 1 if black wins and it's black's turn and else -1.
	return int(abs_score > 0)*(int(pl == 1) - int(pl == 0));

}

/**
 *  終局図のスコアを返す
 *  黒勝ち->+score 白勝ち->-score
 *
 *  Return the score of the board in the end of the game.
 *  white wins: -score, black wins: +score.
 */
double Score(Board& b, double komi) {

	double score[2] = {0.0, 0.0};
	std::array<bool, EBVCNT> visited;
	std::fill(visited.begin(), visited.end(), false);

	// セキがあるか確認. Check Seki.
	for(int i=0,i_max=b.empty_cnt;i<i_max;++i){
		int v = b.empty[i];
		if(b.IsSeki(v) && !visited[v]){
			// 隅の曲がり四目か確認.
			// Check whether it is corner bent fours.
			int ren_idx[2] = {0,0};
			forEach4Nbr(v, v_nbr2, {
				if(b.color[v_nbr2] > 1){
					ren_idx[b.color[v_nbr2] - 2] = b.ren_idx[v_nbr2];
				}
			});

			int bnd_pl = -1;
			for(int j=0;j<2;++j){
				if(b.ren[ren_idx[j]].size == 3){
					int v_tmp = ren_idx[j];
					bool is_edge = true;
					bool is_conner = false;

					do{
						is_edge &= (DistEdge(v_tmp) == 1);
						if(!is_edge) break;
						if (	v_tmp == rtoe[0] 					||
								v_tmp == rtoe[BSIZE - 1] 			||
								v_tmp == rtoe[BSIZE * (BSIZE - 1)] 	||
								v_tmp == rtoe[BVCNT - 1]	){
							bool is_not_bnt = false;
							forEach4Nbr(v_tmp, v_nbr1, {
								// 敵石のとき、曲がり4目ではない
								// If the neighboring stone is an opponnent's one.
								is_not_bnt |= (b.color[v_nbr1] == int(j==0) + 2);
							});

							is_conner = !is_not_bnt;
						}

						v_tmp = b.next_ren_v[v_tmp];
					}while(v_tmp != ren_idx[j]);

					if(is_edge && is_conner){
						// 曲がり四目のとき、4目側の地とする
						// Count all stones as that of the player of the bent fours.
						score[j] += b.ren[ren_idx[0]].size + b.ren[ren_idx[1]].size + 2.0;
						bnd_pl = j;
					}
				}
			}
			// visitedを更新. Update visited.
			int64 lib_bit;
			for(int i=0;i<6;++i){
				lib_bit = b.ren[ren_idx[0]].lib_bits[i];
				while(lib_bit != 0){
					int ntz = NTZ(lib_bit);
					int lib = rtoe[ntz + i * 64];
					visited[lib] = true;

					lib_bit ^= (0x1ULL << ntz);
				}
			}

			if(bnd_pl != -1){
				int v_tmp = ren_idx[0];
				do{
					visited[v_tmp] = true;
					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[0]);
				v_tmp = ren_idx[1];
				do{
					visited[v_tmp] = true;
					v_tmp = b.next_ren_v[v_tmp];
				}while(v_tmp != ren_idx[1]);
			}
		}
	}

	for (auto i: rtoe) {
		int stone_color = b.color[i] - 2;
		if (!visited[i] && stone_color >= 0) {
			visited[i] = true;
			++score[stone_color];
			forEach4Nbr(i, v_nbr, {
				if (!visited[v_nbr] && b.color[v_nbr] == 0) {
					visited[v_nbr] = true;
					++score[stone_color];
				}
			});
		}
	}

	// 白黒パス回数の差、黒が最後に着手で+1
	// Correction factor of PASS. Add one if the last move is black.
	int pass_corr = b.pass_cnt[0] - b.pass_cnt[1] + int((b.move_cnt%2)!=0);
	double abs_score = score[1] - score[0] - komi - pass_corr * int(japanese_rule);

	return abs_score;

}

/**
 *  終局まで着手を繰り返し、勝敗を返す
 *  黒勝->±1、白勝->0
 *
 *  Play until the end and returns the result.
 *  white wins: 0, black wins: 1 or -1.
 */
int Playout(Board& b, double komi) {

	int next_move;
	int prev_move = VNULL;
	int pl = b.my;

	while (b.move_cnt <= 720) {
		next_move = b.SelectMove();
		// 2手連続でパスの場合、終局．
		// Break in case of 2 consecutive pass.
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	prev_move = VNULL;
	while (b.move_cnt <= 720) {
		next_move = b.SelectRandomMove();
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	// Return the result.
	return Win(b, pl, komi);

}

/**
 *  終局までランダムな着手を繰り返し、勝敗を返す
 *  黒勝->±1、白勝->0
 *
 *  Play with random moves until the end and returns the result.
 *  white wins: 0, black wins: 1 or -1.
 */
int PlayoutRandom(Board& b, double komi) {

	int next_move;
	int prev_move = VNULL;
	int pl = b.my;

	while (b.move_cnt <= 720) {
		next_move = b.SelectRandomMove();
		// 2手連続でパスの場合、終局．
		// Break in case of 2 consecutive pass.
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	// Return the result.
	return Win(b, pl, komi);

}

/**
 *  Last Good Replyありでプレイアウトを行い、勝敗を返す
 *  黒勝->±1、白勝->0
 *
 *  Play with 'Last Good Reply' until the end and returns the result.
 *  white wins: 0, black wins: 1 or -1.
 */
int PlayoutLGR(Board& b, LGR& lgr, double komi)
{

	int next_move;
	int prev_move = VNULL;
	int pl = b.my;
	std::array<int, 4> lgr_seed;
	std::vector<std::array<int, 3>> lgr_rollout_add[2];
	std::array<int, 3> lgr_rollout_seed;
	int update_v[2] = {VNULL, VNULL};
	double update_p[2] = {100, 25};

	while (b.move_cnt <= 720) {
		lgr_seed[0] = b.prev_ptn[0].bf;
		lgr_seed[1] = b.prev_move[b.her];
		lgr_seed[2] = b.prev_ptn[1].bf;
		lgr_seed[3] = b.prev_move[b.my];

		// ナカデで石を取られたとき、急所に打つ
		// Forced move if removed stones is Nakade.
		if (b.response_move[0] != VNULL) {
			next_move = b.response_move[0];
			b.PlayLegal(next_move);
		}
		else{

			// lgr.policyに含まれる
			// Check whether lgr_seed is included in lgr.policy.
			auto itr = lgr.policy[b.my].find(lgr_seed);
			int v = VNULL;
			if (itr != lgr.policy[b.my].end()){
				v = itr->second;
				if(v < PASS && b.IsLegal(b.my, v) && !b.IsEyeShape(b.my, v) && !b.IsSeki(v)){
					if(b.prob[b.my][v] != 0){
						b.ReplaceProb(b.my, v, b.prob[b.my][v] * update_p[0]);
						update_v[0] = v;
					}
				}
			}

			v = VNULL;
			if(lgr_seed[1] < PASS && lgr_seed[3] < PASS){
				v = lgr.rollout[b.my][lgr_seed[1]][lgr_seed[3]];

				if(v < PASS){
					if(b.prob[b.my][v] != 0){
						b.ReplaceProb(b.my, v, b.prob[b.my][v] * update_p[1]);
						update_v[1] = v;
					}
				}
			}

			next_move = b.SelectMove();

			// update_vの手の確率を元に戻す
			// Restore probability.
			for(int i=0;i<2;++i){
				if(update_v[i] != VNULL){
					if(b.prob[b.her][update_v[i]] != 0){
						b.ReplaceProb(b.her, update_v[i], b.prob[b.her][update_v[i]] / update_p[i]);
					}
					update_v[i] = VNULL;
				}
			}

		}

		if(lgr_seed[1] < PASS && lgr_seed[3] < PASS && next_move < PASS){
			lgr_rollout_seed[0] = lgr_seed[1];
			lgr_rollout_seed[1] = lgr_seed[3];
			lgr_rollout_seed[2] = next_move;
			lgr_rollout_add[b.her].push_back(lgr_rollout_seed);
		}

		// 2手連続でパスの場合、終局
		// Break in case of 2 consecutive pass.
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	prev_move = VNULL;
	while (b.move_cnt <= 720) {
		next_move = b.SelectRandomMove();
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	int win = Win(b, pl, komi);
	int win_pl = int(win != 0);
	int lose_pl = int(win == 0);

	for(auto& i:lgr_rollout_add[win_pl]){
		lgr.rollout[win_pl][i[0]][i[1]] = i[2];
	}

	for(auto& i:lgr_rollout_add[lose_pl]){
		if(lgr.rollout[lose_pl][i[0]][i[1]] == i[2]){
			lgr.rollout[lose_pl][i[0]][i[1]] = VNULL;
		}
	}

	// 終局図の勝敗を返す
	// Return the result.
	return win;

}

/**
 *  Last Good Replyありでプレイアウトを行い、勝敗を返す
 *  黒勝->±1、白勝->0
 *  盤面の占有率も更新する
 *
 *  Play with 'Last Good Reply' until the end and returns the result.
 *  white wins: 0, black wins: 1 or -1.
 *  Updates game_cnt, stone_cnt and owner_cnt at the same time.
 */
int PlayoutLGR(Board& b, LGR& lgr, Statistics& stat, double komi)
{

	int next_move;
	int prev_move = VNULL;
	int pl = b.my;
	std::array<int, 4> lgr_seed;
	std::vector<std::array<int, 3>> lgr_rollout_add[2];
	std::array<int, 3> lgr_rollout_seed;
	int update_v[2] = {VNULL, VNULL};
	double update_p[2] = {100, 25};

	while (b.move_cnt <= 720) {
		lgr_seed[0] = b.prev_ptn[0].bf;
		lgr_seed[1] = b.prev_move[b.her];
		lgr_seed[2] = b.prev_ptn[1].bf;
		lgr_seed[3] = b.prev_move[b.my];

		// ナカデで石を取られたとき、急所に打つ
		// Forced move if removed stones is Nakade.
		if (b.response_move[0] != VNULL) {
			next_move = b.response_move[0];
			b.PlayLegal(next_move);
		}
		else{

			auto itr = lgr.policy[b.my].find(lgr_seed);
			int v = VNULL;
			if (itr != lgr.policy[b.my].end()){
				v = itr->second;
				if(v != VNULL && b.IsLegal(b.my, v) && !b.IsEyeShape(b.my, v) && !b.IsSeki(v)){
					if(b.prob[b.my][v] != 0){
						b.ReplaceProb(b.my, v, b.prob[b.my][v] * update_p[0]);
						update_v[0] = v;
					}
				}
			}

			v = VNULL;
			if(lgr_seed[1] < PASS && lgr_seed[3] < PASS){
				v = lgr.rollout[b.my][lgr_seed[1]][lgr_seed[3]];
				if(v != VNULL){
					//lgr_rolloutの手の確率をx倍に
					if(b.prob[b.my][v] != 0){
						b.ReplaceProb(b.my, v, b.prob[b.my][v] * update_p[1]);
						update_v[1] = v;
					}
				}
			}

			next_move = b.SelectMove();

			// update_vの手の確率を元に戻す
			// Restore probability.
			for(int i=0;i<2;++i){
				if(update_v[i] != VNULL){
					if(b.prob[b.her][update_v[i]] != 0){
						b.ReplaceProb(b.her, update_v[i], b.prob[b.her][update_v[i]] / update_p[i]);
					}
					update_v[i] = VNULL;
				}
			}

		}

		if(lgr_seed[1] < PASS && lgr_seed[3] < PASS && next_move != PASS){
			lgr_rollout_seed[0] = lgr_seed[1];
			lgr_rollout_seed[1] = lgr_seed[3];
			lgr_rollout_seed[2] = next_move;
			lgr_rollout_add[b.her].push_back(lgr_rollout_seed);
		}

		// 2手連続でパスの場合、終局
		// Break in case of 2 consecutive pass.
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	prev_move = VNULL;
	while (b.move_cnt <= 720) {
		next_move = b.SelectRandomMove();
		if (next_move==PASS && prev_move==PASS) break;
		prev_move = next_move;
	}

	int win = Win(b, pl, stat, komi);
	int win_pl = int(win != 0);
	int lose_pl = int(win == 0);

	for(auto& i:lgr_rollout_add[win_pl]){
		lgr.rollout[win_pl][i[0]][i[1]] = i[2];
	}

	for(auto& i:lgr_rollout_add[lose_pl]){
		if(lgr.rollout[lose_pl][i[0]][i[1]] == i[2]){
			lgr.rollout[lose_pl][i[0]][i[1]] = VNULL;
		}
	}

	// 終局図の勝敗を返す
	// Return the result.
	return win;

}

#undef forEach4Nbr
