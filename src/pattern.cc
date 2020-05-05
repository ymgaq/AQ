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

#include "./pattern.h"
#include "./option.h"

double Pattern::prob_ptn3x3_[65536][256][2][2];
bool Pattern::legal_ptn_[256][256][2];
int Pattern::count_ptn_[256][4];
std::unordered_map<uint32_t, std::array<double, 2>> Pattern::prob_ptn_rsp_;

void Pattern::Init(std::string prob_dir) {
  // 1. Initializes pattern tables.
  Pattern ptn;

  for (int j = 0; j < 256; ++j) {
    ptn.set_stones(0x00aaaa00 | j);
    for (Color c = kColorZero; c < kNumColors; ++c) {
      count_ptn_[j][c] = ptn.CountImpl(c);
    }
  }

  for (uint32_t j = 0; j < 65536; ++j) {
    for (uint32_t k = 0; k < 256; ++k) {
      ptn.set_stones(0x00aa0000 | j | (k << 24));

      for (Color c = kColorZero; c < kNumPlayers; ++c) {
        if (ptn.LegalImpl(c)) {
          prob_ptn3x3_[j][k][c][0] = 1.0;
          prob_ptn3x3_[j][k][c][1] = 1.0;

          legal_ptn_[j & 0xff][k][c] = true;
        } else {
          prob_ptn3x3_[j][k][c][0] = 0.0;
          prob_ptn3x3_[j][k][c][1] = 1.0;

          legal_ptn_[j & 0xff][k][c] = false;
        }
      }
    }
  }

  prob_ptn_rsp_.clear();

  // 2. Imports pattern probability from files.

  std::ifstream ifs;
  std::string str;

  // 2-1. 3x3 patterns.
  ifs.open(JoinPath(prob_dir, "prob_ptn3x3.txt"));
  if (ifs.fail())
    std::cerr << "file could not be opened: prob_ptn3x3.txt" << std::endl;

  while (getline(ifs, str)) {
    std::string line_str;
    std::istringstream iss(str);

    getline(iss, line_str, ',');
    uint32_t stones = stoul(line_str);
    // Swaps color bits.
    stones = (stones & 0xff000000) | (stones ^ 0x00aaaaaa);

    std::array<double, 4> bf_prob;
    for (int i = 0; i < 4; ++i) {
      getline(iss, line_str, ',');
      bf_prob[i] = stod(line_str);
    }

    int stone_bf = stones & 0xffff;
    int atari_bf = stones >> 24;
    for (int j = 0; j < 4; ++j) {
      int color_id = j % 2;
      int restore_id = j < 2 ? 0 : 1;
      prob_ptn3x3_[stone_bf][atari_bf][color_id][restore_id] = bf_prob[j];
    }
  }
  ifs.close();

  // 2-2. Response patterns.
  ifs.open(JoinPath(prob_dir, "prob_ptn_rsp.txt"));
  if (ifs.fail())
    std::cerr << "file could not be opened: prob_ptn_rsp.txt" << std::endl;

  while (getline(ifs, str)) {
    std::string line_str;
    std::istringstream iss(str);

    getline(iss, line_str, ',');
    uint32_t stones = stoul(line_str);
    // Swaps color bits.
    stones = (stones & 0xff000000) | (stones ^ 0x00aaaaaa);

    std::array<double, 2> bf_prob;
    for (int i = 0; i < 2; ++i) {
      getline(iss, line_str, ',');
      bf_prob[i] = stod(line_str);
    }

    prob_ptn_rsp_.insert(std::make_pair(stones, bf_prob));
  }
  ifs.close();
}
