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

#include "./board.h"
#include "./option.h"

double Board::Score(double komi, OwnerMap* owner) const {
  // ASSERT_LV3( finished() );

  double score[kNumPlayers] = {0.0};
  bool visited[kNumVts] = {false};

  auto add_score = [&owner, &score](Color c, Vertex v) {
    ++score[c];
    if (owner != nullptr) (*owner)[c][v] += 1;
  };

  // Counts all stones and empties.
  for (RawVertex rv = kRvtZero; rv < kNumRvts; ++rv) {
    Vertex v = rv2v(rv);
    Color c = color_[v];

    if (!visited[v] && c < kNumPlayers) {
      visited[v] = true;
      add_score(c, v);
      for (const auto& dv : dv4) {
        Vertex v_nbr = v + dv;
        if (visited[v_nbr] || color_[v_nbr] != kEmpty) continue;
        visited[v_nbr] = true;
        if (ptn_[v_nbr].count(~c) == 0) add_score(c, v_nbr);
      }
    }
  }

  double abs_score = score[kBlack] - score[kWhite] - komi;
  if (Options["rule"].get_int() == kJapanese)
    abs_score += num_passes_[kBlack] - num_passes_[kWhite];

  return abs_score;
}

std::vector<std::pair<Vertex, std::vector<int>>> Board::GetAtariInfo() const {
  std::vector<std::pair<Vertex, std::vector<int>>> atari_info;
  bool checked[kNumVts] = {false};
  for (int i = 0; i < num_empties_; ++i) {
    Vertex v = empty_[i];
    if (!ptn_[v].atari()) continue;

    for (const auto& d : dir4) {
      if (ptn_[v].is_stone(d) && ptn_[v].atari_at(d)) {
        Vertex v_nbr = v + dir2v(d);
        if (checked[sg_id_[v_nbr]]) continue;
        checked[sg_id_[v_nbr]] = true;

        Color c_save = color_[v_nbr];
        std::vector<int> nbr_ids;

        Vertex v_tmp = v_nbr;
        do {
          for (const auto& dv : dv4) {
            Vertex v_nbr2 = v_tmp + dv;
            if (color_[v_nbr2] == ~c_save) nbr_ids.push_back(sg_id_[v_nbr2]);
          }

          v_tmp = next_v_[v_tmp];
        } while (v_tmp != v_nbr);

        sort(nbr_ids.begin(), nbr_ids.end());
        nbr_ids.erase(unique(nbr_ids.begin(), nbr_ids.end()), nbr_ids.end());

        atari_info.push_back({v_nbr, nbr_ids});
      }
    }
  }

  return std::move(atari_info);
}

std::vector<std::pair<Vertex, Color>> Board::NeedToBeFilled(
    int num_playouts, const OwnerMap& owner) const {
  std::vector<std::pair<Vertex, Color>> v_fills;
  auto final_color = [num_playouts, &owner](Vertex v) {
    double b_ratio = owner[kBlack][v] / num_playouts;
    double w_ratio = owner[kWhite][v] / num_playouts;
    if (b_ratio < 0.2 && w_ratio < 0.2) return kEmpty;
    return b_ratio >= w_ratio ? kBlack : kWhite;
  };

  auto induced_nakade_color = [this](Vertex v) {
    if (ptn_[v].count(kBlack) == 0 || ptn_[v].count(kWhite) == 0) return kEmpty;

    std::vector<int> sg_ids;
    int small_size = kNumVts;
    Color c_small = kEmpty;
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (!is_stone(color_[v_nbr])) continue;
      int id_ = sg_id_[v_nbr];
      if (sg_[id_].num_liberties() != 3) return kEmpty;

      if (std::find(sg_ids.begin(), sg_ids.end(), id_) == sg_ids.end()) {
        sg_ids.push_back(id_);
        if (sg_[id_].size() < small_size) {
          small_size = sg_[id_].size();
          c_small = color_[v_nbr];
        }
      }

      if (sg_ids.size() > 2) return kEmpty;
    }

    if (small_size >= 5) return kEmpty;

    Bitboard libs;
    for (auto& id_ : sg_ids) libs.Merge(sg_[id_].bb_liberties());
    if (libs.num_bits() != 3) return kEmpty;

    Board b_ = *this;
    b_.MakeMove<kOneWay>(v);
    for (auto& v_lib : libs.Vertices()) {
      if (v_lib == v) continue;
      if (b_.IsSelfAtariNakade(v_lib)) return c_small;
    }

    return kEmpty;
  };

  for (RawVertex rv = kRvtZero; rv < kNumRvts; ++rv) {
    Vertex v = rv2v(rv);
    if (color_[v] != kEmpty) continue;
    if (final_color(v) == kEmpty) continue;

    bool has_nbr_colors[kNumPlayers + 1] = {false};
    for (const auto& dv : dv4) {
      Color c = final_color(v + dv);
      has_nbr_colors[c] = true;
    }

    if (has_nbr_colors[kBlack] && has_nbr_colors[kWhite]) {
      Color c = induced_nakade_color(v);
      v_fills.push_back({v, c});
    }
  }

  auto atari_info = GetAtariInfo();
  for (auto& ai : atari_info) {
    Vertex v_save = ai.first;
    auto& nbr_ids = ai.second;

    Color c_esc = color_[v_save];
    Color c_cap = ~c_esc;
    Color c_esc_final = final_color(v_save);
    Vertex v_atr = sg_[sg_id_[v_save]].liberty_atari();

    // v_save is dead.
    if (c_esc_final != c_esc) continue;

    // Checks whether surrounding stones of opponent remain.
    bool remain_cap = false;
    for (auto id_ : nbr_ids) {
      Color c_cap_final = final_color((Vertex)id_);
      if (c_cap_final == c_cap) {
        // Checks utte-gaeshi.
        if (sg_[sg_id_[v_save]].size() == 1) {
          bool has_same_lib = false;
          for (const auto& d : dir4) {
            if (ptn_[v_atr].color_at(d) == c_cap && ptn_[v_atr].atari_at(d) &&
                sg_[sg_id_[v_atr + dir2v(d)]].size() > 1)
              has_same_lib = true;
          }

          if (has_same_lib) {
            Board b_ = *this;
            if (b_.us_ == c_esc) b_.MakeMove<kOneWay>(kPass);
            if (b_.IsLegal(v_atr)) {
              b_.MakeMove<kOneWay>(v_atr);
              if (b_.ptn_[v_save].atari()) break;  // Utte-gaeshi
            }
          }
        }

        v_fills.push_back({v_atr, c_esc});
        break;
      }
    }
  }

  // Fills in seki.
  bool checked[kNumVts] = {false};
  for (int i = 0; i < num_empties_; ++i) {
    Vertex v = empty_[i];
    if (ptn_[v].count(kBlack) == 0 || ptn_[v].count(kWhite) == 0) continue;
    if (final_color(v) != kEmpty) continue;

    std::deque<Vertex> que;
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (is_stone(color_[v_nbr]) && !checked[sg_id_[v_nbr]]) {
        checked[sg_id_[v_nbr]] = true;
        que.push_back(v_nbr);
      }
    }

    while (!que.empty()) {
      Vertex v_nbr = que.front();
      que.pop_front();

      Color c_nbr = color_[v_nbr];
      Vertex v_tmp = v_nbr;
      do {
        bool found = false;
        for (const auto& dv : dv4) {
          Vertex v_nbr2 = v_tmp + dv;
          Color c_nbr2 = color_[v_nbr2];
          if (c_nbr2 == kEmpty && IsFalseEye(v_nbr2)) {
            for (const auto& dv_ : dv4) {
              Vertex v_nbr3 = v_nbr2 + dv_;
              if (is_stone(color_[v_nbr3]) && !checked[sg_id_[v_nbr3]]) {
                checked[sg_id_[v_nbr3]] = true;
                que.push_back(v_nbr3);
              }
            }
          } else if (c_nbr2 == ~c_nbr && sg_[sg_id_[v_nbr2]].atari()) {
            v_fills.push_back({sg_[sg_id_[v_nbr2]].liberty_atari(), c_nbr});
            found = true;
            break;
          }
        }
        if (found) break;

        v_tmp = next_v_[v_tmp];
      } while (v_tmp != v_nbr);
    }
  }

  return std::move(v_fills);
}

std::unordered_map<double, int> Board::RolloutScores(
    int num_playouts, Vertex next_move, double thr_rate, bool add_moves,
    bool add_null_pass, OwnerMap* owner) const {
  std::unordered_map<double, int> scores;
  double komi = Options["komi"].get_double();

  Board b_base = *this;
  if (next_move <= kPass)
    b_base.MakeMove<kOneWay>(next_move);
  else if (add_null_pass)
    b_base.num_passes_[b_base.opp_]++;

  // Chinese rule
  if (Options["rule"].get_int() != kJapanese) {
    Board b_cpy;
    for (int i = 0; i < num_playouts; ++i) {
      b_cpy = b_base;
      b_cpy.Rollout(komi);
      double s = b_cpy.Score(komi, owner);
      if (scores.count(s) == 0)
        scores[s] = 1;
      else
        scores[s]++;
    }
    return std::move(scores);
  }

  auto atari_info = GetAtariInfo();
  std::vector<Vertex> escaped_stones;

  for (auto& ai : atari_info) {
    Vertex v_save = ai.first;
    Color c_esc = color_[v_save];
    Vertex v_atr = sg_[sg_id_[v_save]].liberty_atari();
    if (add_moves && ptn_[v_atr].enclosed_by(c_esc)) {
      if (next_move == v_atr) {
        if (us_ == c_esc) escaped_stones.push_back(v_save);
      } else if (b_base.us_ == c_esc) {
        if (b_base.IsLegal(v_atr)) {
          b_base.MakeMove<kOneWay>(v_atr);
          escaped_stones.push_back(v_save);
        }
      } else {
        b_base.MakeMove<kOneWay>(kPass);
        if (b_base.IsLegal(v_atr)) {
          b_base.MakeMove<kOneWay>(v_atr);
          escaped_stones.push_back(v_save);
        } else {
          b_base.MakeMove<kOneWay>(kPass);
        }
      }
    }
  }

  Board b_rollout;
  double total_wins = 0.0;

  for (int i = 0; i < num_playouts; ++i) {
    b_rollout = b_base;
    b_rollout.Rollout(komi);
    double s = b_rollout.Score(komi, owner);
    bool ignore_once = add_null_pass;

    // Corrects false eyes.
    for (auto& ai : atari_info) {
      Vertex v_save = ai.first;
      auto& nbr_ids = ai.second;
      if (nbr_ids.empty()) continue;

      Color c_cap = color_[nbr_ids[0]];
      Color c_esc = ~c_cap;
      Vertex v_atr = sg_[sg_id_[v_save]].liberty_atari();
      if (!add_moves && v_atr == next_move) continue;

      bool escaped = std::find(escaped_stones.begin(), escaped_stones.end(),
                               v_save) != escaped_stones.end();
      double ds = c_esc == kBlack ? 1 : -1;

      // v_save is dead even if escaped.
      if (b_rollout.color_[v_save] != c_esc) {
        if (escaped) s += ds;  // not sensible escape
        continue;
      }

      // Checks whether surrounding stones of opponent remain.
      bool remain_cap = false;
      for (auto id_ : nbr_ids)
        if (b_rollout.color_[id_] == c_cap) {
          remain_cap = true;
          break;
        }

      if (remain_cap) {
        if (c_esc == us_ && ignore_once)
          ignore_once = false;  // Ignores correction once.
        else if (!escaped)
          s -= ds;  // Sensible escape
      } else if (escaped) {
        s += ds;  // Not sensible escape
      }
    }

    Color winner = (s == 0 ? kEmpty : s > 0 ? kBlack : kWhite);
    total_wins += winner == us_ ? 1 : winner == kEmpty ? 0.5 : 0.0;

    if (scores.count(s) == 0)
      scores[s] = 1;
    else
      scores[s]++;

    // Early termination.
    if (thr_rate != -1 && prev_move_[opp_] != kPass && i < num_playouts - 1) {
      int num_remain_playouts = num_playouts - i - 1;
      double win_rate_best = (total_wins + num_remain_playouts) / num_playouts;
      if (next_move == kPass && win_rate_best >= 0.95) continue;

      // Cannot exceed thr_rate in kPass case.
      if (next_move == kPass && win_rate_best < thr_rate)
        return std::move(scores);
      // Already exceeded thr_rate in null-pass case.
      if (next_move != kPass && total_wins / num_playouts > thr_rate)
        return std::move(scores);

      // Half-point score for win.
      double half_score = us_ == kBlack ? 0.5 : -0.5;
      if (next_move != kPass)
        half_score = -half_score;  // For losing at null-pass.
      int num_half_games = scores.count(half_score)
                               ? scores[half_score] + num_remain_playouts
                               : num_remain_playouts;

      int max_score_games = 0;
      for (auto& score_and_games : scores)
        max_score_games = std::max(max_score_games, score_and_games.second);

      // Half-point score cannot exceed another score.
      if (max_score_games > num_half_games) return std::move(scores);
    }
  }

  return std::move(scores);
}

void Board::PrintOwnerMap(double s, int num_playouts, const OwnerMap& owner,
                          std::vector<std::ostream*> os_list) const {
  std::string str_x = "ABCDEFGHJKLMNOPQRST";  // Note that excluding "I".
  std::stringstream ss;

  // 1. Displays dead stones.
  ss << "\ndead stones ([ ] = dead)\n";
  ss << "  ";
  for (int x = 0; x < kBSize; ++x) ss << " " << str_x[x] << " ";
  ss << std::endl;

  for (int y = 0; y < kBSize; ++y) {
    if (kBSize - y < 10) ss << " ";
    ss << kBSize - y;

    for (int x = 0; x < kBSize; ++x) {
      Vertex v = xy2v(x + 1, kBSize - y);
      bool is_star = false;
#if (BOARD_SIZE == 13 || BOARD_SIZE == 19)
      if ((x == 3 || x == kBSize / 2 || x == kBSize - 1 - 3) &&
          (y == 3 || y == kBSize / 2 || y == kBSize - 1 - 3))
#else
      if (((x == 2 || x == kBSize - 1 - 2) &&
           (y == 2 || y == kBSize - 1 - 2)) ||
          (x == kBSize / 2 && y == kBSize / 2))
#endif
        is_star = true;

      bool dead =
          color_[v] < kNumPlayers && owner[color_[v]][v] / num_playouts < 0.5;
      if (color_[v] == kWhite)
        ss << (dead ? "[O]" : " O ");
      else if (color_[v] == kBlack)
        ss << (dead ? "[X]" : " X ");
      else if (is_star)
        ss << " + ";  // Star
      else
        ss << " . ";
    }

    if (kBSize - y < 10) ss << " ";
    ss << kBSize - y << std::endl;
  }

  ss << "  ";
  for (int x = 0; x < kBSize; ++x) ss << " " << str_x[x] << " ";
  ss << std::endl;

  // 2. Displays occupied areas.
  ss << "\narea ([ ] = black area, ? ? = unknown)\n";

  ss << "  ";
  for (int x = 0; x < kBSize; ++x) ss << " " << str_x[x] << " ";
  ss << std::endl;

  for (int y = 0; y < kBSize; ++y) {
    if (kBSize - y < 10) ss << " ";
    ss << kBSize - y;

    for (int x = 0; x < kBSize; ++x) {
      Vertex v = xy2v(x + 1, kBSize - y);
      bool is_star = false;
#if (BOARD_SIZE == 13 || BOARD_SIZE == 19)
      if ((x == 3 || x == kBSize / 2 || x == kBSize - 1 - 3) &&
          (y == 3 || y == kBSize / 2 || y == kBSize - 1 - 3))
#else
      if (((x == 2 || x == kBSize - 1 - 2) &&
           (y == 2 || y == kBSize - 1 - 2)) ||
          (x == kBSize / 2 && y == kBSize / 2))
#endif
        is_star = true;

      double black_ratio = owner[kBlack][v] / num_playouts;
      double white_ratio = owner[kWhite][v] / num_playouts;
      bool unknown = black_ratio == white_ratio ||
                     (black_ratio < 0.2 && white_ratio < 0.2);
      bool black_area = owner[kBlack][v] / num_playouts > 0.5;

      if (color_[v] == kWhite)
        ss << (black_area ? "[O]" : unknown ? "?O?" : " O ");
      else if (color_[v] == kBlack)
        ss << (black_area ? "[X]" : unknown ? "?X?" : " X ");
      else if (is_star)
        ss << (black_area ? "[+]" : unknown ? "?+?" : " + ");
      else
        ss << (black_area ? "[.]" : unknown ? "?.?" : " . ");
    }

    if (kBSize - y < 10) ss << " ";
    ss << kBSize - y << std::endl;
  }

  ss << "  ";
  for (int x = 0; x < kBSize; ++x) ss << " " << str_x[x] << " ";
  ss << std::endl;

  // 3. Shows final results
  std::string str_winner = s == 0 ? "" : s > 0 ? "B+" : "W+";
  ss << "result: " << std::fixed << std::setprecision(1);
  if (s > 0)
    ss << "B+" << std::fabs(s) << std::endl;
  else if (s < 0)
    ss << "W+" << std::fabs(s) << std::endl;
  else
    ss << "0" << std::endl;

  // 4. Outputs to ostream.
  for (auto& os : os_list) *os << ss.str();
}

std::string Board::FinalResult(double komi,
                               std::vector<std::ostream*> os_list) const {
  OwnerMap owner = {0};
  double total_black_wins = 0.0;
  const int num_playouts = 1000;
  auto atari_info = GetAtariInfo();

  auto scores = RolloutScores(num_playouts, kVtNull, -1, true, false, &owner);
  for (auto& score_and_games : scores) {
    double score = score_and_games.first;
    int games = score_and_games.second;
    if (score > 0)
      total_black_wins += games;
    else if (score == 0)
      total_black_wins += 0.5 * games;
  }

  // 2. count taken stones for Japanese rule.
  int score_jp[2] = {0, 0};
  int score_cn[2] = {0, 0};
  int agehama[2];
  for (int i = 0; i < 2; ++i) {
    agehama[i] = ply_ / 2 - num_stones_[i];
    agehama[i] += static_cast<int>(i == 1) * static_cast<int>(ply_ % 2 == 1) -
                  num_passes_[i];
  }

  for (int y = 0; y < kBSize; ++y) {
    for (int x = 0; x < kBSize; ++x) {
      Vertex v = xy2v(x + 1, kBSize - y);
      double black_ratio = owner[kBlack][v] / num_playouts;
      double white_ratio = owner[kWhite][v] / num_playouts;
      bool unknown = black_ratio == white_ratio ||
                     (black_ratio < 0.2 && white_ratio < 0.2);
      bool black_area = owner[kBlack][v] / num_playouts > 0.5;

      if (!unknown) {
        Color cv = black_area ? kBlack : kWhite;
        if (color_[v] == ~cv) {
          ++agehama[~cv];
          ++score_jp[cv];
        } else if (color_[v] == kEmpty) {
          ++score_jp[cv];
        }
        ++score_cn[cv];
      }
    }
  }

  // Gathers false eyes remaining.
  for (auto& ai : atari_info) {
    Vertex v_save = ai.first;
    auto& nbr_ids = ai.second;
    if (nbr_ids.empty()) continue;

    Color c_cap = color_[nbr_ids[0]];
    Color c_esc = ~c_cap;
    bool remain_cap = false;
    for (auto id_ : nbr_ids)
      if (owner[c_cap][id_] / num_playouts > 0.5) remain_cap = true;

    if (remain_cap && owner[c_esc][v_save] / num_playouts > 0.5)
      --score_jp[c_esc];
  }

  double abs_score = 0;
  if (Options["rule"].get_int() == kJapanese)
    abs_score = std::abs((score_jp[kBlack] - agehama[kBlack]) -
                         (score_jp[kWhite] - agehama[kWhite]) - komi);
  else
    abs_score = std::abs(score_cn[kBlack] - score_cn[kWhite] - komi);

  // 4. Shows final results.
  Color winner = abs_score == 0
                     ? kEmpty
                     : total_black_wins / num_playouts > 0.5 ? kBlack : kWhite;
  double s = winner == kWhite ? -abs_score : abs_score;
  PrintOwnerMap(s, num_playouts, owner, os_list);

  std::stringstream ss;
  if (abs_score == 0) {
    ss << "0";
  } else {
    ss << std::fixed << std::setprecision(1);
    ss << (winner == kBlack ? "B+" : "W+") << abs_score;
  }

  return ss.str();
}

RepetitionState Board::CheckRepetition(Vertex v) const {
  ASSERT_LV2(v == kPass || color_[v] == kEmpty);

  if (v == kPass || !ptn_[v].atari()) return kRepetitionNone;

  RepetitionRule rule = RepetitionRule(Options["repetition_rule"].get_int());
  Key key_next = hash_key_;
  key_next ^= kCoordTable.zobrist_table[0][us_][v];
  key_next ^= 1;
  if (ko_ != kVtNull) key_next ^= kCoordTable.zobrist_table[0][3][ko_];

  int num_removed_stones = 0;
  std::vector<int> remove_ids;

  for (const auto& d : dir4) {
    Vertex v_nbr = v + dir2v(d);
    if (color_[v_nbr] == opp_ && ptn_[v].atari_at(d)) {
      if (std::find(remove_ids.begin(), remove_ids.end(), sg_id_[v_nbr]) ==
          remove_ids.end()) {
        remove_ids.push_back(sg_id_[v_nbr]);
        num_removed_stones += sg_[sg_id_[v_nbr]].size();
        Vertex v_tmp = v_nbr;
        do {
          key_next ^= kCoordTable.zobrist_table[0][opp_][v_tmp];
          v_tmp = next_v_[v_tmp];
        } while (v_tmp != v_nbr);
      }
    }
  }

  if (ptn_[v].enclosed_by(opp_) && num_removed_stones == 1)
    key_next ^= kCoordTable.zobrist_table[0][3][remove_ids[0]];  // Ko

  if (rule == kRepRuleDraw || rule == kRepRuleSuperKo) {
    for (int i = 0; i < 8; i = i + 2) {  // Checks opponent side.
      if (key_next == key_history_[i])
        return rule == kRepRuleDraw ? kRepetitionDraw : kRepetitionLose;
    }
  } else {  // kTrompTraylor
    for (int i = 0; i < 8; ++i) {
      // Checks both sides / i == 0 is same side as opponent.
      if (key_next == key_history_[i]) return kRepetitionLose;
      key_next ^= 1;  // Ignore side.
    }
  }

  return kRepetitionNone;
}

/**
 * Returns vertices of escape from ladder.
 */
std::vector<Vertex> Board::LadderEscapes(int num_escapes) {
  std::function<bool(Vertex, int, int&)> try_ladder =
      [this, &try_ladder](Vertex v_atr, int depth, int& max_depth) {
        Color c = color_[v_atr];
        Vertex v_esc = sg_[sg_id_[v_atr]].liberty_atari();

        if (depth >= 64 || us_ != c) return false;

        int esc_depth = depth;
        int cap_depth = depth;

        // 1. Checks whether surrounding stones can be taken.
        std::vector<Vertex> v_caps;
        Vertex v_tmp = v_atr;
        do {
          for (const auto& dv : dv4) {
            // Checks whether surrounding stones can be taken.
            Vertex v_nbr = v_tmp + dv;
            if (color_[v_nbr] == ~c && sg_[sg_id_[v_nbr]].atari() &&
                IsLegal(sg_[sg_id_[v_nbr]].liberty_atari())) {
              v_caps.push_back(sg_[sg_id_[v_nbr]].liberty_atari());
            }
          }
          v_tmp = next_v_[v_tmp];
        } while (v_tmp != v_atr);

        // 2. Searchs after capturing the surrounding stones.
        if (!v_caps.empty()) {
          // Removes duplicated indexes.
          sort(v_caps.begin(), v_caps.end());
          v_caps.erase(unique(v_caps.begin(), v_caps.end()), v_caps.end());

          for (auto v_cap : v_caps) {
            bool is_ladder = false;
            // Captures opponent's stones.
            MakeMove<kQuick>(v_cap);

            if (sg_[sg_id_[v_atr]].num_liberties() == 2) {
              ASSERT_LV2(sg_[sg_id_[v_atr]].pre_atari());
              int min_depth = 64;

              for (auto lib : sg_[sg_id_[v_atr]].lib_vertices()) {
                if (IsLegal(lib)) {
                  MakeMove<kQuick>(lib);
                  // Recursive search.
                  ASSERT_LV2(sg_[sg_id_[v_atr]].atari());
                  int tmp_depth = depth + 1;
                  if (try_ladder(v_atr, depth + 1, tmp_depth)) {
                    is_ladder = true;
                    min_depth = std::min(min_depth, tmp_depth);
                  }
                  UnmakeMove<kQuick>();
                }

                if (is_ladder && depth > 4) break;
              }

              cap_depth = std::max(cap_depth, min_depth);
            } else if (sg_[sg_id_[v_atr]].num_liberties() < 2) {
              is_ladder = true;
            }

            UnmakeMove<kQuick>();

            if (!is_ladder) return false;
          }
        }

        // 3. Escapes to v_esc.
        ASSERT_LV2(color_[v_esc] == kEmpty);

        if (!IsLegal(v_esc)) {
          max_depth = std::max(esc_depth, depth);
          return true;
        }

        MakeMove<kQuick>(v_esc);

        // 4. Counts liberty vertexes after placing the stone.
        bool is_ladder = false;

        if (sg_[sg_id_[v_atr]].num_liberties() <= 1) {
          is_ladder = true;  // Captured
        } else if (sg_[sg_id_[v_atr]].num_liberties() > 2) {
          is_ladder = false;  // Survives
        } else {
          ASSERT_LV2(sg_[sg_id_[v_atr]].pre_atari());
          int min_depth = 64;

          // 5. Plays the move making the StoneGroupe in atari.
          for (auto lib : sg_[sg_id_[v_atr]].lib_vertices()) {
            ASSERT_LV2(color_[lib] == kEmpty);

            if (IsLegal(lib)) {
              MakeMove<kQuick>(lib);
              // Recursive search.

              ASSERT_LV2(sg_[sg_id_[v_atr]].atari());
              int tmp_depth = depth + 1;
              if (try_ladder(v_atr, depth + 1, tmp_depth)) {
                is_ladder = true;
                min_depth = std::min(min_depth, tmp_depth);
              }
              UnmakeMove<kQuick>();
            }

            if (is_ladder && depth > 4) break;
          }

          esc_depth = std::max(esc_depth, min_depth);
        }

        UnmakeMove<kQuick>();
        if (is_ladder) max_depth = std::max(esc_depth, cap_depth);

        return is_ladder;
      };  // try_ladder

  auto wasteful_escape = [this, &try_ladder](Vertex v) {
    ASSERT_LV2(color_[v] == kEmpty);

    if (!ptn_[v].atari() || ptn_[v].count(us_) == 0 ||
        ptn_[v].count(kEmpty) == 3 || !IsLegal(v))
      return 0;

    std::vector<int> atr_ids;
    // 1. Checks neighboring 4 positions.
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (color_[v_nbr] == us_) {
        // Not ladder when the neighboring c's StoneGroup has
        // more than 4 liberty vertexes.
        if (sg_[sg_id_[v_nbr]].num_liberties() >= 4)
          return 0;
        else if (sg_[sg_id_[v_nbr]].atari())
          atr_ids.push_back(sg_id_[v_nbr]);
      }
    }

    if (!atr_ids.empty()) {
      // Removes duplicated indexes.
      sort(atr_ids.begin(), atr_ids.end());
      atr_ids.erase(unique(atr_ids.begin(), atr_ids.end()), atr_ids.end());

      // 2. Returns true if any of the Rens in Atari is taken with Ladder.
      for (auto atr_id : atr_ids) {
        int max_depth = 0;
        ASSERT_LV2(sg_[atr_id].atari());
        if (try_ladder((Vertex)atr_id, 0, max_depth)) return max_depth;
      }
    }

    return 0;
  };

  std::vector<Vertex> escape_vertices;

  for (int i = 0, n = num_empties_; i < n; ++i) {
    Vertex v = empty_[i];
    if (wasteful_escape(v) >= num_escapes) escape_vertices.push_back(v);
  }

  return std::move(escape_vertices);
}
