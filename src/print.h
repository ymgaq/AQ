#pragma once

#include <string>
#include "board.h"

void PrintBoard(Board& b, int v);
void PrintBoard(Board& b, int v, std::string file_path);
void PrintFinalScore(Board& b, int (&game_cnt)[3], int (&owner_cnt)[2][EBVCNT],
		int win_pl, double komi, std::string file_path);
void PrintOccupancy(int (&game_cnt)[3], int (&owner_cnt)[2][EBVCNT]);
