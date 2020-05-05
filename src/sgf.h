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

#ifndef SGF_H_
#define SGF_H_

#include <algorithm>
#include <string>
#include <vector>

#include "./board.h"
#include "./option.h"

/**
 * @class SgfData
 * SgfData class has player information, game results, and moves.
 * It inputs and outputs them to the sgf format game record.
 */
class SgfData {
 public:
  // Constructor
  SgfData() { Init(); }

  std::string player_name(Color c) const { return player_name_[c]; }

  double score() const { return score_; }

  void set_score(double val) { score_ = val; }

  Vertex move_at(int t) const { return move_history_[t]; }

  int game_ply() const { return move_history_.size(); }

  Color winner() const {
    return score_ == 0 ? kEmpty : score_ > 0 ? kBlack : kWhite;
  }

  bool resign_or_score() const {
    return game_ply() >= 12 && std::abs(score_) < 1024;
  }

  void Init() {
    komi_ = kBSize < 19 ? 7.0 : Options["komi"].get_double();
    handicap_ = 0;
    for (Color c = kColorZero; c < kNumPlayers; ++c) {
      player_name_[c] = "";
      player_rating_[c] = 2800;
      handicap_stones_[c].clear();
    }
    move_history_.clear();
    score_ = 0.0;
  }

  void Add(Vertex v) {
    ASSERT_LV3(v == kPass || (kRvtZero <= v2rv(v) && v2rv(v) < kNumRvts));
    move_history_.push_back(v);
  }

  /**
   * Returns Vertex converted from sgf-style string.
   *
   *  aa -> Vertex(22), i.e. (x,y) = (1,1)
   */
  Vertex sgf2v(std::string aa) const {
    // Returns kPass if input size is not 2.
    if (aa.size() != 2) return kPass;

    // Converts aa to (x,y) of RawVertex.
    char a0 = aa[0];
    char a1 = aa[1];
    int rx = isupper(a0) ? a0 - 'A' : a0 - 'a';
    int ry = isupper(a1) ? a1 - 'A' : a1 - 'a';

    if (rx < 0 || ry < 0 || rx >= kBSize || ry >= kBSize) return kPass;

    return rv2v(xy2rv(rx, ry));
  }

  void Read(std::string file_path);

  /**
   * Outputs the match information to an SGF file with comments.
   */
  void Write(std::string file_path,
             std::vector<std::string>* comments = nullptr) const;

  /**
   * Constructs board from sgf information.
   */
  bool ReconstructBoard(Board* b, int move_idx) const;

  /**
   * Imports all sgf files in the folder.
   */
  static int GetSgfFiles(std::string dir_path, std::vector<std::string>* files);

 private:
  double komi_;
  std::string player_name_[kNumPlayers];
  int player_rating_[kNumPlayers];
  int handicap_;
  std::vector<Vertex> handicap_stones_[kNumPlayers];
  std::vector<Vertex> move_history_;
  double score_;
};  // SgfData

#endif  // SGF_H_
