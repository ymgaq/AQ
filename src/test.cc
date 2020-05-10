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

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "./test.h"

/**
 * Displays and terminates abnormally when the member variables t1 and t2 of
 * board do not match.
 */
template <typename T>
void CheckEqual(const Board& b1, const Board& b2, std::string member_str,
                const T t1, const T t2, int idx = -1) {
  if (!(t1 == t2)) {
    std::cout << member_str << std::endl;
    if (idx >= 0) std::cout << "idx: " << idx << std::endl;
    std::cout << "b1, b2=\n" << t1 << std::endl << t2 << std::endl;
    std::cout << "b1\n" << b1 << std::endl << "b2\n" << b2 << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
    exit(1);
  }
}

/**
 * Check that b1 and b2 have the same member variable.
 * A friend function of Board class.
 */
bool IdentifyBoards(const Board& b1, const Board& b2) {
  CheckEqual(b1, b2, "us_", b1.us_, b2.us_);
  CheckEqual(b1, b2, "opp", b1.opp_, b2.opp_);
  CheckEqual(b1, b2, "num_stones_[kBlack]", b1.num_stones_[kBlack],
             b2.num_stones_[kBlack]);
  CheckEqual(b1, b2, "num_stones_[kWhite]", b1.num_stones_[kWhite],
             b2.num_stones_[kWhite]);
  CheckEqual(b1, b2, "num_empties_", b1.num_empties_, b2.num_empties_);
  CheckEqual(b1, b2, "ko_", b1.ko_, b2.ko_);
  CheckEqual(b1, b2, "ply_", b1.ply_, b2.ply_);
  CheckEqual(b1, b2, "num_passes_[kBlack]", b1.num_passes_[kBlack],
             b2.num_passes_[kBlack]);
  CheckEqual(b1, b2, "num_passes_[kWhite]", b1.num_passes_[kWhite],
             b2.num_passes_[kWhite]);
  CheckEqual(b1, b2, "prev_move_[kBlack]", b1.prev_move_[kBlack],
             b2.prev_move_[kBlack]);
  CheckEqual(b1, b2, "prev_move_[kWhite]", b1.prev_move_[kWhite],
             b2.prev_move_[kWhite]);
  CheckEqual(b1, b2, "prev_ko_", b1.prev_ko_, b2.prev_ko_);
  CheckEqual(b1, b2, "removed_stones_", b1.removed_stones_, b2.removed_stones_);
  CheckEqual(b1, b2, "prev_ptn_[kBlack]", b1.prev_ptn_[kBlack],
             b2.prev_ptn_[kBlack]);
  CheckEqual(b1, b2, "prev_ptn_[kWhite]", b1.prev_ptn_[kWhite],
             b2.prev_ptn_[kWhite]);
  CheckEqual(b1, b2, "response_move_[0]", b1.response_move_[0],
             b2.response_move_[0]);
  CheckEqual(b1, b2, "response_move_[1]", b1.response_move_[1],
             b2.response_move_[1]);
  CheckEqual(b1, b2, "response_move_[2]", b1.response_move_[2],
             b2.response_move_[2]);
  CheckEqual(b1, b2, "response_move_[3]", b1.response_move_[3],
             b2.response_move_[3]);
  CheckEqual(b1, b2, "prev_rsp_prob_", b1.prev_rsp_prob_, b2.prev_rsp_prob_);
  CheckEqual(b1, b2, "hash_key_", b1.hash_key_, b2.hash_key_);
  for (int i = 0; i < 8; ++i)
    CheckEqual(b1, b2, "key_history_[i]", b1.key_history_[i],
               b2.key_history_[i], i);
  for (int i = 0; i < b1.game_ply(); ++i)
    CheckEqual(b1, b2, "move_history_[i]", b1.move_history_[i],
               b2.move_history_[i], i);
  for (int i = 0; i < kBSize; ++i) {
    CheckEqual(b1, b2, "sum_prob_rank_[kBlack][i]",
               b1.sum_prob_rank_[kBlack][i], b2.sum_prob_rank_[kBlack][i], i);
    CheckEqual(b1, b2, "sum_prob_rank_[kWhite][i]",
               b1.sum_prob_rank_[kWhite][i], b2.sum_prob_rank_[kWhite][i], i);
  }

  for (int i = 0; i < kNumVts; ++i) {
    CheckEqual(b1, b2, "color_[i]", b1.color_[i], b2.color_[i], i);
    CheckEqual(b1, b2, "empty_id_[i]", b1.empty_id_[i], b2.empty_id_[i], i);
    CheckEqual(b1, b2, "sg_id_[i]", b1.sg_id_[i], b2.sg_id_[i], i);
    CheckEqual(b1, b2, "sg_[i]", b1.sg_[i], b2.sg_[i], i);
    CheckEqual(b1, b2, "next_v[i]", b1.next_v_[i], b2.next_v_[i], i);
    CheckEqual(b1, b2, "ptn_[i]", b1.ptn_[i], b2.ptn_[i], i);
    CheckEqual(b1, b2, "prob_[kBlack][i]", b1.prob_[kBlack][i],
               b2.prob_[kBlack][i], i);
    CheckEqual(b1, b2, "prob_[kWhite][i]", b1.prob_[kWhite][i],
               b2.prob_[kWhite][i], i);
  }

  for (int i = 0; i < kNumRvts; ++i)
    CheckEqual(b1, b2, "empty_[i]", b1.empty_[i], b2.empty_[i], i);

  CheckEqual(b1, b2, "feature_", b1.feature_, b2.feature_);

  return true;
}

/**
 * Displays the top five, bottom five, and middle five pairs of 3x3 patterns and
 * response patterns.
 * A friend function of Patten class.
 */
void PrintPatternProb() {
  auto comp = [](double d1, double d2) {
    return d2 * 0.999 < d1 && d1 < d2 * 1.001;
  };

  Pattern tmp_ptn;
  std::vector<std::pair<double, uint32_t>> p3x3s[kNumPlayers];
  std::vector<std::pair<double, uint32_t>> prsps;

  for (int i = 0; i < 65536; ++i)
    for (int j = 0; j < 256; ++j) {
      uint32_t stones = (0x00aa0000 | i | (j << 24));

      if (!(Pattern::prob_ptn3x3_[i][j][kWhite][0] == 0 ||
            comp(Pattern::prob_ptn3x3_[i][j][kWhite][0],
                 1 / Pattern::prob_ptn3x3_[i][j][kWhite][1]))) {
        std::cout << Pattern(stones) << std::endl;
        std::cout << Pattern::prob_ptn3x3_[i][j][kWhite][0] << ","
                  << 1 / Pattern::prob_ptn3x3_[i][j][kWhite][1] << std::endl;
        std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
        exit(1);
      }
      if (!(Pattern::prob_ptn3x3_[i][j][kBlack][0] == 0 ||
            comp(Pattern::prob_ptn3x3_[i][j][kBlack][0],
                 1 / Pattern::prob_ptn3x3_[i][j][kBlack][1]))) {
        std::cout << Pattern(stones) << std::endl;
        std::cout << Pattern::prob_ptn3x3_[i][j][kBlack][0] << ","
                  << 1 / Pattern::prob_ptn3x3_[i][j][kBlack][1] << std::endl;
        std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
        exit(1);
      }

      if (Pattern(stones).legal(kWhite))
        p3x3s[kWhite].push_back(
            {Pattern::prob_ptn3x3_[i][j][kWhite][0], stones});
      if (Pattern(stones).legal(kBlack))
        p3x3s[kBlack].push_back(
            {Pattern::prob_ptn3x3_[i][j][kBlack][0], stones});
    }

  for (auto& pr : Pattern::prob_ptn_rsp_)
    prsps.push_back({pr.second[0], pr.first});

  std::sort(p3x3s[kWhite].begin(), p3x3s[kWhite].end(),
            std::greater<std::pair<double, uint32_t>>());
  std::sort(p3x3s[kBlack].begin(), p3x3s[kBlack].end(),
            std::greater<std::pair<double, uint32_t>>());
  std::sort(prsps.begin(), prsps.end(),
            std::greater<std::pair<double, uint32_t>>());

  std::cout << "\npattern3x3 best 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << "White\n"
              << Pattern(p3x3s[kWhite][i].second) << std::endl
              << p3x3s[kWhite][i].first << std::endl;
    std::cout << "Black\n"
              << Pattern(p3x3s[kBlack][i].second) << std::endl
              << p3x3s[kBlack][i].first << std::endl;
  }

  std::cout << "\npattern3x3 middle 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << "White\n"
              << Pattern(p3x3s[kWhite][p3x3s[kWhite].size() / 2 + i].second)
              << std::endl
              << p3x3s[kWhite][p3x3s[kWhite].size() / 2 + i].first << std::endl;
    std::cout << "Black\n"
              << Pattern(p3x3s[kBlack][p3x3s[kBlack].size() / 2 + i].second)
              << std::endl
              << p3x3s[kBlack][p3x3s[kBlack].size() / 2 + i].first << std::endl;
  }

  std::cout << "\npattern3x3 worst 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << "White\n"
              << Pattern(p3x3s[kWhite][p3x3s[kWhite].size() - 1 - i].second)
              << std::endl
              << p3x3s[kWhite][p3x3s[kWhite].size() - 1 - i].first << std::endl;
    std::cout << "Black\n"
              << Pattern(p3x3s[kBlack][p3x3s[kBlack].size() - 1 - i].second)
              << std::endl
              << p3x3s[kBlack][p3x3s[kBlack].size() - 1 - i].first << std::endl;
  }

  std::cout << "\npattern_rsp best 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << Pattern(prsps[i].second) << std::endl
              << prsps[i].first << std::endl;
  }

  std::cout << "\npattern_rsp middle 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << Pattern(prsps[prsps.size() / 2 + i].second) << std::endl
              << prsps[prsps.size() / 2 + i].first << std::endl;
  }

  std::cout << "\npattern_rsp worst 5\n";
  for (int i = 0; i < 5; ++i) {
    std::cout << "#" << i + 1 << std::endl;
    std::cout << Pattern(prsps[prsps.size() - 1 - i].second) << std::endl
              << prsps[prsps.size() - 1 - i].first << std::endl;
  }
}

/**
 * Checks to see if Nakade is registered correctly.
 */
void CheckNakade() {
  auto rot = [](Direction d) {
    Direction rot_table[] = {
        kDirR, kDirD, kDirL, kDirU, kDirRU, kDirRD, kDirLD, kDirLU,
    };
    return rot_table[d];
  };

  auto mir = [](Direction d) {
    Direction rot_table[] = {
        kDirU, kDirL, kDirD, kDirR, kDirRU, kDirLU, kDirLD, kDirRD,
    };
    return rot_table[d];
  };

  Direction ndirs[7][5] = {
      {kDirU, kDirD, kNumDirs, kNumDirs, kNumDirs},
      {kDirU, kDirR, kNumDirs, kNumDirs, kNumDirs},
      {kDirU, kDirR, kDirL, kNumDirs, kNumDirs},
      {kDirU, kDirR, kDirRU, kNumDirs, kNumDirs},
      {kDirU, kDirR, kDirRU, kDirL, kNumDirs},
      {kDirU, kDirR, kDirD, kDirL, kNumDirs},
      {kDirU, kDirR, kDirD, kDirL, kDirRU},
  };

  std::unordered_set<uint64_t> nakade_tmp;

  for (Vertex v = kVtZero; v < kNumVts; ++v) {
    if (in_wall(v)) continue;

    Direction tmp_ndirs[7][5];
    std::memcpy(tmp_ndirs, ndirs, sizeof(ndirs));

    for (int i = 0; i < 8; ++i) {
      for (int j = 0; j < 7; ++j) {
        uint64_t space = 0;
        AddEmptyHash(&space, v);
        bool out_board = false;
        for (int k = 0; k < 5; ++k) {
          if (tmp_ndirs[j][k] != kNumDirs) {
            Vertex v_nbr = v + dir2v(tmp_ndirs[j][k]);
            AddEmptyHash(&space, v_nbr);
            out_board |= in_wall(v_nbr);

            tmp_ndirs[j][k] = rot(tmp_ndirs[j][k]);
            if (i == 3) tmp_ndirs[j][k] = mir(tmp_ndirs[j][k]);
          }
        }

        bool hit =
            kCoordTable.nakade_map.find(space) != kCoordTable.nakade_map.end();
        Vertex vital = hit ? kCoordTable.nakade_map.at(space) : kVtNull;
        bool vital_match =
            (vital == v) || (vital != kVtNull && j == 3 && dist(v, vital) <= 3);

        if ((out_board && hit) || (!out_board && !hit) ||
            (hit && !vital_match)) {
          std::cout << "v=" << v << ", i=" << i << ", j=" << j << std::endl;
          std::cout << "out_board: " << out_board << " hit: " << hit;
          std::cout << " vital: " << vital << std::endl;
          std::this_thread::sleep_for(
              std::chrono::microseconds(3000));  // 3 msec
          exit(1);
        }

        if (hit) nakade_tmp.insert(space);
      }
    }
  }

  if (nakade_tmp.size() != kCoordTable.nakade_map.size()) {
    std::cout << "nakade size: " << kCoordTable.nakade_map.size()
              << " checked size: " << nakade_tmp.size() << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
    exit(1);
  }

  if (kCoordTable.bent4_set.size() != 12) {
    std::cout << "bent4 size: " << kCoordTable.bent4_set.size() << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(3000000));  // 3 msec
    exit(1);
  }

  int inv = kEBSize - 2;

  Vertex bend4s[12][3] = {
      {
          xy2v(1, 1),
          xy2v(1, 1) + kVtR,
          xy2v(1, 1) + kVtU,
      },
      {
          xy2v(1, 1),
          xy2v(1, 1) + kVtR,
          xy2v(1, 1) + kVtRR,
      },
      {
          xy2v(1, 1),
          xy2v(1, 1) + kVtU,
          xy2v(1, 1) + kVtUU,
      },
      {
          xy2v(inv, 1),
          xy2v(inv, 1) + kVtL,
          xy2v(inv, 1) + kVtU,
      },
      {
          xy2v(inv, 1),
          xy2v(inv, 1) + kVtL,
          xy2v(inv, 1) + kVtLL,
      },
      {
          xy2v(inv, 1),
          xy2v(inv, 1) + kVtU,
          xy2v(inv, 1) + kVtUU,
      },
      {
          xy2v(1, inv),
          xy2v(1, inv) + kVtR,
          xy2v(1, inv) + kVtD,
      },
      {
          xy2v(1, inv),
          xy2v(1, inv) + kVtR,
          xy2v(1, inv) + kVtRR,
      },
      {
          xy2v(1, inv),
          xy2v(1, inv) + kVtD,
          xy2v(1, inv) + kVtDD,
      },
      {
          xy2v(inv, inv),
          xy2v(inv, inv) + kVtL,
          xy2v(inv, inv) + kVtD,
      },
      {
          xy2v(inv, inv),
          xy2v(inv, inv) + kVtL,
          xy2v(inv, inv) + kVtLL,
      },
      {
          xy2v(inv, inv),
          xy2v(inv, inv) + kVtD,
          xy2v(inv, inv) + kVtDD,
      },
  };

  for (int i = 0; i < 12; ++i) {
    uint64_t space = 0;
    for (int j = 0; j < 3; ++j) AddEmptyHash(&space, bend4s[i][j]);
    if (!IsBentFourHash(space)) {
      std::cout << "bent4 i=" << i << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
      exit(1);
    }
  }
}

/**
 * Checks to see if Sichuan is an accurate judge.
 */
void CheckLadder() {
  Board b;
  int num_shown_boards = 0;
  for (int i = 0; i < 10000; ++i) {
    if (num_shown_boards >= 25) break;
    b.Init();

    for (int j = 0; j < kMaxPly; ++j) {
      b.MakeMove<kOneWay>(b.SelectMove());
      Board b_cpy = b;
      constexpr int num_escapes = kBSize == 9 ? 3 : 4;
      auto esc_list = b_cpy.LadderEscapes(num_escapes);
      IdentifyBoards(b, b_cpy);
      if (!esc_list.empty() && dist_edge(esc_list[0]) != 1) {
        std::cout << b << esc_list[0] << std::endl << std::endl;
        ++num_shown_boards;

        break;
      }

      if (b.double_pass()) break;
    }
  }
}

/**
 * Checks if there are any illegal stones or empty points in the end phase.
 */
Vertex CheckFinishedBoard(const Board& b) {
  if (b.game_ply() >= kMaxPly) return kVtNull;

  for (Vertex v = kVtZero; v < kNumVts; ++v) {
    if (in_wall(v)) continue;

    if (b.color_at(v) == kEmpty) {
      if (b.count_neighbors(v, kEmpty) > 0 && !b.IsSeki(v))
        return v;  // Invalid
      if (!b.IsSeki(v) && !(b.IsEyeShape(kBlack, v) || b.IsEyeShape(kWhite, v)))
        return v;
    } else if (b.color_at(v) < kNumPlayers) {
      Color c = b.color_at(v);
      Vertex v_eye = kVtNull;
      int num_eyes = 0;
      Vertex v_tmp = v;
      do {
        for (const auto& dv : dv4) {
          Vertex v_nbr = v_tmp + dv;
          if (b.color_at(v_nbr) == kEmpty) {
            if (b.IsSeki(v_nbr)) {
              num_eyes = 2;
              break;
            }

            if (b.IsEyeShape(c, v_nbr) && v_nbr != v_eye) {
              v_eye = v_nbr;
              ++num_eyes;
            }
          }
        }

        v_tmp = b.next_v(v_tmp);
      } while (v_tmp != v);

      if (num_eyes < 2) return v;
    } else {
      return v;  // kWall
    }
  }

  return kVtNull;
}

/**
 * Displays the score calculation for the end of the game.
 */
void PrintFinalResult() {
  Board b;
  for (int i = 0; i < 5; ++i) {
    b.Init();
    for (int j = 0; j < kMaxPly; ++j) {
      b.MakeMove<kOneWay>(b.SelectMove());
      if (b.double_pass()) break;
    }
    if (b.game_ply() >= kMaxPly) continue;

    std::vector<std::ostream*> os_list = {&std::cout};
    double komi = Options["komi"].get_double();
    b.FinalResult(komi, os_list);
    std::cout << "b.score()=" << b.Score(komi) << std::endl << std::endl;
  }
}

/**
 * Test structure and transitions of Board class.
 */
void TestBoard() {
  std::cout << "*** Test board ***" << std::endl;
  // Pattern probability
  PrintPatternProb();
  std::cout << "pattern: [OK]\n";

  // Nakade
  CheckNakade();
  std::cout << "nakade: [OK]\n";

  // kRollout
  Board b;
  for (int i = 0; i < 10000; ++i) {
    b.Init();

    Vertex next_move;
    Vertex prev_move = kVtNull;

    for (int j = 0; j < kMaxPly; ++j) {
      next_move = b.SelectMove();
      b.MakeMove<kRollout>(next_move);

      if (next_move == kPass && prev_move == kPass) break;

      prev_move = next_move;
    }

    if (!b.finished()) {
      prev_move = kVtNull;
      for (int j = 0; j < kMaxPly; ++j) {
        next_move = b.SelectMoveRandom();
        b.MakeMove<kRollout>(next_move);

        if (next_move == kPass && prev_move == kPass) break;

        prev_move = next_move;
      }
    }

    Vertex v;
    if ((v = CheckFinishedBoard(b)) != kVtNull) {
      std::cout << "kRollout #" << i << std::endl;
      std::cout << b << std::endl;
      std::cout << v << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
      exit(1);
    }

    if (b.game_ply() >= kMaxPly) {
      std::cout << "kMaxPly\n";
      std::cout << b << std::endl;
      if (b.SelectMoveAny(b.side_to_move()) != kPass)
        std::cout << "repetition type="
                  << (b.CheckRepetition(b.SelectMoveRandom()) == kRepetitionNone
                          ? "NONE"
                          : "DRAW")
                  << std::endl;
    }
  }
  std::cout << "kRollout: [OK]\n";

  // kOneWay rollout
  for (int i = 0; i < 10000; ++i) {
    b.Init();

    Vertex next_move;
    Vertex prev_move = kVtNull;

    for (int j = 0; j < kMaxPly; ++j) {
      next_move = b.SelectMove();
      b.MakeMove<kOneWay>(next_move);

      if (next_move == kPass && prev_move == kPass) break;

      prev_move = next_move;
    }

    if (!b.finished()) {
      prev_move = kVtNull;
      for (int j = 0; j < kMaxPly; ++j) {
        next_move = b.SelectMoveRandom();
        b.MakeMove<kOneWay>(next_move);

        if (next_move == kPass && prev_move == kPass) break;

        prev_move = next_move;
      }
    }

    Vertex v;
    if ((v = CheckFinishedBoard(b)) != kVtNull) {
      std::cout << "kOneWay #" << i << std::endl;
      std::cout << b << std::endl;
      std::cout << v << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
      exit(1);
    }

    if (b.game_ply() >= kMaxPly) {
      std::cout << "kMaxPly\n";
      std::cout << b << std::endl;
      if (b.SelectMoveAny(b.side_to_move()) != kPass)
        std::cout << "repetition type="
                  << (b.CheckRepetition(b.SelectMoveRandom()) == kRepetitionNone
                          ? "NONE"
                          : "DRAW")
                  << std::endl;
    }
  }
  std::cout << "kOneWay: [OK]\n";

  // kQuick rollout
  Board b_cpy;
  for (int i = 0; i < 10000; ++i) {
    b.Init();
    int cpy_idx = static_cast<int>(kNumRvts) * RandDouble();

    Vertex next_move;
    Vertex prev_move = kVtNull;

    for (int j = 0; j < kMaxPly; ++j) {
      if (j == cpy_idx) b_cpy = b;

      next_move = b.SelectMoveRandom();
      b.MakeMove<kQuick>(next_move);
      if (next_move == kPass && prev_move == kPass) break;

      prev_move = next_move;
    }

    if (!b.finished()) {
      prev_move = kVtNull;
      for (int j = 0; j < kMaxPly; ++j) {
        next_move = b.SelectMoveRandom();
        b.MakeMove<kQuick>(next_move);

        if (next_move == kPass && prev_move == kPass) break;

        prev_move = next_move;
      }
    }

    Vertex v;
    if ((v = CheckFinishedBoard(b)) != kVtNull) {
      std::cout << "kQuick #" << i << std::endl;
      std::cout << b << std::endl;
      std::cout << v << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
      exit(1);
    }

    if (cpy_idx >= b.game_ply()) continue;

    int j_max = b.game_ply() - cpy_idx;

    for (int j = 0; j < j_max; ++j) {
      b.UnmakeMove<kQuick>();
    }

    IdentifyBoards(b_cpy, b);
  }
  std::cout << "kQuick: [OK]\n";

  // kReversible rollout
  for (int i = 0; i < 10000; ++i) {
    b.Init();
    int cpy_idx = static_cast<int>(kNumRvts) * RandDouble();

    Vertex prev_move = kVtNull;
    Vertex next_move;

    for (int j = 0; j < kMaxPly; ++j) {
      if (j == cpy_idx) b_cpy = b;

      next_move = b.SelectMove();
      b.MakeMove<kReversible>(next_move);

      if (prev_move == kPass && next_move == kPass) break;

      prev_move = next_move;
    }

    if (!b.finished()) {
      prev_move = kVtNull;
      for (int j = 0; j < kMaxPly; ++j) {
        next_move = b.SelectMoveRandom();
        b.MakeMove<kReversible>(next_move);

        if (next_move == kPass && prev_move == kPass) break;

        prev_move = next_move;
      }
    }

    Vertex v;
    if ((v = CheckFinishedBoard(b)) != kVtNull) {
      std::cout << "kReversible #" << i << std::endl;
      std::cout << b << std::endl;
      std::cout << v << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(3000));  // 3 msec
      exit(1);
    }

    if (cpy_idx >= b.game_ply()) continue;

    int j_max = b.game_ply() - cpy_idx;

    for (int j = 0; j < j_max; ++j) b.UnmakeMove<kReversible>();

    IdentifyBoards(b_cpy, b);
  }
  std::cout << "kReversible: [OK]\n";

  // Score
  PrintFinalResult();
  std::cout << "score: [OK]\n";

  // Ladder
  CheckLadder();
  std::cout << "ladder: [OK]\n";
}

/**
 * Displays the probability distribution of the board.
 */
void PrintProb(const Board& b, const ValueAndProb& vp) {
  std::vector<std::pair<float, int>> prob_order;
  for (int i = 0; i < kNumRvts; ++i) prob_order.push_back({vp.prob[i], i});
  std::sort(prob_order.begin(), prob_order.end(),
            std::greater<std::pair<float, int>>());

  std::printf("move count=%d  value=%.1f[%%]\n", b.game_ply() + 1,
              (vp.value + 1) / 2 * 100);
  for (int i = 0; i < 5; ++i) {
    std::string v_str =
        prob_order[i].second == kNumRvts
            ? "PASS"
            : "ABCDEFGHJKLMNOPQRST"[x_of((RawVertex)prob_order[i].second)] +
                  std::to_string(y_of((RawVertex)prob_order[i].second) + 1);
    std::printf("#%d %-4s %.1f[%%]\n", i + 1, v_str.c_str(),
                prob_order[i].first * 100);
  }

  std::cout << "  ";
  for (int x = 0; x < kBSize; ++x)
    std::cout << " "
              << "ABCDEFGHJKLMNOPQRST"[x] << " ";
  std::cout << std::endl;

  for (int y = 0; y < kBSize; ++y) {
    if (kBSize - y < 10) std::cout << " ";
    std::cout << kBSize - y;

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

      if (b.move_before() == v) {
        std::cout << (~b.side_to_move() == kWhite ? "[O]" : "[X]");
      } else if (b.color_at(v) == kWhite) {
        std::cout << " O ";
      } else if (b.color_at(v) == kBlack) {
        std::cout << " X ";
      } else {
        bool top10 = false;
        for (int i = 0; i < 10; ++i) {
          if (v2rv(v) == (RawVertex)(prob_order[i].second) &&
              prob_order[i].first > 0.02) {
            std::printf("%2d ", static_cast<int>(prob_order[i].first * 99));
            top10 = true;
          }
        }
        if (!top10) {
          if (is_star)
            std::cout << " + ";  // star
          else
            std::cout << " . ";
        }
      }
    }

    if (kBSize - y < 10) std::cout << " ";
    std::cout << kBSize - y << std::endl;
  }

  std::cout << "  ";
  for (int x = 0; x < kBSize; ++x)
    std::cout << " "
              << "ABCDEFGHJKLMNOPQRST"[x] << " ";
  std::cout << std::endl;
}

/**
 * Check if the board with symmetric operation is registered in EvalCache.
 */
void TestSymmetry() {
  Board b;
  std::vector<Vertex> moves;
  moves.push_back(xy2v(16, 16));
  moves.push_back(xy2v(16, 4));
  moves.push_back(xy2v(3, 16));

  for (int i = 0; i < 8; ++i) {
    b.Init();
    for (int j = 0, jmax = moves.size(); j < jmax; ++j) {
      Vertex v = moves[j];
      int rv = v2rv(v);
      Vertex v_sym = rv2v((RawVertex)rv2sym(rv, i));
      b.MakeMove<kOneWay>(v_sym);
    }
    std::cout << b << std::endl;

    for (int j = 0; j < 8; ++j) {
      std::cout << "hash " << j << " = " << b.key(j) << std::endl;
    }
  }

  b.Init();
  for (int j = 0, jmax = moves.size(); j < jmax; ++j) {
    Vertex v = moves[j];
    b.MakeMove<kOneWay>(v);
  }

  ValueAndProb vp;
  Feature ft = b.get_feature();
  TensorEngine engine(0, 1);
  engine.Init();
  EvalCache eval_cache;

  for (int j = 0; j < 8; ++j) {
    engine.Infer(ft, &vp, j);
    PrintProb(b, vp);
    if (j == 0) eval_cache.Insert(b.key(), vp);
  }

  for (int i = 0; i < 8; ++i) {
    b.Init();
    for (int j = 0, jmax = moves.size(); j < jmax; ++j) {
      Vertex v = moves[j];
      int rv = v2rv(v);
      Vertex v_sym = rv2v((RawVertex)rv2sym(rv, i));
      b.MakeMove<kOneWay>(v_sym);
    }

    std::cout << "symmetry_idx=" << i << std::endl;

    bool found = eval_cache.Probe(b, &vp);
    if (!found) {
      std::cout << "cache not found" << std::endl;
    } else {
      PrintProb(b, vp);
    }
  }
}

/**
 * Plays a self match with the hand with the maximum probability of Policy head.
 */
void PolicySelf() {
  Board b;
  ValueAndProb vp;
  TensorEngine engine(0, 8);
  engine.Init();

  for (int j = 0; j < 1; ++j) {
    b.Init();

    for (int i = 0; i < kMaxPly; ++i) {
      engine.Infer(b.get_feature(), &vp);
      PrintProb(b, vp);

      std::cerr << "[Press Enter]" << std::endl;
      getchar();

      std::vector<std::pair<float, int>> prob_order;
      for (int i = 0; i < kNumRvts; ++i) prob_order.push_back({vp.prob[i], i});
      std::sort(prob_order.begin(), prob_order.end(),
                std::greater<std::pair<float, int>>());

      bool has_sensible = false;

      if (b.SelectMoveAny(b.side_to_move()) != kVtNull) {
        for (auto& po : prob_order) {
          Vertex v = po.second == kNumRvts ? kPass : rv2v((RawVertex)po.second);
          if (v != kPass && b.IsSensible(v)) {
            b.MakeMove<kOneWay>(v);
            has_sensible = true;
            break;
          }
        }

        if (!has_sensible) b.MakeMove<kOneWay>(kPass);
      } else {
        b.MakeMove<kOneWay>(kPass);
      }

      if (b.double_pass()) break;
    }
  }

  b.FinalResult(Options["komi"], std::vector<std::ostream*>({&std::cout}));
}

/**
 * Plays a self match.
 */
void SelfMatch() {
  Board b;
  SearchTree tree;
  tree.SetGPUAndMemory();

  double winning_rate = 0.5;
  int j_max = Options["num_games"].get_int();

  for (int j = 0; j < j_max; ++j) {
    b.Init();
    tree.InitRoot();

    for (int i = 0; i < kMaxPly; ++i) {
      Vertex v;
      if (j_max > 1) std::cerr << "game index: " << j + 1 << " ";
      v = tree.Search(b, 0.0, &winning_rate, true, false);
      b.MakeMove<kOneWay>(v);
      tree.PrintBoardLog(b);
      std::cerr << std::endl;

      if (b.double_pass()) break;
    }

    std::array<std::array<double, kNumVts>, kNumPlayers> owner = {0};
    double s = tree.FinalScore(b, kVtNull, -1, 1024, &owner);
    b.PrintOwnerMap(s, 1024, owner, std::vector<std::ostream*>({&std::cerr}));
  }
}

/**
 * Benchmark the inference speed of a neural network.
 */
void NetworkBench() {
  Board b;
  std::vector<Vertex> moves;
  moves.push_back(xy2v(16, 16));
  moves.push_back(xy2v(16, 4));
  moves.push_back(xy2v(3, 16));

  for (int j = 0, jmax = moves.size(); j < jmax; ++j) {
    Vertex v = moves[j];
    b.MakeMove<kOneWay>(v);
  }

  int batch_size = Options["batch_size"].get_int();

  TensorEngine engine(0, batch_size);
  engine.Init();

  std::vector<std::shared_ptr<SyncedEntry>> entries;
  int num_entries = batch_size;
  for (int i = 0; i < num_entries; ++i) {
    entries.emplace_back(std::make_shared<SyncedEntry>(b.get_feature()));
  }

  auto start = std::chrono::system_clock::now();
  const int max_evaluation = 20000;
  int num_evaluation = 0;

  for (int j = 0; j < max_evaluation && num_evaluation < max_evaluation; ++j) {
    for (int i = 0; i < num_entries; ++i) {
      entries[i]->ft = b.get_feature();
    }
    engine.Infer(&entries);
    num_evaluation += num_entries;
  }

  const auto end = std::chrono::system_clock::now();
  double elapsed_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count() /
      1000.0;

  std::cout << "evaluates per seconds = " << num_evaluation / elapsed_time
            << " [eps]" << std::endl;
}

/**
 * Measure the execution speed of the rollout.
 */
void BenchMark() {
  Board b;
  Vertex prev_move = kVtNull;
  int num_games = 50000;
  int ply = 0;

  std::cout << "*** Benchmark ***" << std::endl;
  const auto start = std::chrono::system_clock::now();

  for (int j = 0; j < num_games; ++j) {
    b.Init();

    for (int i = 0; i < kMaxPly; ++i) {
      Vertex next_move = b.SelectMoveRandom();

      b.MakeMove<kRollout>(next_move);
      ++ply;

      if (prev_move == kPass && next_move == kPass) break;

      prev_move = next_move;
    }
  }

  const auto end = std::chrono::system_clock::now();
  double elapsed_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count() /
      1000.0;
  std::cout << "rollouts per seconds = " << num_games / elapsed_time << " [pps]"
            << std::endl;
  std::cout << "moves per seconds = " << ply / elapsed_time << " [mps]"
            << std::endl;
}

/**
 * Measures the speed at which a tree node is freed from memory.
 */
void TestFreeMemory() {
  Board b;
  SearchTree tree;
  const int max_node_size = 500000;
  tree.Resize(max_node_size);

  tree.ShiftRootNode(kVtNull, b);

  std::function<void(Node*, Board&, int)> create_all_nodes =
      [&](Node* nd, Board& b_, int depth) {
        if (depth >= 3 || tree.num_entries() > max_node_size) return;
        int i_max = nd->num_children();
        for (int i = 0; i < i_max - 1; ++i) {
          Board b_cpy = b_;
          b_cpy.MakeMove<kOneWay>((Vertex)nd->children[i].move());
          std::unique_ptr<Node> pnd(new Node(b_cpy));
          nd->children[i].set_next_ptr(&pnd);
          nd->increment_entries();
          tree.increment_entries();
          create_all_nodes(nd->children[i].next_ptr(), b_cpy, depth + 1);
          if (tree.num_entries() > max_node_size) break;
        }
      };

  const auto t0 = std::chrono::system_clock::now();

  Board b_;
  create_all_nodes(tree.root_node(), b_, 0);
  int num_entries = tree.num_entries();
  std::cout << "num_entries = " << num_entries << std::endl;

  const auto t1 = std::chrono::system_clock::now();

  double elapsed_time = double(
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
  elapsed_time /= 1000;
  std::cout << "created node per seconds = " << num_entries / elapsed_time
            << " nps" << std::endl;

  std::cout << "[Press Enter] for deleting all nodes." << std::endl;
  getchar();

  Vertex v = xy2v(4, 4);
  b.MakeMove<kOneWay>(v);
  tree.ShiftRootNode(v, b);
  std::cout << "num_entries = " << tree.num_entries() << std::endl;

  const auto t2 = std::chrono::system_clock::now();
  elapsed_time = double(
      std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
  elapsed_time /= 1000;
  std::cout << "deleted node per seconds = " << num_entries / elapsed_time
            << " nps" << std::endl;
}

/**
 * Read the SGF file and test the score of the last board.
 */
void ReadSgfFinalScore(int argc, char** argv) {
  std::string file_name = argv[1];
  int ply = argc > 2 ? std::stoi(std::string(argv[2])) : -1;

  SgfData sgf_data;
  sgf_data.Read(file_name);
  int max_ply = sgf_data.game_ply();
  if (ply > 0) max_ply = std::min(max_ply, ply);

  Board b;
  for (int i = 0; i < max_ply; ++i) b.MakeMove<kOneWay>(sgf_data.move_at(i));

  std::cout << b << std::endl;
  std::vector<std::ostream*> os_list;
  os_list.push_back(&std::cout);
  b.FinalResult(Options["komi"], os_list);
}

/**
 * Random rollouts are performed to display the seki.
 */
void TestSeki() {
  auto v2str = [](Vertex v) {
    return v == kPass
               ? "PASS"
               : v > kPass ? "NULL"
                           : std::string("ABCDEFGHJKLMNOPQRST")[x_of(v) - 1] +
                                 std::to_string(y_of(v));
  };

  Board b;
  Vertex prev_move = kVtNull;
  bool go_next = false;

  for (int i = 0; i < 100000; ++i) {
    b.Init();
    prev_move = kVtNull;
    go_next = false;

    for (int j = 0; j < kMaxPly; ++j) {
      Vertex v = b.SelectMove();
      b.MakeMove<kOneWay>(v);

      if ((v == kPass && prev_move == kPass) || go_next) break;
    }

    for (Vertex v_k : b.empties()) {
      if (b.IsSeki(v_k)) {
        std::cout << b << std::endl;
        std::cout << v2str(v_k) << std::endl;
        getchar();
      }
    }
  }
}

/**
 * Test to see if you can pass properly under Japanese rules.
 */
void TestPassMove() {
  std::string sgf_dir = Options["sgf_dir"];

  std::ifstream ifs;
  std::string str;

  // open sgf list.
  //
  // 1413887-model_167_30-vs-model_167_10.sgf 251
  // 1413827-model_167_30-vs-model_167_20.sgf 323
  // 1413740-model_167_30-vs-model_167_10.sgf 277
  // ...
  ifs.open(JoinPath(sgf_dir, "sgf_list.txt"));
  if (ifs.fail())
    std::cerr << "file could not be opened: sgf_list.txt" << std::endl;

  TensorEngine engine(0, 1);
  engine.Init();
  SearchTree tree;
  std::vector<std::ostream*> os_list;
  os_list.push_back(&std::cout);

  while (getline(ifs, str)) {
    std::string line_str;
    std::istringstream iss(str);

    getline(iss, line_str, ' ');
    std::string file_name = JoinPath(sgf_dir, line_str);
    SgfData sgf_data;
    sgf_data.Read(file_name);

    int ply = sgf_data.game_ply();
    std::string wrong_type = "ws";

    Board b;

    auto show_scores = [](const Board& b, bool add_pass) {
      const int num_games = 1024;
      std::array<std::array<double, kNumVts>, kNumPlayers> owner = {0};
      Vertex nv = add_pass ? kPass : kVtNull;
      bool add_null_pass = !add_pass;

      auto scores =
          b.RolloutScores(num_games, nv, -1, true, add_null_pass, &owner);

      double total_black_wins = 0.0;
      std::vector<std::pair<int, double>> desc_scores;
      for (auto& score_and_games : scores) {
        double s = score_and_games.first;
        int games = score_and_games.second;
        if (s > 0)
          total_black_wins += games;
        else if (s == 0)
          total_black_wins += 0.5 * games;
        desc_scores.push_back({score_and_games.second, score_and_games.first});
      }

      double winning_rate = total_black_wins / num_games;
      if (b.side_to_move() == kWhite) winning_rate = 1 - winning_rate;
      std::sort(desc_scores.begin(), desc_scores.end(),
                std::greater<std::pair<int, double>>());

      std::cout << std::fixed << std::setprecision(1);
      if (add_pass)
        std::cout << "[with PASS]\n";
      else
        std::cout << "[without PASS]\n";
      std::cout << "winning_rate=" << winning_rate * 100 << "[%]\n";
      std::cout << "score:\n";

      int i_max = std::min(static_cast<int>(desc_scores.size()), 5);
      for (int i = 0; i < i_max; ++i) {
        double s = b.side_to_move() == kBlack ? desc_scores[i].second
                                              : -desc_scores[i].second;
        double rate = desc_scores[i].first * 100.0 / num_games;
        std::cout << s << "\t" << rate << "[%]\n";
      }
    };

    auto need_pass = [&](const Board& b, Vertex next_move) {
      const int num_playouts = 1024;
      const int num_policy_moves = -1;
      Vertex best_move_32 =
          tree.ShouldPass(b, next_move, 32, num_playouts, &engine);
      Vertex best_move = tree.ShouldPass(b, next_move, num_policy_moves,
                                         num_playouts, &engine);

      if (best_move != best_move_32) {
        std::cout << "ply=" << b.game_ply() << " next_move:" << next_move
                  << " best_move:" << best_move
                  << " best_move(32):" << best_move_32 << std::endl;
      }

      if (next_move != kVtNull && next_move != best_move) {
        std::cout << "ply=" << b.game_ply() << " next_move:" << next_move
                  << " best_move:" << best_move << std::endl;
      }

      return best_move == kPass;
    };

    std::cout << file_name << std::endl;
    std::cout << "type=" << wrong_type << " target ply=" << ply << std::endl;

    bool check_last = true;
    for (int i = 0; i < ply; ++i) {
      Vertex next_move = sgf_data.move_at(i);
      if (i >= 240) {
        if (need_pass(b, next_move)) {
          std::cout << "should pass at ply=" << b.game_ply() << std::endl;
        }
      }

      b.MakeMove<kOneWay>(next_move);
    }

    if (check_last) {
      int max_ply = sgf_data.game_ply();
      Vertex v_last =
          b.game_ply() >= max_ply ? kVtNull : sgf_data.move_at(b.game_ply());
      bool should_pass = need_pass(b, v_last);
      std::cout << "ply=" << b.game_ply()
                << (should_pass ? " should pass" : " should not pass")
                << std::endl;

      if (wrong_type == "sp" && should_pass) continue;
      if (wrong_type == "snp" && !should_pass) continue;
      if (wrong_type == "fe" && !should_pass) continue;

      std::cout << b << std::endl;

      if (wrong_type != "ws") {
        show_scores(b, true);
        show_scores(b, false);
      }

      std::array<std::array<double, kNumVts>, kNumPlayers> owner = {0};
      // double s = tree.final_score(b, kVtNull, -1, 1024, owner);
      double s = tree.FinalScore(b, kVtNull, -1, 1024, &owner, &engine);
      b.PrintOwnerMap(s, 1024, owner, os_list);
      std::cout << "sgf   : " << (sgf_data.winner() == kBlack ? "B+" : "W+");
      std::cout << std::fixed << std::setprecision(1)
                << std::fabs(sgf_data.score()) << std::endl;
      // getchar();
    }
  }
  ifs.close();
}
