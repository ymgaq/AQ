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

#ifndef BOARD_H_
#define BOARD_H_

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "./bitboard.h"
#include "./config.h"
#include "./feature.h"
#include "./pattern.h"
#include "./types.h"

// --------------------
//        Board
// --------------------

/**
 * @enum AdvanceType
 * Advance type of moves on board.
 *   (fast) kRollout > kOneWay > kQuick > kReversible (slow)
 */
enum AdvanceType {
  kOneWay,      // Updates only input features for NN.
  kReversible,  // Keeps all difference infomation.
  kQuick,       // Keeps only infomation related ladder search.
  kRollout,     // For random rollouts.
};

/**
 * Probability for response moves
 */
constexpr double kRespWeight[4][kNumPlayers] = {{13.9441, 0.0717287},
                                                {86.1518, 0.0116074},
                                                {19.3089, 0.0517895},
                                                {1.70591, 0.5861973}};

/**
 * @class Board
 * The Board class consists of four types of board information (color_, ptn_,
 * sg_, next_v_), probability distributions for rollout, and Feature class for
 * neural networks.
 *
 * The move is templated with AdvanceType, kOneWay is one-way and used for usual
 * search. kQuick, which is used to search for Ladder, and kReversible update
 * Diff structure.
 */
class Board {
 public:
  typedef std::array<std::array<double, kNumVts>, kNumPlayers> OwnerMap;

  // Constructor.
  Board() { Init(); }

  Board(const Board& rhs);

  Board& operator=(const Board& rhs);

  /**
   * Initializes board.
   */
  void Init();

  /**
   * Returns a color of the side to move. (i.e. us_)
   */
  Color side_to_move() const { return us_; }

  int game_ply() const { return ply_; }

  Vertex move_before() const { return prev_move_[opp_]; }

  Vertex move_before_2() const { return prev_move_[us_]; }

  Color color_at(Vertex v) const { return color_[v]; }

  std::vector<Vertex> empties() const {
    std::vector<Vertex> vertices;
    for (int i = 0; i < num_empties_; ++i) vertices.push_back(empty_[i]);
    return std::move(vertices);
  }

  int sg_id(Vertex v) const { return sg_id_[v]; }

  bool sg_atari_at(Vertex v) const { return sg_[sg_id_[v]].atari(); }

  int sg_size_at(Vertex v) const { return sg_[sg_id_[v]].size(); }

  Bitboard sg_liberties_at(Vertex v) const {
    return sg_[sg_id_[v]].bb_liberties();
  }

  int sg_num_liberties_at(Vertex v) const {
    return sg_[sg_id_[v]].num_liberties();
  }

  Vertex next_v(Vertex v) const { return next_v_[v]; }

  int num_passes(Color c) const { return num_passes_[c]; }

  std::vector<Vertex> move_history() const {
    std::vector<Vertex> history;
    for (int i = 0; i < ply_; ++i) history.push_back(move_history_[i]);
    return std::move(history);
  }

  int count_neighbors(Vertex v, Color c) const { return ptn_[v].count(c); }

  bool has_atari_neighbor(Vertex v) const { return ptn_[v].atari(); }

  bool enclosed_by(Vertex v, Color c) const { return ptn_[v].enclosed_by(c); }

  bool has_atari_neighbor_at(Vertex v, Direction d) const {
    return ptn_[v].atari_at(d);
  }

  bool has_counter_move() const {
    return response_move_[1] != kVtNull || response_move_[2] != kVtNull;
  }

  void set_game_ply(int val) { ply_ = val; }

  void set_num_passes(Color c, int val) { num_passes_[c] = val; }

  void increment_passes(Color c) { num_passes_[c]++; }

  void decrement_passes(Color c) { num_passes_[c]--; }

  /**
   * Returns whether v is a legal move.
   */
  bool IsLegal(Vertex v) const {
    ASSERT_LV2(is_ok(v));

    if (v == kPass)
      return true;
    else if (v == ko_ || color_[v] != kEmpty)
      return false;

    return ptn_[v].legal(us_);
  }

  /**
   * Returns whether v is legal move when considering color.
   */
  bool IsLegal(Color c, Vertex v) const {
    ASSERT_LV2(is_ok(v));
    ASSERT_LV2(kColorZero <= c && c < kNumPlayers);

    if (v == kPass)
      return true;
    else if (v == ko_ || color_[v] != kEmpty)
      return false;

    return ptn_[v].legal(c);
  }

  bool IsEyeShape(Color c, Vertex v, bool ignore_atari = false) const;

  bool IsEyeShape(Vertex v) const { return IsEyeShape(us_, v, false); }

  /**
   * Returns whether v is a false eye.
   */
  bool IsFalseEye(Vertex v) const {
    if (ptn_[v].count(kEmpty)) return false;
    if (ptn_[v].enclosed_by(kBlack)) return CountWedges(kBlack, v) >= 2;
    if (ptn_[v].enclosed_by(kWhite)) return CountWedges(kWhite, v) >= 2;
    return false;
  }

  /**
   * Returns whether move on position v is self-atari and forms nakade.
   * This method is only used for checking whether hourikomi is effective in
   * seki.
   */
  bool IsSelfAtariNakade(Vertex v, int expected_num_liberties = 2,
                         Color nakade_color = kEmpty) const {
    return IsVitalSelfAtari(v, kBlack, expected_num_liberties, nakade_color) ||
           IsVitalSelfAtari(v, kWhite, expected_num_liberties, nakade_color);
  }

  /**
   * Returns whether v is an empty vertex for Seki.
   */
  bool IsSeki(Vertex v) const;

  /**
   * Updates the board with the move on position v.
   * NOTE: NEED to confirm in advance whether the move is legal.
   */
  template <AdvanceType Type>
  void MakeMove(Vertex v);

  /**
   * Undoes previous move from Diff class.
   */
  template <AdvanceType Type>
  void UnmakeMove();

  /**
   * Returns hash key of current board.
   */
  Key key() const { return hash_key_; }

  /**
   * Returns hash key of current board with symmetric operation.
   */
  Key key(int symmetry_idx) const {
    Key sym_hash_key = 0;

    if (symmetry_idx == 0) {
      sym_hash_key = hash_key_;
    } else {
      for (int rv = 0; rv < kNumRvts; ++rv) {
        Vertex v = rv2v((RawVertex)rv);
        Vertex v_sym = rv2v((RawVertex)rv2sym(rv, symmetry_idx));
        if (color_[v_sym] == kBlack)
          sym_hash_key ^= kCoordTable.zobrist_table[0][kBlack][v];
        else if (color_[v_sym] == kWhite)
          sym_hash_key ^= kCoordTable.zobrist_table[0][kWhite][v];
      }
      if (ko_ != kVtNull) {
        int rv = v2rv(ko_);
        Vertex v_sym = ko_;
        for (int i = 0; i < 8; ++i) {
          int rv_sym = rv2sym(rv, i);
          if (rv2sym(rv_sym, symmetry_idx) == rv) {
            v_sym = rv2v((RawVertex)rv_sym);
            break;
          }
        }
        sym_hash_key ^= kCoordTable.zobrist_table[0][3][v_sym];
      }
      if (us_ == kWhite) sym_hash_key ^= 1;
    }

    return sym_hash_key;
  }

  Feature get_feature() const {
    Feature ft = feature_;
    ft.Update(*this);
    return std::move(ft);
  }

  /**
   * Returns whether v is a sensible move.
   */
  bool IsSensible(Vertex v) const {
    return IsLegal(v) && !IsEyeShape(v) && !IsSeki(v);
  }

  /**
   * Returns whether v is a sensible move.
   */
  bool IsSensible(Color c, Vertex v) const {
    return IsLegal(c, v) && !IsEyeShape(c, v) && !IsSeki(v);
  }

  /**
   * Returns whether v is a move causing self-atari.
   */
  bool IsSelfAtariWithoutNakade(Color c, Vertex v) const {
    return IsSelfAtari(c, v) && !IsSelfAtariNakade(v);
  }

  /**
   * Returns whether there is no legal move.
   */
  bool finished() const {
    return SelectMoveAny(us_) == kPass && SelectMoveAny(opp_) == kPass;
  }

  /**
   * Returns whether pass lasted twice.
   */
  bool double_pass() const {
    return prev_move_[kBlack] == kPass && prev_move_[kWhite] == kPass;
  }

  /**
   * Returns the final score at komi.
   *
   *    Black wins: score > 0
   *    White wins: score < 0
   *    Draw      : score == 0
   */
  double Score(double komi, OwnerMap* owner = nullptr) const;

  /**
   * Returns player color of the game winner.
   */
  Color Winner(double komi) const {
    double s = Score(komi);
    return s == 0 ? kEmpty : s > 0 ? kBlack : kWhite;
  }

  /**
   * Returns {Vertex, liberties} of stones in Atari. (for Japanese rule)
   */
  std::vector<std::pair<Vertex, std::vector<int>>> GetAtariInfo() const;

  /**
   * Returns vertices and colors that need to be filled. (for Japanese rule)
   */
  std::vector<std::pair<Vertex, Color>> NeedToBeFilled(
      int num_playouts, const OwnerMap& owner) const;

  /**
   * Returns score distribution with rollout. (for Japanese rule)
   */
  std::unordered_map<double, int> RolloutScores(
      int num_playouts, Vertex next_move, double thr_rate, bool add_moves,
      bool add_null_pass, OwnerMap* owner = nullptr) const;

  /**
   * Outputs the final result.
   */
  void PrintOwnerMap(double s, int num_playouts, const OwnerMap& owner,
                     std::vector<std::ostream*> os_list) const;

  /**
   * Returns the final score.
   */
  std::string FinalResult(double komi,
                          std::vector<std::ostream*> os_list) const;

  /**
   * Returns repetition state of v.
   */
  RepetitionState CheckRepetition(Vertex v) const;

  /**
   * Returns vertices that runs away from Ladder.
   */
  std::vector<Vertex> LadderEscapes(int num_escapes);

  /**
   * Returns any legal move.
   */
  Vertex SelectMoveAny(Color c) const {
    int i = 0;
    Vertex next_move = kPass;

    for (;;) {
      next_move = empty_[i];
      if (IsSensible(c, next_move)) break;
      if (++i == num_empties_) return kPass;
    }

    return next_move;
  }

  /**
   * Returns a legal move selected randomly.
   */
  Vertex SelectMoveRandom() const {
    int i0 = RandDouble() * num_empties_;  // [0, num_empties_)
    int i = i0;
    Vertex next_move = kPass;

    for (;;) {
      next_move = empty_[i];

      // Stops if next_move is legal and dosen't fill an eye and seki.
      if (IsSensible(next_move)) break;

      if (++i == num_empties_) i = 0;

      // Repeats until all empty vertexes are checked.
      if (i == i0) {
        next_move = kPass;
        break;
      }
    }

    return next_move;
  }

  /**
   * Returns a legal move selected based on probability distribution.
   */
  Vertex SelectMove() const {
    Vertex next_move = kPass;
    double rand_double = RandDouble();  // [0.0, 1.0)
    double prob_rank[kBSize];
    double prob_v[kNumVts];
    int i, j, y;
    double rand_move, tmp_sum;

    // 1. Copys probability.
    std::memcpy(prob_rank, sum_prob_rank_[us_], sizeof(prob_rank));
    std::memcpy(prob_v, prob_[us_], sizeof(prob_v));
    double total_sum = 0.0;
    for (auto rs : prob_rank) total_sum += rs;

    // 2. Selects next move based on probability distribution.
    for (i = 0; i < num_empties_; ++i) {
      rand_move = rand_double * total_sum;
      if (total_sum <= 0) break;
      tmp_sum = 0;

      // a. Finds the rank where sum of prob_rank exceeds rand_move.
      y = -1;
      for (j = 0; j < kBSize; ++j) {
        ++y;
        tmp_sum += prob_rank[y];
        if (tmp_sum > rand_move) break;
      }
      ASSERT_LV2(0 <= y && y < kBSize);
      tmp_sum -= prob_rank[y];

      // b. Finds the position where sum of probability exceeds rand_move.
      next_move = xy2v(0, y + 1);
      for (j = 0; j < kBSize; ++j) {
        ++next_move;
        tmp_sum += prob_v[next_move];
        if (tmp_sum > rand_move) break;
      }
      if (j == kBSize) {
        next_move = kPass;
        continue;
      }

      ASSERT_LV2(kVtZero <= next_move && next_move < kNumVts);
      ASSERT_LV2(!in_wall(next_move));
      ASSERT_LV2(color_[next_move] == kEmpty);

      // c. Stops if next_move is legal and dosen't fill an eye or Seki.
      if (IsSensible(next_move)) break;

      // d. Recalculate after subtracting probability of next_move.
      prob_rank[y] -= prob_v[next_move];
      total_sum -= prob_v[next_move];
      prob_v[next_move] = 0;
      next_move = kPass;
    }

    return next_move;
  }

  /**
   * Returns the winner of the result of a rollout from the current board.
   */
  Color Rollout(double komi) {
    Vertex next_move;
    Vertex prev_move_ = kVtNull;
    if (us_ == kWhite) num_passes_[kWhite]++;

    while (ply_ <= kMaxPly) {
      next_move = SelectMove();
      MakeMove<kRollout>(next_move);

      // Stops when successive passes.
      if (next_move == kPass && prev_move_ == kPass) break;

      prev_move_ = next_move;
    }

    prev_move_ = kVtNull;

    while (ply_ <= kMaxPly) {
      next_move = SelectMoveRandom();
      MakeMove<kRollout>(next_move);

      if (next_move == kPass && prev_move_ == kPass) break;

      prev_move_ = next_move;
    }

    return Winner(komi);
  }

  /**
   * Outputs board infromation. (for debug)
   */
  friend std::ostream& operator<<(std::ostream& os, const Board& b) {
    std::string str_x = "ABCDEFGHJKLMNOPQRST";  // note that excluding "I"
    os << "  ";
    for (int x = 0; x < kBSize; ++x) os << " " << str_x[x] << " ";
    os << std::endl;

    for (int y = 0; y < kBSize; ++y) {
      if (kBSize - y < 10) os << " ";
      os << kBSize - y;

      for (int x = 0; x < kBSize; ++x) {
        Vertex v = xy2v(x + 1, kBSize - y);
        bool is_star = false;

        if (kBSize >= 13 &&
            (x == 3 || x == kBSize / 2 || x == kBSize - 1 - 3) &&
            (y == 3 || y == kBSize / 2 || y == kBSize - 1 - 3))
          is_star = true;
        else if (kBSize == 9 && ((x == 2 || x == kBSize - 1 - 2) &&
                                 (y == 2 || y == kBSize - 1 - 2)) ||
                 (x == kBSize / 2 && y == kBSize / 2))
          is_star = true;

        if (b.prev_move_[b.opp_] == v)
          os << (b.opp_ == kWhite ? "[O]" : "[X]");
        else if (b.color_[v] == kWhite)
          os << " O ";
        else if (b.color_[v] == kBlack)
          os << " X ";
        else if (is_star)
          os << " + ";  // star
        else
          os << " . ";
      }

      if (kBSize - y < 10) os << " ";
      os << kBSize - y << std::endl;
    }

    os << "  ";
    for (int x = 0; x < kBSize; ++x) os << " " << str_x[x] << " ";
    os << std::endl;

    return os;
  }

  friend bool IdentifyBoards(const Board& b1, const Board& b2);

 private:
  // Turn indeces. (kWhite or kBlack)
  Color us_, opp_;

  // Vertex colors.
  // (kWhite, kBlack, kEmpty or kWall)
  Color color_[kNumVts];

  // List of empty vertexes, containing their positions
  // in range of [0, num_empties_-1].
  //
  //  for(int i=0;i<num_empties_;++i)
  //    v = empty[i];
  //    ...
  Vertex empty_[kNumRvts];

  // Empty index of each position.
  // If empty_idx[v] < num_empties_, v is kEmpty.
  int empty_id_[kNumVts];

  // The number of stones.
  int num_stones_[kNumPlayers];

  // The number of empty vertexes.
  int num_empties_;

  // The position of the illegal move of Ko.
  Vertex ko_;

  // Stone-group index.
  int sg_id_[kNumVts];

  // Stone groups.
  // NOTE: This member is called as sg_[sg_id_[v]].
  StoneGroup sg_[kNumVts];

  // Next vertex of another stone in the save stone group.
  Vertex next_v_[kNumVts];

  // Total move count of the game.
  int ply_;

  // Number of moves of kPass. (for Japanese rule)
  int num_passes_[kNumPlayers];

  // History of moves.
  Vertex move_history_[kMaxPly];

  // Previous moves of both colors.
  Vertex prev_move_[kNumPlayers];

  // Previous vertex of illegal move of Ko.
  Vertex prev_ko_;

  // List of stones removed in the current move.
  Bitboard removed_stones_;

  // Basic (3x3) patterns.
  Pattern ptn_[kNumVts];

  // Twelve-point patterns around last and two moves before moves.
  Pattern prev_ptn_[2];

  // Response move, such as nakade or save stones in atari.
  Vertex response_move_[4];

  // Probability of each vertex.
  double prob_[kNumPlayers][kNumVts];

  // Sum of probability of each rank.
  double sum_prob_rank_[kNumPlayers][kBSize];

  // Probability of previous response pattern.
  double prev_rsp_prob_;

  // Hash key of current board.
  Key hash_key_;

  // History of hash keys.
  Key key_history_[8];

  // History of difference.
  std::vector<Diff> diffs_;

  Feature feature_;

  // Flag indicating whether pattern has been updated.
  bool updated_[kNumVts];

  // List of (position, previous value of stones) of updated patterns.
  std::vector<std::pair<Vertex, uint32_t>> updated_ptns_;

  /**
   * Returns count of wedges made of opponent's stone.
   */
  int CountWedges(Color c, Vertex v, bool ignore_atari = false) const {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] == kEmpty);
    ASSERT_LV2(kColorZero <= c && c < kNumPlayers);

    int num_wedges = dist_edge(v) == 1 ? 1 : 0;
    for (const auto& d : diag4)
      if (ptn_[v].color_at(d) == ~c) {
        int nbr_id = sg_id_[v + dir2v(d)];
        if (ignore_atari || !sg_[nbr_id].atari() ||
            sg_[nbr_id].liberty_atari() == ko_)
          num_wedges++;
      }

    return num_wedges;
  }

  /**
   * Returns whether v is an eye shape.
   */
  bool IsEyeShapeImpl(Color c, Vertex v, bool ignore_atari = false) const {
    if (!ptn_[v].enclosed_by(c)) return false;
    return CountWedges(c, v, ignore_atari) < 2;
  }

  /**
   * Returns whether v is an empty point of bend-four.
   */
  bool IsLibertyOfBent4(Vertex v) const {
    if (color_[v] != kEmpty) return false;
    if (dist_edge(v) != 1) return false;

    int x = x_of(v);
    int y = y_of(v);

    if (x == 1 || x == kBSize) {
      if (y == 2 || y == 3)
        y--;
      else if (y == kBSize - 1 || y == kBSize - 2)
        y++;
      else
        return false;
    } else {  // y == 1 || y == kBSize
      if (x == 2 || x == 3)
        x--;
      else if (x == kBSize - 1 || x == kBSize - 2)
        x++;
      else
        return false;
    }

    Vertex v_nbr = xy2v(x, y);

    if (!is_stone(color_[v_nbr])) return false;
    if (sg_[sg_id_[v_nbr]].size() != 3) return false;

    uint64_t stone_hash = 0;
    Vertex v_tmp = v_nbr;
    do {
      AddEmptyHash(&stone_hash, v_tmp);
      v_tmp = next_v_[v_tmp];
    } while (v_tmp != v_nbr);

    return IsBentFourHash(stone_hash);
  }

  /**
   * Returns whether v is a false eye where is sensible to fill.
   */
  bool IsSensibleFalseEye(Vertex v, const std::vector<Vertex>& libs) const {
    bool may_nakade5 = false;
    // Returns false if it is gomoku-nakade.
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (is_stone(color_[v_nbr]) && sg_[sg_id_[v_nbr]].size() != 1)
        may_nakade5 = true;
    }

    if (!may_nakade5) return false;

    Color opp_c = ptn_[v].count(kBlack) > 0 ? kWhite : kBlack;
    bool opp_nakade = false;

    // Checks whether the liberties are in pre-Atari.
    for (auto lib : libs) {
      if (lib == v) continue;

      for (const auto& d : dir4)
        if (ptn_[lib].is_stone(d) && !ptn_[lib].pre_atari_at(d)) return false;

      // It is sensible if opponet can hourikomi in liberties.
      if (IsSelfAtariNakade(lib, 3, opp_c)) opp_nakade = true;
    }

    return opp_nakade;
  }

  /**
   * Returns counts of eye shape on the stone group including v.
   */
  void CountEyes(Vertex v, int* num_liberties, int* num_eyes,
                 std::vector<bool>* checked_liberties,
                 std::vector<bool>* checked_sg_id) const;

  void UpdateResponseMove(const std::vector<int>& atari_ids);

  /**
   * Adds v to the list of updated patterns.
   */
  template <AdvanceType Type>
  void AddUpdatePattern(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);

    if (!updated_[v]) {
      updated_[v] = true;
      if (Type == kQuick || Type == kReversible)
        diffs_.back().ptn.Insert(v, ptn_[v].stones());

      if (Type != kQuick) updated_ptns_.push_back({v, ptn_[v].stones()});
    }
  }

  /**
   * Sets atari to the stone group including v.
   */
  template <AdvanceType Type>
  void set_atari(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] < kNumPlayers);

    Vertex v_atr = sg_[sg_id_[v]].liberty_atari();

    AddUpdatePattern<Type>(v_atr);
    ptn_[v_atr].set_atari(
        sg_id_[v_atr + kVtU] == sg_id_[v], sg_id_[v_atr + kVtR] == sg_id_[v],
        sg_id_[v_atr + kVtD] == sg_id_[v], sg_id_[v_atr + kVtL] == sg_id_[v]);
  }

  /**
   * Sets pre-atari to the stone group including v.
   */
  template <AdvanceType Type>
  void set_pre_atari(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] < kNumPlayers);

    for (auto v_patr : sg_[sg_id_[v]].lib_vertices()) {
      AddUpdatePattern<Type>(v_patr);
      ptn_[v_patr].set_pre_atari(sg_id_[v_patr + kVtU] == sg_id_[v],
                                 sg_id_[v_patr + kVtR] == sg_id_[v],
                                 sg_id_[v_patr + kVtD] == sg_id_[v],
                                 sg_id_[v_patr + kVtL] == sg_id_[v]);
    }
  }

  /**
   * Cancels atari in the stone group including v.
   */
  template <AdvanceType Type>
  void cancel_atari(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] < kNumPlayers);

    Vertex v_atr = sg_[sg_id_[v]].liberty_atari();
    AddUpdatePattern<Type>(v_atr);

    ptn_[v_atr].cancel_atari(
        sg_id_[v_atr + kVtU] == sg_id_[v], sg_id_[v_atr + kVtR] == sg_id_[v],
        sg_id_[v_atr + kVtD] == sg_id_[v], sg_id_[v_atr + kVtL] == sg_id_[v]);
  }

  /**
   * Cancels pre-atari in the stone group including v.
   */
  template <AdvanceType Type>
  void cancel_pre_atari(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] < kNumPlayers);

    for (auto v_patr : sg_[sg_id_[v]].lib_vertices()) {
      AddUpdatePattern<Type>(v_patr);
      ptn_[v_patr].cancel_pre_atari(sg_id_[v_patr + kVtU] == sg_id_[v],
                                    sg_id_[v_patr + kVtR] == sg_id_[v],
                                    sg_id_[v_patr + kVtD] == sg_id_[v],
                                    sg_id_[v_patr + kVtL] == sg_id_[v]);
    }
  }

  /**
   * Places a stone on v.
   */
  template <AdvanceType Type>
  void PlaceStone(Vertex v);

  /**
   * Removes a stone on v.
   */
  template <AdvanceType Type>
  void RemoveStone(Vertex v);

  /**
   * Merges stone groups including v_base and v_add.
   * Replaces index of the stone group including v_add.
   */
  template <AdvanceType Type>
  void Merge(Vertex v_base, Vertex v_add);

  /**
   * Removes a stone group including v.
   */
  template <AdvanceType Type>
  void RemoveStoneGroup(Vertex v);

  /**
   * Returns whether v causes self-Atari and the vital point of
   * Nakade.
   */
  bool IsVitalSelfAtari(Vertex v, Color c, int expected_num_liberties,
                        Color nakade_color) const;

  /**
   * Returns whether move on position v is self-atari.
   */
  bool IsSelfAtari(Color c, Vertex v) const {
    ASSERT_LV2(kVtZero <= v && v < kPass);
    ASSERT_LV2(color_[v] == kEmpty);
    ASSERT_LV2(c < kNumPlayers);

    if (ptn_[v].count(kEmpty) >= 2) return false;

    Bitboard libs;

    // Counts number of liberty of neighboring stone groups.
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;

      if (color_[v_nbr] == kEmpty) {
        libs.Add(v_nbr);
      } else if (color_[v_nbr] == ~c) {
        if (sg_[sg_id_[v_nbr]].atari()) {
          if (sg_[sg_id_[v_nbr]].size() > 1) return false;
          libs.Add(v_nbr);
        }
      } else if (color_[v_nbr] == c) {
        if (sg_[sg_id_[v_nbr]].num_liberties() > 2) return false;
        libs.Merge(sg_[sg_id_[v_nbr]].bb_liberties());
      }
    }

    return libs.num_bits() <= 2;
  }

  /**
   * Registers 12-point pattern around v.
   */
  template <AdvanceType Type>
  void UpdatePrevPtn(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kPass);

    if (Type == kQuick)
      return;
    else if (Type == kReversible)
      diffs_.back().prev_ptn = prev_ptn_[1];

    // 1. Copys the pattern to prev_ptn_[1].
    prev_ptn_[1] = prev_ptn_[0];

    // 2. Add stones in position of the Manhattan distance = 4.
    prev_ptn_[0] = ptn_[v];
    prev_ptn_[0].set_color(kDirUU,
                           v + kVtUU < kNumVts ? color_[v + kVtUU] : kWall);
    prev_ptn_[0].set_color(kDirRR, color_[v + kVtRR]);
    prev_ptn_[0].set_color(kDirDD, v + kVtDD >= 0 ? color_[v + kVtDD] : kWall);
    prev_ptn_[0].set_color(kDirLL, color_[v + kVtLL]);

    // 3. Flip color_ if it is black's turn.
    if (us_ == kBlack) prev_ptn_[0].FlipColor();
  }

  /**
   * Multiplies probability on v.
   */
  template <AdvanceType Type>
  void add_prob(Color c, Vertex v, double add_prob) {
    ASSERT_LV2(kVtZero <= v && v <= kPass);
    ASSERT_LV2(c < kNumPlayers);

    if (Type == kQuick) return;

    if (v >= kPass || prob_[c][v] == 0) return;

    if (Type == kReversible) {
      diffs_.back().prob[c].Insert(v, prob_[c][v]);
      diffs_.back().sum_prob_rank[c].Insert(y_of(v) - 1,
                                            sum_prob_rank_[c][y_of(v) - 1]);
    }

    sum_prob_rank_[c][y_of(v) - 1] += (add_prob - 1) * prob_[c][v];
    prob_[c][v] *= add_prob;
  }

  /**
   * Updates probability on all vertexes where 3x3 patterns were changed.
   */
  template <AdvanceType Type>
  void UpdateProbs() {
    if (Type == kQuick) return;

    // 1. Updates probability on empty vertexes where 3x3 pattern was changed.
    for (auto& up : updated_ptns_) {
      for (const Color& c : {kBlack, kWhite}) {
        Vertex v = up.first;
        double ptn_prob = ptn_[v].prob(c, false);

        if (prob_[c][v] == 0) {
          replace_prob<Type>(c, v, ptn_prob);
        } else {
          Pattern ptn_tmp(up.second);
          add_prob<Type>(c, v, ptn_tmp.prob(c, true) * ptn_prob);
        }
      }
    }

    // 2. Updates probability on positions where a stone was removed
    //    probability of prob_dist_base has been already added in
    //    RemoveStone().
    for (auto rs : removed_stones_.Vertices()) {
      add_prob<Type>(us_, rs, ptn_[rs].prob(us_, false));
      add_prob<Type>(opp_, rs, ptn_[rs].prob(opp_, false));
    }
  }

  // Replaces the probability on position v with new_prob.
  template <AdvanceType Type>
  void replace_prob(Color c, Vertex v, double new_prob) {
    ASSERT_LV2(kColorZero <= c && c < kNumPlayers);
    ASSERT_LV2(0 <= v && v < kNumVts);
    ASSERT_LV2(0 <= new_prob);

    if (Type == kQuick) {
      return;
    } else if (Type == kReversible) {
      diffs_.back().prob[c].Insert(v, prob_[c][v]);
      diffs_.back().sum_prob_rank[c].Insert(y_of(v) - 1,
                                            sum_prob_rank_[c][y_of(v) - 1]);
    }
    sum_prob_rank_[c][y_of(v) - 1] += new_prob - prob_[c][v];
    prob_[c][v] = new_prob;
  }
};

inline Board::Board(const Board& rhs)
    : us_(rhs.us_),
      opp_(rhs.opp_),
      num_empties_(rhs.num_empties_),
      ko_(rhs.ko_),
      ply_(rhs.ply_),
      prev_ko_(rhs.prev_ko_),
      removed_stones_(rhs.removed_stones_),
      prev_rsp_prob_(rhs.prev_rsp_prob_),
      hash_key_(rhs.hash_key_),
      feature_(rhs.feature_) {
  std::memcpy(color_, rhs.color_, sizeof(color_));
  std::memcpy(empty_, rhs.empty_, sizeof(empty_));
  std::memcpy(empty_id_, rhs.empty_id_, sizeof(empty_id_));
  std::memcpy(num_stones_, rhs.num_stones_, sizeof(num_stones_));
  std::memcpy(sg_, rhs.sg_, sizeof(sg_));
  std::memcpy(next_v_, rhs.next_v_, sizeof(next_v_));
  std::memcpy(sg_id_, rhs.sg_id_, sizeof(sg_id_));
  std::memcpy(move_history_, rhs.move_history_, sizeof(move_history_));
  std::memcpy(prev_move_, rhs.prev_move_, sizeof(prev_move_));
  std::memcpy(prob_, rhs.prob_, sizeof(prob_));
  std::memcpy(ptn_, rhs.ptn_, sizeof(ptn_));
  prev_ptn_[0].set_stones(rhs.prev_ptn_[0].stones());
  prev_ptn_[1].set_stones(rhs.prev_ptn_[1].stones());
  std::memcpy(response_move_, rhs.response_move_, sizeof(response_move_));
  std::memcpy(updated_, rhs.updated_, sizeof(updated_));
  updated_ptns_ = rhs.updated_ptns_;
  std::memcpy(sum_prob_rank_, rhs.sum_prob_rank_, sizeof(sum_prob_rank_));
  std::memcpy(num_passes_, rhs.num_passes_, sizeof(num_passes_));
  std::memcpy(key_history_, rhs.key_history_, sizeof(key_history_));
  // diffs_.clear();
}

inline Board& Board::operator=(const Board& rhs) {
  us_ = rhs.us_;
  opp_ = rhs.opp_;
  std::memcpy(color_, rhs.color_, sizeof(color_));
  std::memcpy(empty_, rhs.empty_, sizeof(empty_));
  std::memcpy(empty_id_, rhs.empty_id_, sizeof(empty_id_));
  std::memcpy(num_stones_, rhs.num_stones_, sizeof(num_stones_));
  num_empties_ = rhs.num_empties_;
  ko_ = rhs.ko_;
  std::memcpy(sg_, rhs.sg_, sizeof(sg_));
  std::memcpy(next_v_, rhs.next_v_, sizeof(next_v_));
  std::memcpy(sg_id_, rhs.sg_id_, sizeof(sg_id_));
  ply_ = rhs.ply_;
  std::memcpy(move_history_, rhs.move_history_, sizeof(move_history_));
  std::memcpy(prev_move_, rhs.prev_move_, sizeof(prev_move_));
  prev_ko_ = rhs.prev_ko_;
  removed_stones_ = rhs.removed_stones_;
  std::memcpy(prob_, rhs.prob_, sizeof(prob_));
  std::memcpy(ptn_, rhs.ptn_, sizeof(ptn_));
  prev_ptn_[0].set_stones(rhs.prev_ptn_[0].stones());
  prev_ptn_[1].set_stones(rhs.prev_ptn_[1].stones());
  prev_rsp_prob_ = rhs.prev_rsp_prob_;
  std::memcpy(response_move_, rhs.response_move_, sizeof(response_move_));
  std::memcpy(updated_, rhs.updated_, sizeof(updated_));
  updated_ptns_ = rhs.updated_ptns_;
  std::memcpy(sum_prob_rank_, rhs.sum_prob_rank_, sizeof(sum_prob_rank_));
  std::memcpy(num_passes_, rhs.num_passes_, sizeof(num_passes_));
  hash_key_ = rhs.hash_key_;
  std::memcpy(key_history_, rhs.key_history_, sizeof(key_history_));
  diffs_.clear();  // Resets diffs_
  feature_ = rhs.feature_;

  return *this;
}

inline void Board::Init() {
  us_ = kBlack;
  opp_ = kWhite;
  num_empties_ = 0;
  std::memset(sum_prob_rank_, 0, sizeof(sum_prob_rank_));
  std::memset(updated_, 0, sizeof(updated_));

  Vertex v = kVtZero;
  for (int i = 0, i_max = kNumVts; i < i_max; ++i) {
    v = (Vertex)i;
    next_v_[v] = v;
    sg_id_[v] = v;
    sg_[v].SetNull();

    // In wall.
    if (in_wall(v)) {
      color_[v] = kWall;
      empty_id_[v] = kVtNull;
      ptn_[v].SetNull();
      prob_[kBlack][v] = prob_[kWhite][v] = 0;
    } else {  // real board
      color_[v] = kEmpty;
      empty_id_[v] = num_empties_;
      empty_[num_empties_] = v;
      ++num_empties_;
    }
  }

  for (int i = 0, i_max = kNumVts; i < i_max; ++i) {
    v = (Vertex)i;
    if (in_wall(v)) continue;

    ptn_[v].SetEmpty();
    for (const auto& d : dir8) ptn_[v].set_color(d, color_[v + dir2v(d)]);

    // Probability.
    prob_[kWhite][v] = ptn_[v].prob(kWhite, false);
    prob_[kBlack][v] = ptn_[v].prob(kBlack, false);

    // Sum of prob_[c][v] in the Nth rank on the real board.
    sum_prob_rank_[kWhite][y_of(v) - 1] += prob_[kWhite][v];
    sum_prob_rank_[kBlack][y_of(v) - 1] += prob_[kBlack][v];
  }

  num_stones_[kWhite] = num_stones_[kBlack] = 0;
  num_empties_ = kNumRvts;
  ply_ = 0;
  num_passes_[kWhite] = num_passes_[kBlack] = 0;

  for (auto& mh : move_history_) mh = kVtNull;
  removed_stones_.Init();
  ko_ = kVtNull;
  prev_move_[kWhite] = prev_move_[kBlack] = kVtNull;
  prev_ko_ = kVtNull;
  for (auto& i : response_move_) i = kVtNull;

  updated_ptns_.clear();
  prev_ptn_[kWhite].SetNull();
  prev_ptn_[kBlack].SetNull();

  prev_rsp_prob_ = 0.0;
  hash_key_ = 0;
  for (auto& k : key_history_) k = UINT64_MAX;

  diffs_.clear();
  feature_.Init();
}

inline bool Board::IsEyeShape(Color c, Vertex v, bool ignore_atari) const {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] == kEmpty);
  ASSERT_LV2(kColorZero <= c && c < kNumPlayers);

  if (!IsEyeShapeImpl(c, v, ignore_atari)) return false;
  if (dist_edge(v) != 1) return true;  // Eye shape.

  // Checks whether filling eye of gomoku-nakade.
  bool checked[kNumVts] = {0};
  int num_checks = 0;
  Bitboard libs;
  int stone_size = 0;

  for (const auto& d : dir4) {
    if (!ptn_[v].is_stone(d)) continue;

    int nbr_id = sg_id_[v + dir2v(d)];
    if (!checked[nbr_id]) {
      checked[nbr_id] = true;
      num_checks++;
      stone_size += sg_[nbr_id].size();
      libs.Merge(sg_[nbr_id].bb_liberties());

      if (libs.num_bits() > 2 || stone_size > 4)
        return true;  // Normal eye shape.
    }
  }

  if (libs.num_bits() != 2 || stone_size != 4) return true;

  bool is_conner = (x_of(v) == 1 || x_of(v) == kBSize) &&
                   (y_of(v) == 1 || y_of(v) == kBSize);
  if (is_conner && num_checks > 1) return true;  // Not gomoku-nakade.

  // Check the stones can be connected to another stone.
  auto vs = libs.Vertices();
  Vertex lib = vs[0] != v ? vs[0] : vs[1];  // vs.size == 2

  int num_opp_stones = 0;
  for (const auto& dv : dv4) {
    int nbr_id = sg_id_[lib + dv];
    Color nbr_c = color_[lib + dv];

    if (nbr_c == c && !checked[nbr_id])
      return true;  // Eye shape.
    else if (nbr_c == ~c && !sg_[nbr_id].atari())
      ++num_opp_stones;
  }

  if (IsSeki(lib)) return true;          // Seki
  if (num_opp_stones > 0) return false;  // Gomoku-nakade

  return true;  // Eye shape
}

inline void Board::CountEyes(Vertex v, int* num_liberties, int* num_eyes,
                             std::vector<bool>* checked_liberties,
                             std::vector<bool>* checked_sg_id) const {
  (*checked_liberties)[v] = true;

  if (!IsFalseEye(v)) (*num_liberties)++;
  if (*num_liberties >= 5) return;

  if (IsEyeShapeImpl(kBlack, v) || IsEyeShapeImpl(kWhite, v)) (*num_eyes)++;

  // Adds num_eyes if opponet's stone can be captured.
  if (ptn_[v].atari()) {
    Color atr_c = kEmpty;
    Color patr_c = kEmpty;
    int patr_id = -1;

    for (const auto& d : dir4) {
      if (!ptn_[v].is_stone(d)) continue;
      Color c = ptn_[v].color_at(d);

      if (ptn_[v].atari_at(d) && c != atr_c) {
        atr_c = atr_c == kEmpty ? c : kWall;
      } else if (ptn_[v].pre_atari_at(d)) {
        if (patr_c == kEmpty)
          patr_c = c, patr_id = sg_id_[v + dir2v(d)];
        else if (patr_c != c)
          c = kWall;
        else if (patr_id != sg_id_[v + dir2v(d)])
          c = kWall;
      }
    }

    if (is_stone(atr_c) && ~atr_c == patr_c) (*num_eyes)++;
  }

  // Recursive search.
  for (const auto& dv : dv4) {
    Vertex v_nbr = v + dv;

    if (color_[v_nbr] == kEmpty) {
      if (!(*checked_liberties)[v_nbr])
        CountEyes(v_nbr, num_liberties, num_eyes, checked_liberties,
                  checked_sg_id);
    } else if (is_stone(color_[v_nbr])) {
      int nbr_id = sg_id_[v_nbr];
      if ((*checked_sg_id)[nbr_id]) continue;

      (*checked_sg_id)[nbr_id] = true;
      Bitboard libs_nbr = sg_[nbr_id].bb_liberties();

      if (libs_nbr.num_bits() > 2) {
        *num_liberties += 5;
        return;
      }

      for (auto nv : libs_nbr.Vertices()) {
        if (nv == v) continue;
        if (!(*checked_liberties)[nv])
          CountEyes(nv, num_liberties, num_eyes, checked_liberties,
                    checked_sg_id);
      }
    }
  }
}

inline bool Board::IsSeki(Vertex v) const {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] == kEmpty);

  // 1. Not seki when empty vertexes are more than 2 or
  //    both stones are not in neighboring positions.
  if (!ptn_[v].pre_atari()) return false;
  if (ptn_[v].count(kEmpty) >= 2) return false;
  if ((ptn_[v].count(kWhite) == 0 || ptn_[v].count(kBlack) == 0) &&
      !IsFalseEye(v))
    return false;

  // 2. Gathers liberties.
  Bitboard libs;
  for (const auto& d : dir4) {
    Vertex v_nbr = v + dir2v(d);  // A neighboring position.
    Color c = color_[v_nbr];

    if (c == kWall) {
      continue;
    } else if (c == kEmpty) {
      libs.Add(v_nbr);
      continue;
    }

    // 3. Not seki when the liberty number is not 2 or the size if 1.
    if (!ptn_[v].pre_atari_at(d)) {
      return false;
    } else if (sg_[sg_id_[v_nbr]].size() == 1 && ptn_[v].count(c) == 1) {
      for (const auto& dv : dv4) {
        Vertex v_nbr2 = v_nbr + dv;
        if (v_nbr2 != v && color_[v_nbr2] == kEmpty &&
            ptn_[v_nbr2].count(c) == 1)
          return false;
      }
    }

    libs.Merge(sg_[sg_id_[v_nbr]].bb_liberties());
  }

  if (libs.num_bits() == 2) {
    // 4. Checks whether it is 'bent-four'.
    if (IsLibertyOfBent4(v)) return false;

    // 5. Checks whether it is self-atari of Nakade.
    for (auto lib : libs.Vertices())
      if (IsSelfAtariNakade(lib)) return false;

    return true;
  } else if (libs.num_bits() == 3) {
    // 6. Check whether it is possible to fill a false eye.
    if (IsFalseEye(v)) return IsSensibleFalseEye(v, libs.Vertices());

    // 7. Counts total liberties and eyes.
    int num_liberties = 0;
    int num_eyes = 0;
    std::vector<bool> checked_liberties(kNumVts, false);
    std::vector<bool> checked_sg_id(kNumVts, false);

    CountEyes(v, &num_liberties, &num_eyes, &checked_liberties, &checked_sg_id);

    return num_liberties == 2 || (num_liberties <= 4 && num_eyes >= 2);
  }

  return false;
}

inline void Board::UpdateResponseMove(const std::vector<int>& atari_ids) {
  std::unordered_set<int> checked;

  for (auto aid : atari_ids) {
    if (checked.count(aid)) continue;
    checked.insert(aid);

    Vertex v_atr = sg_[aid].liberty_atari();
    Vertex v_tmp = (Vertex)aid;
    int max_num_stones = 0;

    // b. Checks whether it is possible to take neighboring stones
    //    of the stone group in atari.
    do {
      for (const auto& dv : dv4) {
        Vertex v_nbr = v_tmp + dv;
        StoneGroup& sg_nbr = sg_[sg_id_[v_nbr]];

        if (color_[v_nbr] != us_) continue;
        if (!sg_nbr.atari() || sg_nbr.liberty_atari() == ko_) continue;
        // if(sg_nbr.size != 1) continue;

        int num_captured = 0;
        Vertex lib = sg_nbr.liberty_atari();
        for (const auto& d : dir4)
          if (ptn_[lib].atari_at(d) && ptn_[lib].color_at(d) == us_)
            num_captured++;

        if (num_captured <= 1 && lib == sg_[aid].liberty_atari())
          continue;  // inner caputure

        if (sg_nbr.size() > max_num_stones) {
          response_move_[1] = lib;
          max_num_stones = sg_nbr.size();
        }
      }

      v_tmp = next_v_[v_tmp];
    } while (v_tmp != aid);

    // c. Checks whether it is possible to save stones by escape.
    if (response_move_[1] == kVtNull &&
        (ptn_[v_atr].count(kEmpty) >= 2 || !IsSelfAtari(opp_, v_atr)))
      response_move_[2] = v_atr;
  }
}

template <AdvanceType Type>
inline void Board::MakeMove(Vertex v) {
  ASSERT_LV2(kVtZero <= v && v <= kPass);
  ASSERT_LV2(v == kPass || color_[v] == kEmpty);
  ASSERT_LV2(v != ko_);

  bool use_diff = (Type == kQuick || Type == kReversible);
  bool reversible = (Type == kReversible);
  bool use_prob = (Type != kQuick);
  bool use_feature = (Type == kOneWay || Type == kReversible);

  if (use_diff) {
    diffs_.emplace_back(Diff());
    diffs_.back().removed_stones = removed_stones_;
    diffs_.back().prev_ko = prev_ko_;
  }

  // 1. Updates history.
  int prev_num_empties = num_empties_;
  bool is_in_eye = ptn_[v].enclosed_by(opp_);
  prev_ko_ = ko_;
  ko_ = kVtNull;
  move_history_[ply_++] = v;
  removed_stones_.Init();

  // 2. Restores probability of distance and 12-point pattern
  //    of the previous move.
  if (use_prob) {
    if (prev_move_[opp_] < kPass) {
      for (const auto& dv : dv8)
        add_prob<Type>(us_, prev_move_[opp_] + dv, kRespWeight[0][1]);

      // Restores the probability of 12 surroundings.
      if (prev_rsp_prob_ != 0) {
        for (const auto& dv : dv12) {
          Vertex v_nbr = prev_move_[opp_] + dv;
          if (v_nbr < kVtZero)
            continue;
          else if (v_nbr >= kNumVts)
            continue;
          add_prob<Type>(us_, v_nbr, prev_rsp_prob_);
        }
      }
    }

    // 3. Restores probability of response_move_.
    if (reversible) {
      for (int i = 0; i < 4; ++i)
        diffs_.back().response_move[i] = response_move_[i];
    }

    response_move_[0] = kVtNull;
    for (int i = 1; i < 4; ++i) {
      if (response_move_[i] != kVtNull) {
        add_prob<Type>(us_, response_move_[i], kRespWeight[i][1]);
        response_move_[i] = kVtNull;
      }
    }

    if (reversible) {
      diffs_.back().features_add = feature_.last_add();
      diffs_.back().features_sub = feature_.last_sub();
    }

    if (use_feature) feature_.DoNullMove();
  }

  // Updates if v is pass.
  if (v == kPass) {
    if (Type != kRollout) ++num_passes_[us_];
    prev_move_[us_] = v;

    if (use_prob) {
      if (reversible) {
        diffs_.back().prev_ptn = prev_ptn_[1];
        diffs_.back().key = key_history_[7];
        diffs_.back().prev_rsp_prob = prev_rsp_prob_;
      }
      prev_ptn_[1] = prev_ptn_[0];
      prev_ptn_[0].SetNull();
      prev_rsp_prob_ = 0;

      for (int i = 7; i > 0; --i) key_history_[i] = key_history_[i - 1];
      key_history_[0] = hash_key_;
    }

    // Exchange turn.
    us_ = ~us_;
    opp_ = ~opp_;

    hash_key_ ^= 1;
    if (prev_ko_ != kVtNull)
      hash_key_ ^= kCoordTable.zobrist_table[0][3][prev_ko_];

    return;
  }

  // 4. Initializes updated flag.
  std::memset(updated_, 0, sizeof(updated_));

  if (use_prob) {
    // 5. Initializes the updating flag of 3x3 pattern
    //    and update response pattern.
    updated_ptns_.clear();
    UpdatePrevPtn<Type>(v);
  }

  // 6. Places stone at v.
  PlaceStone<Type>(v);

  // 7. Merges the stone with other stone groups.
  for (const auto& dv : dv4) {
    Vertex v_nbr = v + dv;
    // a. When v_nbr is our stone color_ and another stone group.
    if (color_[v_nbr] != us_ || sg_id_[v_nbr] == sg_id_[v]) continue;

    // b. Cancels pre-atari when it becomes in atari.
    if (sg_[sg_id_[v_nbr]].atari()) cancel_pre_atari<Type>(v_nbr);

    // c. Merges them with the larger size of stone group as the base.
    if (sg_[sg_id_[v]].size() > sg_[sg_id_[v_nbr]].size())
      Merge<Type>(v, v_nbr);
    else
      Merge<Type>(v_nbr, v);
  }

  std::vector<int> atari_ids;
  // 8. Reduces liberty of opponent's stones.
  for (const auto& dv : dv4) {
    Vertex v_nbr = v + dv;
    // If an opponent stone.
    if (color_[v_nbr] != opp_) continue;

    int num_liberties = sg_[sg_id_[v_nbr]].num_liberties();

    if (num_liberties == 2) {
      set_pre_atari<Type>(v_nbr);
    } else if (num_liberties == 1) {
      set_atari<Type>(v_nbr);
      atari_ids.push_back(sg_id_[v_nbr]);
    } else if (num_liberties == 0) {
      RemoveStoneGroup<Type>(v_nbr);
    }
  }

  // 9. Updates ko
  if (is_in_eye && prev_num_empties == num_empties_)
    ko_ = empty_[num_empties_ - 1];

  // 10. Updates atari/pre-atari of the stone group including v.
  if (sg_[sg_id_[v]].num_liberties() == 2) {
    set_pre_atari<Type>(v);
  } else if (sg_[sg_id_[v]].num_liberties() == 1) {
    set_atari<Type>(v);
    if (use_prob && sg_[sg_id_[v]].liberty_atari() != ko_)
      response_move_[3] = sg_[sg_id_[v]].liberty_atari();
  }

  if (use_prob) {
    // 11. Updates response_move_ which saves stones in atari.
    if (!atari_ids.empty()) UpdateResponseMove(atari_ids);

    // 12. Updates probability on all vertexes where 3x3 patterns were changed.
    UpdateProbs<Type>();

    // 13. Updates probability of the response pattern.
    if (reversible) diffs_.back().prev_rsp_prob = prev_rsp_prob_;

    double rsp_prob = 0;
    double inv_prob = 0;
    prev_ptn_[0].ResponseProb(&rsp_prob, &inv_prob);
    if (rsp_prob != -1) {
      prev_rsp_prob_ = inv_prob;
      for (const auto& dv : dv12) {
        Vertex v_nbr = v + dv;
        if (v_nbr < kVtZero) continue;
        if (v_nbr >= kNumVts) continue;

        add_prob<Type>(opp_, v_nbr, rsp_prob);
      }
    } else {
      prev_rsp_prob_ = 0;
    }

    // 14. Updates probability based on distance and response moves.
    for (const auto& dv : dv8) add_prob<Type>(opp_, v + dv, kRespWeight[0][0]);

    if (response_move_[0] != kVtNull) {
      add_prob<Type>(opp_, response_move_[0], 1000);
      add_prob<Type>(us_, response_move_[0], 1000);
    }

    for (int i = 1; i < 4; ++i)
      if (response_move_[i] != kVtNull)
        add_prob<Type>(opp_, response_move_[i], kRespWeight[i][0]);

    // 15. Updates hash history.
    if (reversible) diffs_.back().key = key_history_[7];

    for (int i = 7; i > 0; --i) key_history_[i] = key_history_[i - 1];
    key_history_[0] = hash_key_;
  }

  // 16. Updates current hash key.
  hash_key_ ^= kCoordTable.zobrist_table[0][us_][v];
  hash_key_ ^= 1;

  for (auto& rs : removed_stones_.Vertices())
    hash_key_ ^= kCoordTable.zobrist_table[0][opp_][rs];

  if (prev_ko_ != kVtNull)
    hash_key_ ^= kCoordTable.zobrist_table[0][3][prev_ko_];
  if (ko_ != kVtNull) hash_key_ ^= kCoordTable.zobrist_table[0][3][ko_];

  // 17. Flips turn.
  prev_move_[us_] = v;
  us_ = ~us_;
  opp_ = ~opp_;
}

template <AdvanceType Type>
inline void Board::UnmakeMove() {
  ASSERT_LV2(diffs_.size() > 0);

  auto& diff = diffs_.back();
  Vertex v = move_history_[ply_ - 1];
  move_history_[ply_ - 1] = kVtNull;
  --ply_;

  if (v == kPass) {
    --num_passes_[opp_];
  } else {
    // Recovers stones.
    color_[v] = kEmpty;
    --num_stones_[opp_];
    ++num_empties_;

    for (auto rs : removed_stones_.Vertices()) {
      color_[rs] = us_;
      ++num_stones_[us_];
      --num_empties_;
    }

    // Recovers board struct.
    for (auto& sp : diff.empty.entry) empty_[sp.first] = sp.second;
    for (auto& sp : diff.empty_id.entry) empty_id_[sp.first] = sp.second;
    for (auto& sp : diff.next_v.entry) next_v_[sp.first] = sp.second;
    for (auto& sp : diff.ptn.entry) ptn_[sp.first].set_stones(sp.second);
    for (auto& sp : diff.sg.entry) sg_[sp.first] = sp.second;
    for (auto& sp : diff.sg_id.entry) sg_id_[sp.first] = sp.second;
  }

  if (Type == kReversible) {
    for (const Color& c : {kBlack, kWhite}) {
      for (auto& sp : diff.prob[c].entry) prob_[c][sp.first] = sp.second;
      for (auto& sp : diff.sum_prob_rank[c].entry)
        sum_prob_rank_[c][sp.first] = sp.second;
    }
    prev_ptn_[0] = prev_ptn_[1];
    prev_ptn_[1] = diff.prev_ptn;
    prev_rsp_prob_ = diff.prev_rsp_prob;

    for (int i = 0; i < 7; ++i) key_history_[i] = key_history_[i + 1];
    key_history_[7] = diff.key;

    for (int i = 0; i < 4; ++i) response_move_[i] = diff.response_move[i];

    feature_.Undo(diff.features_add, diff.features_sub);
  }

  // Updates hash key.
  if (v != kPass) hash_key_ ^= kCoordTable.zobrist_table[0][opp_][v];
  hash_key_ ^= 1;

  for (auto rs : removed_stones_.Vertices())
    hash_key_ ^= kCoordTable.zobrist_table[0][us_][rs];

  if (prev_ko_ != kVtNull)
    hash_key_ ^= kCoordTable.zobrist_table[0][3][prev_ko_];
  if (ko_ != kVtNull) hash_key_ ^= kCoordTable.zobrist_table[0][3][ko_];

  // Recovers Ko.
  ko_ = prev_ko_;
  prev_ko_ = diff.prev_ko;

  removed_stones_ = diff.removed_stones;

  // Exchange turn.
  prev_move_[opp_] = ply_ > 1 ? move_history_[ply_ - 2] : kVtNull;
  us_ = ~us_;
  opp_ = ~opp_;

  diffs_.pop_back();
}

/**
 * Places a stone on v.
 */
template <AdvanceType Type>
inline void Board::PlaceStone(Vertex v) {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] == kEmpty);

  // 1. Updates 3x3 patterns around v.
  color_[v] = us_;
  for (const auto& d : dir8) {
    Vertex v_nbr = v + dir2v(d);

    if (color_[v_nbr] == kEmpty) AddUpdatePattern<Type>(v_nbr);

    if (Type == kReversible || Type == kQuick)
      diffs_.back().ptn.Insert(v_nbr, ptn_[v_nbr].stones());
    ptn_[v_nbr].set_color(~d, us_);
  }

  // 2. Updates stone number and probability at v.
  ++num_stones_[us_];
  --num_empties_;

  if (Type == kQuick || Type == kReversible) {
    diffs_.back().empty_id.Insert(empty_[num_empties_],
                                  empty_id_[empty_[num_empties_]]);
    diffs_.back().empty.Insert(empty_id_[v], empty_[empty_id_[v]]);
    diffs_.back().sg_id.Insert(v, sg_id_[v]);
    diffs_.back().sg.Insert(v, sg_[v]);
  }

  if (Type == kOneWay || Type == kReversible) feature_.Add(us_, v);

  empty_id_[empty_[num_empties_]] = empty_id_[v];
  empty_[empty_id_[v]] = empty_[num_empties_];
  replace_prob<Type>(us_, v, 0.0);
  replace_prob<Type>(opp_, v, 0.0);

  // 3. Updates sg_id_ including v.
  sg_id_[v] = v;
  sg_[v].Init();

  // 4. Updates liberty on neighboring positions.
  for (const auto& dv : dv4) {
    Vertex v_nbr = v + dv;

    if (color_[v_nbr] == kEmpty) {  // Adds liberty when v_nbr is empty.
      sg_[v].Add(v_nbr);
    } else {  // Delete liberty.
      if (Type == kReversible || Type == kQuick)
        diffs_.back().sg.Insert(sg_id_[v_nbr], sg_[sg_id_[v_nbr]]);
      sg_[sg_id_[v_nbr]].Remove(v);
    }
  }
}

template <AdvanceType Type>
inline void Board::RemoveStone(Vertex v) {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] < kNumPlayers);

  // 1. Updates 3x3 patterns around v.
  color_[v] = kEmpty;
  updated_[v] = true;

  if (Type == kQuick || Type == kReversible)
    diffs_.back().ptn.Insert(v, ptn_[v].stones());
  ptn_[v].clear_atari();
  ptn_[v].clear_pre_atari();

  for (const auto& d : dir8) {
    Vertex v_nbr = v + dir2v(d);
    if (color_[v_nbr] == kEmpty) AddUpdatePattern<Type>(v_nbr);

    if (Type == kQuick || Type == kReversible)
      diffs_.back().ptn.Insert(v_nbr, ptn_[v_nbr].stones());
    ptn_[v_nbr].set_color(~d, kEmpty);
  }

  // 2. Updates stone number and probability at v.
  --num_stones_[opp_];

  if (Type == kQuick || Type == kReversible) {
    diffs_.back().empty_id.Insert(v, empty_id_[v]);
    diffs_.back().empty.Insert(num_empties_, empty_[num_empties_]);
    diffs_.back().sg_id.Insert(v, sg_id_[v]);
    // diffs_.back().removed_stones.Insert(v);
  }

  if (Type == kOneWay || Type == kReversible) feature_.Remove(opp_, v);

  empty_id_[v] = num_empties_;
  empty_[num_empties_] = v;
  ++num_empties_;
  replace_prob<Type>(us_, v, 1.0);
  replace_prob<Type>(opp_, v, 1.0);
  sg_id_[v] = v;
  removed_stones_.Add(v);
}

template <AdvanceType Type>
inline void Board::Merge(Vertex v_base, Vertex v_add) {
  ASSERT_LV2(kVtZero <= v_base && v_base < kPass);
  ASSERT_LV2(color_[v_base] < kNumPlayers);
  ASSERT_LV2(kVtZero <= v_add && v_add < kPass);
  ASSERT_LV2(color_[v_add] < kNumPlayers);

  // 1. Merges stone group class.
  if (Type == kQuick || Type == kReversible)
    diffs_.back().sg.Insert(sg_id_[v_base], sg_[sg_id_[v_base]]);

  sg_[sg_id_[v_base]].Merge(sg_[sg_id_[v_add]]);

  // 2. Replaces sg_id_ of the stone group including v_add.
  int v_tmp = v_add;
  do {
    if (Type == kQuick || Type == kReversible)
      diffs_.back().sg_id.Insert(v_tmp, sg_id_[v_tmp]);

    sg_id_[v_tmp] = sg_id_[v_base];
    v_tmp = next_v_[v_tmp];
  } while (v_tmp != v_add);

  // 3. Swap positions of next_v_.
  //
  //    (before)
  //    v_base: 0->1->2->3->0
  //    v_add : 4->5->6->4
  //    (after)
  //    v_base: 0->5->6->4->1->2->3->0
  if (Type == kQuick || Type == kReversible) {
    diffs_.back().next_v.Insert(v_base, next_v_[v_base]);
    diffs_.back().next_v.Insert(v_add, next_v_[v_add]);
  }

  std::swap(next_v_[v_base], next_v_[v_add]);
}

template <AdvanceType Type>
inline void Board::RemoveStoneGroup(Vertex v) {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] == opp_);

  // 1. Removes all stones of the stone group.
  std::vector<Vertex> spaces;

  Vertex v_tmp = v;
  do {
    RemoveStone<Type>(v_tmp);
    spaces.push_back(v_tmp);
    v_tmp = next_v_[v_tmp];
  } while (v_tmp != v);

  // 2. Checks whether the space after removing stones is a nakade shape.
  if (Type != kQuick) {
    if (spaces.size() >= 3 && spaces.size() <= 6) {
      uint64_t space_hash = 0;
      for (auto v_space : spaces)
        // a. Calculates Zobrist hash relative to the center position.
        AddEmptyHash(&space_hash, v_space);

      if (IsNakadeHash(space_hash)) {
        if (kCoordTable.nakade_map.at(space_hash) < kNumVts)
          // b. Registers the vital of nakade.
          response_move_[0] = kCoordTable.nakade_map.at(space_hash);
      }
    }
  }

  // 3. Updates liberty of neighbor stone.
  std::vector<int> patr_ids;

  do {
    for (const auto& dv : dv4) {
      Vertex v_nbr = v_tmp + dv;
      if (is_stone(color_[v_nbr])) {
        // Cancels atari or pre-atari because liberty positions are added.
        // Final status of atari or pre-atari will be calculated in
        // MakeMove().
        if (sg_[sg_id_[v_nbr]].atari()) {
          cancel_atari<Type>(v_nbr);
          patr_ids.push_back(sg_id_[v_nbr]);
        } else if (sg_[sg_id_[v_nbr]].pre_atari()) {
          cancel_pre_atari<Type>(v_nbr);
        }
      }

      if (Type == kQuick || Type == kReversible)
        diffs_.back().sg.Insert(sg_id_[v_nbr], sg_[sg_id_[v_nbr]]);

      sg_[sg_id_[v_nbr]].Add(v_tmp);
    }

    if (Type == kQuick || Type == kReversible)
      diffs_.back().next_v.Insert(v_tmp, next_v_[v_tmp]);

    Vertex v_next = next_v_[v_tmp];
    next_v_[v_tmp] = v_tmp;
    v_tmp = v_next;
  } while (v_tmp != v);

  // 4. Updates liberty of neighboring stone groups.

  // 5. Removes duplicated indexes.
  sort(patr_ids.begin(), patr_ids.end());
  patr_ids.erase(unique(patr_ids.begin(), patr_ids.end()), patr_ids.end());

  for (auto pi : patr_ids)
    // Updates petterns when liberty number is 2.
    if (sg_[pi].pre_atari()) set_pre_atari<Type>((Vertex)pi);
}

inline bool Board::IsVitalSelfAtari(Vertex v, Color c,
                                    int expected_num_liberties,
                                    Color nakade_color) const {
  ASSERT_LV2(kVtZero <= v && v < kPass);
  ASSERT_LV2(color_[v] == kEmpty);

  std::vector<bool> checked(kNumVts, false);
  uint64_t space_hash = 0;
  bool may_nakade = true;
  Bitboard libs;

  for (const auto& dv : dv4) {
    Vertex v_nbr = v + dv;

    if (color_[v_nbr] != c) continue;
    if (nakade_color != kEmpty && nakade_color != c) continue;
    if (checked[sg_id_[v_nbr]]) continue;

    if (sg_[sg_id_[v_nbr]].size() >= 5) return false;
    if (!sg_[sg_id_[v_nbr]].pre_atari()) return false;

    checked[sg_id_[v_nbr]] = true;

    if (nakade_color != kEmpty) {
      for (auto lib : sg_[sg_id_[v_nbr]].lib_vertices()) {
        for (const auto& d : dir4) {
          Vertex v_nbr2 = lib + dir2v(d);
          if ((ptn_[lib].is_stone(d) && !ptn_[lib].pre_atari_at(d)) ||
              (ptn_[lib].color_at(d) == c && sg_id_[v_nbr] != sg_id_[v_nbr2]))
            return false;
        }
      }
    }

    libs.Merge(sg_[sg_id_[v_nbr]].bb_liberties());
    Vertex v_tmp = v_nbr;
    do {
      AddEmptyHash(&space_hash, v_tmp);

      for (const auto& d : dir4) {
        Vertex v_nbr2 = v_tmp + dir2v(d);
        if (color_[v_nbr2] == ~c) {
          if (sg_[sg_id_[v_nbr2]].num_liberties() != 2)
            return false;
          else
            libs.Merge(sg_[sg_id_[v_nbr2]].bb_liberties());
        }
      }

      v_tmp = next_v_[v_tmp];
    } while (v_tmp != v_nbr);
  }

  // Adds v to space_hash.
  AddEmptyHash(&space_hash, v);

  return (IsNakadeHash(space_hash) &&
          libs.num_bits() == expected_num_liberties);
}

#endif  // BOARD_H_
