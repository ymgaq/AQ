#include <algorithm>
#include <fstream>
#include <iostream>
#include <chrono>

#include "ladder.h"
#include "feed_tensor.h"
#include "sgf.h"


#ifdef USE_52FEATURE
	constexpr int feature_cnt = 52;
#else
	constexpr int feature_cnt = 49;
#endif


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
#ifndef USE_52FEATURE
		i[4] = 1.0;	//ones
#endif
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
#ifndef USE_52FEATURE
		i[4] = 1.0;	//ones
#endif
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

#ifndef USE_52FEATURE
	// 1. turn since(5-12)を更新
	//    Update turn since (5-12).
	for(int i=0, i_max=std::min(8,int(b.move_cnt));i<i_max;++i){
		int v = b.move_history[b.move_cnt - 1 - i];
		if(v == PASS) continue;
		feature[etor[v]][TURNSINCE + i] = 1.0;
	}
#endif //USE_52FEATURE

	for (int rv=0;rv<BVCNT;++rv) {
		int v = rtoe[rv];
		int v_color = b.color[v];
		int pl_color[2] = { 2 + int(b.my == 1), 2 + int(b.my == 0) };

#ifdef USE_52FEATURE
		// 1. color(16)を更新. Update color (16)
		feature[rv][COLOR] = (float)color;

		// 2. stones(0-15)を更新. Update stone (0-15).
		feature[rv][0] = float(v_color == pl_color[0]);
		feature[rv][1] = float(v_color == pl_color[1]);
		for(int i=1;i<8;++i){
			feature[rv][STONES + 2 * i] = float(b.prev_color[i - 1][v] == pl_color[0]);
			feature[rv][STONES + 2 * i + 1] = float(b.prev_color[i - 1][v] == pl_color[1]);
		}
#else

		// 2. stone(0-3)を更新. Update stone (0-3).
		//    v_color == 0 -> empty,  2 -> white, 3 -> black
		feature[rv][int(v_color != 0)*(1 + int(b.my == int(v_color == 2)))] = 1.0;
#endif //USE_52FEATURE

		if (v_color >= 2) {

			// 3. liberty(LIBERTY...LIBERTY+8)を更新. Update number of liberty.
			//    [LIBERTY + 7] -> 8 or more liberty
			feature[rv][LIBERTY + std::min(7, (int)b.ren[b.ren_idx[v]].lib_cnt - 1)] = 1.0;

		}
		else if (b.IsLegal(b.my, v)) {

			// 4. sensibleness(SENSIBLENESS)を更新
			//    Update sensibleness (SENSIBLENESS).
			if(!b.IsEyeShape(b.my, v) && !b.IsSeki(v)){
				feature[rv][SENSIBLENESS] = 1.0;
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

			// 7. capture size(CAPTURESIZE...CAPTURESIZE+8)を更新
			//    Update capture size.
			if(cap_stone_cnt != 0){
				feature[rv][CAPTURESIZE + std::min(7, cap_stone_cnt - 1)] = 1.0;
			}

			lib_bits[etor[v]/64] &= ~(0x1ULL<<(etor[v]%64));	// vを除外. Exclude v.
			int lib_cnt = 0;
			for(int k=0;k<6;++k){
				if(lib_bits[k] != 0){
					lib_cnt += (int)popcnt64(lib_bits[k]);
				}
			}

			// 8. self atari size(SELFATARI..SELFATARI+8)を更新
			//    Update self Atari size.
			if (lib_cnt == 1) {
				feature[rv][SELFATARI + std::min(7, my_stone_cnt - 1)] = 1.0;
			}
			// 9. liberty after(LIBERTYAFTER...LIBERTYAFTER+8)を更新
			//    Update liberty after.
			feature[rv][LIBERTYAFTER + std::min(7, lib_cnt - 1)] = 1.0;
		}
#ifndef USE_52FEATURE
		// 10. false eye(48)を更新
		//     Update false eye (48).
		if(v_color == 0 && b.IsFalseEye(v)){
			feature[rv][FALSEEYE] = 1.0;
		}
#endif //USE_52FEATURE
	}

	// 11. ladder escape/capture (LADDERCAP-LADDERESC)を更新
	//     Update ladder escape/capture.
	std::vector<int> ladder_list[2];
	if (SearchLadder(b, ladder_list)) {
		for (auto v : ladder_list[0]) {
			feature[etor[v]][LADDERESC] = 1.0; // Ladder escape.
		}
		for (auto v : ladder_list[1]) {
			feature[etor[v]][LADDERCAP] = 1.0; // Ladder capture.
		}
	}

}

std::string TrimString(const std::string& str, const char* trim_chars) {

	std::string trimmed_str;
	auto left = str.find_first_not_of(trim_chars);

	if (left != std::string::npos)
	{
		auto right = str.find_last_not_of(trim_chars);
		trimmed_str = str.substr(left, right - left + 1);
	}
	return trimmed_str;

}

bool IsFlagOn(const std::string& str) {  return str == "on" || str == "On" || str == "ON"; }


/**
 *  学習用のバイナリを生成する
 *  Make data binary for supervised learning.
 */
void MakeLearningData(){

	// Read configures.
	std::string import_path, export_path;
	double komi_ = 7.5;
	int max_file_cnt = 10;

	std::string config_path = "learn/makedata_config.txt";
	std::ifstream ifs(config_path);
	std::string str;

	// Read line by line.
	int line_cnt = 0;
	while (ifs && getline(ifs, str)) {

		++line_cnt;
		// Exclude comment.
		auto cmt_pos = str.find("#");
		if(cmt_pos != std::string::npos){
			str = str.substr(0, cmt_pos);
		}
		str = TrimString(str, " \t\v\r\n-");
		if(str.length() == 0) continue;

		// Find position after '='.
		auto eql_pos = str.find("=") + 1;
		if(eql_pos == std::string::npos){
			std::cerr 	<< "Failed to parse config:" << config_path << ":"
						<< line_cnt << " " << str << ". '=' not found.\n";
			continue;
		}

		auto key = TrimString(str.substr(0, eql_pos - 1));
		auto val = TrimString(str.substr(eql_pos));

		if(key == "import sgf dir")	import_path = val;
		else if(key == "export dir") export_path = val;
		else if(key == "max file count") max_file_cnt = stoi(val);
		else if(key == "learning komi") komi_ = stod(val);
		else{
			std::cerr << "Unknown key: [" << key << "]" << std::endl;
		}

	}
	ifs.close();

	struct Feeds{
		FeedTensor ft;
		char color;
		short move;
		char result;
	};

	int max_train_cnt = 100000;
	int max_test_cnt = 10000;

	std::vector<Feeds> train_list;
	std::vector<Feeds> test_list;
	train_list.resize(max_train_cnt);
	test_list.resize(max_test_cnt);

	int file_idx = 0;
	int train_cnt = 0;
	int test_cnt = 0;
	int total_feed_cnt = 0;

	Board b;
	FeedTensor ft;
	bool export_test = false;

	auto start = std::chrono::system_clock::now();

	std::vector<SgfData> sgf_list;
	int kifu_cnt = ImportSGFList(import_path, sgf_list);
	std::fprintf(stderr, "%d files were imported.\n", kifu_cnt);

	for(;;){
		for (int i=0;i<kifu_cnt;++i) {
			if (sgf_list[i].move_cnt < 64) continue;
			else if (sgf_list[i].handicap != 0) continue;
			else if (sgf_list[i].winner == -1) continue;
			else if (sgf_list[i].komi < 6.5) continue;

			// Fix result of 0.5p with different Komi.
			if(	sgf_list[i].komi == komi_ &&
				sgf_list[i].score == 0.5 &&
				sgf_list[i].winner == (komi_==6.5)? 1 : 0)
			{
				sgf_list[i].winner = (komi_==6.5)? 0 : 1;
			}

			b.Clear();
			int rand_cnt = int(mt_double(mt_32) * (sgf_list[i].move_cnt - 3));

			for (int j = 0; j < sgf_list[i].move_cnt; ++j) {
				int vn = sgf_list[i].move_history[j];
				if (!b.IsLegal(b.my, vn)) break;

				if(j == rand_cnt && vn != PASS){
					if(i % 100 == 0){
						if(test_cnt >= max_test_cnt) break;
						ft.Set(b, vn);
						test_list[test_cnt].ft = ft;
						test_list[test_cnt].color = ft.color;
						test_list[test_cnt].move = vn;
						test_list[test_cnt].result = int(ft.color == sgf_list[i].winner);

						++test_cnt;
					}
					else{
						ft.Set(b, vn);
						train_list[train_cnt].ft = ft;
						train_list[train_cnt].color = ft.color;
						train_list[train_cnt].move = vn;
						train_list[train_cnt].result = int(ft.color == sgf_list[i].winner);

						++train_cnt;
						++total_feed_cnt;
					}

					if(train_cnt >= max_train_cnt || export_test){
						std::ofstream fout;
						std::stringstream ss;

						std::string str_idx = std::to_string(file_idx);
						if(file_idx < 10) 	str_idx = "0" + str_idx;
						if(file_idx < 100) 	str_idx = "0" + str_idx;
						if(export_test) str_idx = "test";

						ss << export_path << str_idx << ".bin";
						fout.open(ss.str(), std::ios::out | std::ios::binary | std::ios::trunc);

						if (!fout){
							std::cerr << "file " << ss.str() << " can not be opened." << std::endl;
							return;
						}

						int export_feeds_cnt = export_test? test_cnt : train_cnt;

						fout.write((char*)&export_feeds_cnt, sizeof(int));
						//char buf;
						short buf_s;
						std::vector<char> run_length;

						std::vector<Feeds>* pfd = export_test? &test_list : &train_list;

						for(int j=0;j<export_feeds_cnt;++j){

							for(int l=0;l<feature_cnt;++l){
								run_length.clear();
								char length = 1;
								for(int k=1;k<BVCNT;++k){
									if(	pfd->at(j).ft.feature[k - 1][l] == pfd->at(j).ft.feature[k][l] &&
										length < 127)
									{
										++length;
									}
									else{
										if(pfd->at(j).ft.feature[k - 1][l] == 1){
											run_length.push_back(length);
										}
										else run_length.push_back(-length);
										length = 1;
									}
								}

								if(pfd->at(j).ft.feature[BVCNT - 1][l] == 1){
									run_length.push_back(length);
								}
								else run_length.push_back(-length);

								buf_s = (short)run_length.size();
								fout.write((char*)&buf_s, sizeof(short));
								for(auto rl: run_length){
									fout.write((char*)&rl, sizeof(char));
								}
							}

							fout.write((char*)&pfd->at(j).color, sizeof(char));
							fout.write((char*)&pfd->at(j).result, sizeof(char));
							fout.write((char*)&pfd->at(j).move, sizeof(short));
						}

						fout.close();

						auto end = std::chrono::system_clock::now();
						double elapsed = double(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count()) / 1000;
						fprintf(stderr, "%s.bin %.1f[sec]\n", str_idx.c_str(), elapsed);

						start = std::chrono::system_clock::now();
						train_cnt = 0;
						++file_idx;

						if(export_test){
							std::cerr << "finished.\n";
							exit(0);
						}
						else if(file_idx >= max_file_cnt) export_test = true;

					}
					else if(train_cnt % (max_train_cnt / 10) == 0) std::cerr << ".";

					break;
				}

				b.PlayLegal(vn);
			}
		}
	}

	std::cerr << "finished.\n";
	exit(0);

}


#undef forEach4Nbr
#undef forEach8Nbr
