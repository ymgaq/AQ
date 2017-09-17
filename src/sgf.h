#pragma once
#include <fstream>
#include <iostream>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
	#include <Windows.h>
	#include <conio.h>
#else
	#include <sys/types.h>
	#include <dirent.h>
	#include <limits.h>
	#include <pthread.h>
#endif

#include "board.h"
#include "print.h"

/**************************************************************
 *
 *  SGFファイルの情報を格納するクラス
 *  Class containing match information of an SGF file.
 *
 ***************************************************************/
class SgfData {

public:
	std::string rule;
	int board_size;
	double komi;
	std::string player[2];
	int rating[2];
	int handicap;
	int winner;
	std::vector<int> handicap_stone[2];
	std::vector<int> move_history;
	int move_cnt;
	bool is_black_first;
	double score;

	SgfData();
	void Clear();
	void AddMove(int v);
	void ImportData(std::string file_name);
	void ExportData(std::string file_name);
	void ExportData(std::string file_name, std::vector<std::string> cmt_list);
	int ConvertToVertex(std::string aa);
	bool GenerateBoard(Board& b, int move_idx);

};

int ImportSGFList(std::string folder, std::vector<SgfData>& sgf_list);
