#pragma once

#include <array>
#include "board_config.h"


int DistBetween(int v1, int v2);
int DistEdge(int v);
void ImportProbDist();

extern std::array<std::array<std::array<double, 2>, 17>,2> prob_dist;
extern std::array<double, EBVCNT> prob_dist_base;
