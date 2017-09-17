#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "print.h"

using std::cerr;
using std::endl;
using std::string;

/**
 *  盤面をエラーコンソールに表示する
 *  Display the board in stderr.
 */
void PrintBoard(Board& b, int v){

	// x座標を出力
	// Output captions of x coordinate.
	string str_x = "ABCDEFGHJKLMNOPQRST";
	cerr << "  ";
	for (int x=0;x<BSIZE;++x)  cerr << " " << str_x[x] << " ";
	cerr << endl;

	for(int y=0;y<BSIZE;++y){

		// 一桁のときインデントを追加
		// Add indent when single digit.
		if(BSIZE - y < 10) cerr << " ";
		// 左下が(A,1)のため、y座標を逆順で出力する
		// Output y coordinate in reverse order because the lower left is (A,1).
		cerr << BSIZE - y;

		for (int x=0;x<BSIZE;++x) {
			int ev = rtoe[xytor[x][BSIZE - 1 - y]];
			bool is_star = false;
			if(	(x == 3 || x == 9 || x == 15) &&
				(y == 3 || y == 9 || y == 15)) is_star = true;

			if (v == ev) {
				// 直前の着手が白
				// When previous move is white.
				if(b.my == 1) cerr << "[O]";
				// 直前の着手が黒
				// When previous move is black.
				else cerr << "[X]";
			}
			else if(b.color[ev] == 2) cerr << " O ";
			else if(b.color[ev] == 3) cerr << " X ";
			else if(is_star) cerr << " + "; // Star
			else cerr << " . ";
		}

		if(BSIZE - y < 10) cerr << " ";
		cerr << BSIZE - y << endl;

	}

	cerr << "  ";
	for (int x=0;x<BSIZE;++x)  cerr << " " << str_x[x] << " ";
	cerr << endl;

}

/**
 *  盤面をファイルに出力する.
 *  Output the board to the file.
 */
void PrintBoard(Board& b, int v, std::string file_path) {

	//出力するファイルを開く
	std::ofstream ofs(file_path, std::ios::app);
	if (ofs.fail()) return;

	// x座標を出力
	// Output captions of x coordinate.
	string str_x = "ABCDEFGHJKLMNOPQRST";
	ofs << "  ";
	for (int x=0;x<BSIZE;++x)  ofs << " " << str_x[x] << " ";
	ofs << endl;

	for(int y=0;y<BSIZE;++y){

		// 一桁のときインデントを追加
		// Add indent when single digit.
		if(BSIZE - y < 10) ofs << " ";
		// 左下が(A,1)のため、y座標を逆順で出力する
		// Output y coordinate in reverse order because the lower left is (A,1).
		ofs << BSIZE - y;

		for (int x=0;x<BSIZE;++x) {
			int ev = rtoe[xytor[x][BSIZE - 1 - y]];
			bool is_star = false;
			if(	(x == 3 || x == 9 || x == 15) &&
				(y == 3 || y == 9 || y == 15)) is_star = true;

			if (v == ev) {
				// 直前の着手が白
				// When previous move is white.
				if(b.my == 1) ofs << "[O]";
				// 直前の着手が黒
				// When previous move is black.
				else ofs << "[X]";
			}
			else if(b.color[ev] == 2) ofs << " O ";
			else if(b.color[ev] == 3) ofs << " X ";
			else if(is_star) ofs << " + "; // Star
			else ofs << " . ";
		}

		if(BSIZE - y < 10) ofs << " ";
		ofs << BSIZE - y << endl;

	}

	ofs << "  ";
	for (int x=0;x<BSIZE;++x)  ofs << " " << str_x[x] << " ";
	ofs << endl;

	ofs.close();
}

void PrintEF(std::string str, std::ofstream& ofs){
	std::cerr << str;
	if(ofs) ofs << str;
}

/**
 *  最終結果を出力する
 *  Output the final score.
 */
void PrintFinalScore(Board& b, int (&game_cnt)[3], int (&owner_cnt)[2][EBVCNT],
		int win_pl, double komi, std::string file_path)
{

	std::ofstream ofs(file_path, std::ios::app);

	// 1. 死石を表示する
	//    Display dead stones.
	PrintEF("\ndead stones ([ ] = dead)\n", ofs);

	// x座標を出力
	// Output captions of x coordinate.
	string str_x = "ABCDEFGHJKLMNOPQRST";
	PrintEF("  ", ofs);
	for (int x=0;x<BSIZE;++x){
		string str_ = " "; str_ += str_x[x]; str_ += " ";
		PrintEF(str_, ofs);
	}
	PrintEF("\n", ofs);

	for(int y=0;y<BSIZE;++y){

		// 一桁のときインデントを追加
		// Add indent when single digit.
		if (BSIZE - y < 10) PrintEF(" " + std::to_string(BSIZE - y), ofs);
		else PrintEF(std::to_string(BSIZE - y), ofs);

		for(int x=0;x<BSIZE;++x){
			int v = rtoe[xytor[x][BSIZE - 1 - y]];
			bool is_star = false;
			if(	(x == 3 || x == 9 || x == 15) &&
				(y == 3 || y == 9 || y == 15)) is_star = true;

			// vにおける占有率が50%未満のときは死石
			// Dead stone if occupancy is less than 50%.
			if(b.color[v] == 2 && (double)owner_cnt[0][v]/game_cnt[2] < 0.5){
				PrintEF("[O]", ofs);
			}
			else if(b.color[v] == 3 && (double)owner_cnt[1][v]/game_cnt[2] < 0.5){
				PrintEF("[X]", ofs);
			}
			else if(b.color[v] == 2){
				PrintEF(" O ", ofs);
			}
			else if(b.color[v] == 3) {
				PrintEF(" X ", ofs);
			}
			else if(is_star){
				PrintEF(" + ", ofs);
			}
			else {
				PrintEF(" . ", ofs);
			}
		}

		if (BSIZE - y < 10) PrintEF(" " + std::to_string(BSIZE - y) + "\n", ofs);
		else PrintEF(std::to_string(BSIZE - y) + "\n", ofs);

	}

	PrintEF("  ", ofs);
	for (int x=0;x<BSIZE;++x){
		string str_ = " "; str_ += str_x[x]; str_ += " ";
		PrintEF(str_, ofs);
	}
	PrintEF("\n", ofs);

	// 2. アゲハマを計算する
	//    Count taken stones for Japanese rule.
	int score_jp[2] = {0, 0};
	int score_cn[2] = {0, 0};
	int agehama[2];
	for(int i=0;i<2;++i){
		agehama[i] = b.move_cnt / 2 - b.stone_cnt[i];
		agehama[i] += int(i == 1)*int(b.move_cnt % 2 == 1) - b.pass_cnt[i];
	}

	// 3. 地を表示する
	//    Display occupied areas.
	PrintEF("\narea ([ ] = black area, ? ? = unknown)\n", ofs);

	// x座標を出力
	// Output captions of x coordinate.
	PrintEF("  ", ofs);
	for (int x=0;x<BSIZE;++x){
		string str_ = " "; str_ += str_x[x]; str_ += " ";
		PrintEF(str_, ofs);
	}
	PrintEF("\n", ofs);

	for(int y=0;y<BSIZE;++y){

		// 一桁のときインデントを追加
		// Add indent when single digit.
		if (BSIZE - y < 10) PrintEF(" " + std::to_string(BSIZE - y), ofs);
		else PrintEF(std::to_string(BSIZE - y), ofs);

		for(int x=0;x<BSIZE;++x){
			int v = rtoe[xytor[x][BSIZE - 1 - y]];
			bool is_star = false;
			if(	(x == 3 || x == 9 || x == 15) &&
				(y == 3 || y == 9 || y == 15)) is_star = true;

			// vにおける白の占有率が50%以上
			// When white occupancy is more than 50%.
			if((double)owner_cnt[0][v]/game_cnt[2] > 0.5){
				if(b.color[v] == 2) PrintEF(" O ", ofs);
				else if(b.color[v] == 3){
					PrintEF(" X ", ofs);
					// 日本ルールで地とアゲハマに加える
					// For Japanese rule.
					++agehama[1];
					++score_jp[0];
				}
				else{
					if(is_star) PrintEF(" + ", ofs);
					else PrintEF(" . ", ofs);
					++score_jp[0];
				}

				++score_cn[0];
			}
			// vにおける黒の占有率が50%以上
			// When black occupancy is more than 50%.
			else if((double)owner_cnt[1][v]/game_cnt[2] > 0.5){
				if(b.color[v] == 2){
					PrintEF("[O]", ofs);
					// 日本ルールで地とアゲハマに加える
					// For Japanese rule.
					++agehama[0];
					++score_jp[1];
				}
				else if(b.color[v] == 3) PrintEF("[X]", ofs);
				else{
					if(is_star) PrintEF("[+]", ofs);
					else PrintEF("[.]", ofs);
					++score_jp[1];
				}

				++score_cn[1];
			}
			// どちらの地か不明
			// Unknown area.
			else if (b.color[v] == 2){
				PrintEF("?O?", ofs);
			}
			else if (b.color[v] == 3) {
				PrintEF("?X?", ofs);
			}
			else {
				if(is_star) PrintEF("?+?", ofs);
				else PrintEF("?.?", ofs);
			}
		}

		if (BSIZE - y < 10) PrintEF(" " + std::to_string(BSIZE - y) + "\n", ofs);
		else PrintEF(std::to_string(BSIZE - y) + "\n", ofs);

	}

	PrintEF("  ", ofs);
	for (int x=0;x<BSIZE;++x){
		string str_ = " "; str_ += str_x[x]; str_ += " ";
		PrintEF(str_, ofs);
	}
	PrintEF("\n", ofs);

	// 4. 結果
	//    Show final results.
	string str_win_pl = win_pl==0 ? "W+":"B+";

	double abs_score_jp = std::abs((score_jp[1]-agehama[1])-(score_jp[0]-agehama[0])-komi);
	double abs_score_cn = std::abs(score_cn[1] - score_cn[0] - komi);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(1);
	ss << "result: " << str_win_pl;
	ss << abs_score_cn << " (Chinese rule)" << endl;

	PrintEF(ss.str(), ofs);

	ss.str("");
	ss << "result: " << str_win_pl;
	ss << abs_score_jp << " (Japanese rule)" << endl;

	PrintEF(ss.str(), ofs);

	ofs.close();

}

/**
 *  占有率を表示する
 *  Display occupancy of the vertexes in stderr.
 */
void PrintOccupancy(int (&game_cnt)[3], int (&owner_cnt)[2][EBVCNT]){

	// x座標を出力
	// Output captions of x coordinate.
	string str_x = "ABCDEFGHJKLMNOPQRST";
	cerr << "  ";
	for (int x=0;x<BSIZE;++x)  cerr << " " << str_x[x] << " ";
	cerr << endl;

	for(int y=0;y<BSIZE;++y){

		// 一桁のときインデントを追加
		// Add indent when single digit.
		if(BSIZE - y < 10) cerr << " ";
		// 左下が(A,1)のため、y座標を逆順で出力する
		// Output y coordinate in reverse order because the lower left is (A,1).
		cerr << BSIZE - y;

		for (int x=0;x<BSIZE;++x) {
			int v = rtoe[xytor[x][BSIZE - 1 - y]];

			// 黒の占有率
			// Occupancy of black stones.
			double black_ratio = (double)owner_cnt[1][v] / game_cnt[2];
			// 0-9の整数に切り上げ
			// Round up to integer [0,9]
			int disp_ratio = (int)round(black_ratio * 9);


			if(black_ratio >= 0.5){
				// 黒の占有率が50%以上
				// When black occupancy is more than 50%.
				cerr << "[" << disp_ratio << "]";
			}
			else{
				// 白の占有率が50%以上
				// When white occupancy is more than 50%.
				cerr << " " << disp_ratio << " ";
			}
		}

		if(BSIZE - y < 10) cerr << " ";
		cerr << BSIZE - y << endl;

	}

	cerr << "  ";
	for (int x=0;x<BSIZE;++x)  cerr << " " << str_x[x] << " ";
	cerr << endl;

}
