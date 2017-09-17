#pragma once

#include "board.h"
#include "board_simple.h"

bool SearchLadder(Board& b, std::vector<int> (&ladder_list)[2]);
bool IsWastefulEscape(BoardSimple& b, int pl, int v);
bool IsSuccessfulCapture(BoardSimple& b, int pl, int v);
bool IsLadder(BoardSimple& b, int v_atr, int depth);
