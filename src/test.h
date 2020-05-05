/*
 * AQ, a Go playing engine.
 * Copyright (C) 2017-2020 Yu Yamaguchi
 * except where otherwise indicated.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TEST_H_
#define TEST_H_

#include "./board.h"
#include "./network.h"
#include "./option.h"
#include "./search.h"
#include "./sgf.h"

/**
 * Tests structure and transitions of Board class.
 */
void TestBoard();

/**
 * Checks if the board with symmetric operation is registered in EvalCache.
 */
void TestSymmetry();

/**
 * Plays a self match with the hand with the maximum probability of Policy head.
 */
void PolicySelf();

/**
 * Plays a self match.
 */
void SelfMatch();

/**
 * Benchmark the inference speed of a neural network.
 */
void NetworkBench();

/**
 * Measures the execution speed of the rollout.
 */
void BenchMark();

/**
 * Measures the speed at which a tree node is freed from memory.
 */
void TestFreeMemory();

/**
 * Reads an SGF file and test the score of the last board.
 */
void ReadSgfFinalScore(int argc, char** argv);

/**
 * Random rollouts are performed to display the seki.
 */
void TestSeki();

/**
 * Tests to see if you can pass properly under Japanese rules.
 */
void TestPassMove();

#endif  // TEST_H_
