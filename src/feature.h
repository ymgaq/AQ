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

#ifndef FEATURE_H_
#define FEATURE_H_

#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

#include "./bitboard.h"
#include "./config.h"
#include "./pattern.h"
#include "./types.h"

// --------------------
//        Diff
// --------------------

/**
 * @struct LightMap
 * A lightweight map that stores variables of T with an integer key of
 * upper limit Size.
 */
template <typename T, int Size>
struct LightMap {
  std::vector<std::pair<int, T>> entry;
  bool flag[Size];

  LightMap() : flag{false} {}

  void Insert(int idx, T val) {
    if (!flag[idx]) {
      flag[idx] = true;
      entry.push_back({idx, val});
    }
  }
};

/**
 * @struct Diff
 * Diff class keeps history of board properties.
 * See Board class members for detail.
 */
struct Diff {
  LightMap<Vertex, kNumVts> empty;
  LightMap<int, kNumVts> empty_id;
  LightMap<StoneGroup, kNumVts> sg;
  LightMap<int, kNumVts> sg_id;
  LightMap<Vertex, kNumVts> next_v;
  LightMap<uint32_t, kNumVts> ptn;
  LightMap<double, kNumVts> prob[kNumPlayers];
  LightMap<double, kBSize> sum_prob_rank[kNumPlayers];

  Key key;
  Vertex prev_ko;
  Bitboard removed_stones;
  Pattern prev_ptn;
  double prev_rsp_prob;
  Vertex response_move[4];

  Vertex features_add;
  Bitboard features_sub;

  Diff()
      : key(UINT64_MAX),
        prev_ko(kVtNull),
        removed_stones{},
        prev_ptn(0xffffffff),
        prev_rsp_prob(0.0),
        response_move{kVtNull},
        features_add(kVtNull),
        features_sub{} {}
};

// --------------------
//     Feature
// --------------------

class Board;
constexpr int kNumHistory = 8;
constexpr int kFeatureSize = 8;

/**
 * @class Feature
 * Feature class contains input features for neural network.
 *
 *   [0]-[15] : stones 0->my(t) 1->her(t) 2->my(t-1) ...
 *   [16]-[17]: color
 *   [18]-[25]: liberty
 *   [26]-[33]: capture size
 *   [34]-[41]: self Atari size
 *   [42]-[49]: liberty after
 *   [50]     : ladder escape
 *   [51]     : sensibleness
 */
class Feature {
 public:
  Feature()
      : next_side_(kBlack),
        stones_(kNumPlayers, std::vector<std::vector<float>>(
                                 kNumHistory, std::vector<float>(kNumRvts, 0))),
        add_history_(kNumHistory, kVtNull),
        sub_history_(kNumHistory),
        liberty_(kFeatureSize, std::vector<float>(kNumRvts, 0)),
        cap_size_(kFeatureSize, std::vector<float>(kNumRvts, 0)),
        self_atari_(kFeatureSize, std::vector<float>(kNumRvts, 0)),
        liberty_after_(kFeatureSize, std::vector<float>(kNumRvts, 0)),
        ladder_esc_(kNumRvts, 0),
        sensibleness_(kNumRvts, 0) {}

  Feature(const Feature& rhs)
      : next_side_(rhs.next_side_),
        stones_(rhs.stones_),
        add_history_(rhs.add_history_),
        sub_history_(rhs.sub_history_),
        liberty_(rhs.liberty_),
        cap_size_(rhs.cap_size_),
        self_atari_(rhs.self_atari_),
        liberty_after_(rhs.liberty_after_),
        ladder_esc_(rhs.ladder_esc_),
        sensibleness_(rhs.sensibleness_) {}

  void Init() {
    next_side_ = kBlack;
    std::fill(add_history_.begin(), add_history_.end(), kVtNull);

    for (int i = 0; i < kNumHistory; ++i) {
      sub_history_[i].Init();
      std::fill(stones_[kWhite][i].begin(), stones_[kWhite][i].end(),
                float{0.0});
      std::fill(stones_[kBlack][i].begin(), stones_[kBlack][i].end(),
                float{0.0});
    }

    for (int i = 0; i < kFeatureSize; ++i) {
      std::fill(liberty_[i].begin(), liberty_[i].end(), float{0.0});
      std::fill(cap_size_[i].begin(), cap_size_[i].end(), float{0.0});
      std::fill(self_atari_[i].begin(), self_atari_[i].end(), float{0.0});
      std::fill(liberty_after_[i].begin(), liberty_after_[i].end(), float{0.0});
    }

    std::fill(ladder_esc_.begin(), ladder_esc_.end(), float{0.0});
    std::fill(sensibleness_.begin(), sensibleness_.end(), float{0.0});
  }

  Feature& operator=(const Feature& rhs) {
    stones_ = rhs.stones_;
    next_side_ = rhs.next_side_;
    add_history_ = rhs.add_history_;
    sub_history_ = rhs.sub_history_;
    liberty_ = rhs.liberty_;
    cap_size_ = rhs.cap_size_;
    self_atari_ = rhs.self_atari_;
    liberty_after_ = rhs.liberty_after_;
    ladder_esc_ = rhs.ladder_esc_;
    sensibleness_ = rhs.sensibleness_;

    return *this;
  }

  bool operator==(const Feature& rhs) const {
    return next_side_ == rhs.next_side_ && stones_ == rhs.stones_;
  }

  float stones(Color c, int t, int rv) const { return stones_[c][t][rv]; }

  Color next_side() const { return next_side_; }

  Vertex last_add() const { return add_history_[kNumHistory - 1]; }

  Bitboard last_sub() const { return sub_history_[kNumHistory - 1]; }

  float liberty(int t, int rv) const { return liberty_[t][rv]; }

  float cap_size(int t, int rv) const { return cap_size_[t][rv]; }

  float self_atari(int t, int rv) const { return self_atari_[t][rv]; }

  float liberty_after(int t, int rv) const { return liberty_after_[t][rv]; }

  float ladder_esc(int rv) const { return ladder_esc_[rv]; }

  float sensibleness(int rv) const { return sensibleness_[rv]; }

  void DoNullMove() {
    Color c = ~next_side_;

    for (int i = 1; i < kNumHistory; ++i) {
      if (add_history_[i - 1] < kPass)
        stones_[c][i][v2rv(add_history_[i - 1])] = 1.0;
      for (auto& rs : sub_history_[i - 1].Vertices())
        stones_[~c][i][v2rv(rs)] = 0.0;
      c = ~c;
    }

    for (int i = kNumHistory - 1; i > 0; --i) {
      add_history_[i] = add_history_[i - 1];
      sub_history_[i] = sub_history_[i - 1];
    }

    add_history_[0] = kVtNull;
    sub_history_[0].Init();

    next_side_ = ~next_side_;
  }

  void Undo(Vertex v, Bitboard bb) {
    Color c = ~next_side_;

    for (int i = 0; i < kNumHistory; ++i) {
      if (add_history_[i] < kPass) stones_[c][i][v2rv(add_history_[i])] = 0.0;
      for (auto& rs : sub_history_[i].Vertices())
        stones_[~c][i][v2rv(rs)] = 1.0;

      c = ~c;
    }

    for (int i = 0; i < kNumHistory - 1; ++i) {
      add_history_[i] = add_history_[i + 1];
      sub_history_[i] = sub_history_[i + 1];
    }

    add_history_[kNumHistory - 1] = v;
    sub_history_[kNumHistory - 1] = bb;

    next_side_ = ~next_side_;
  }

  void Add(Color c, Vertex v) {
    ASSERT_LV2(kColorZero <= c && c < kNumPlayers);
    ASSERT_LV2(is_ok(v) && !in_wall(v));
    ASSERT_LV2(c == ~next_side_);

    stones_[c][0][v2rv(v)] = 1.0;
    add_history_[0] = v;
  }

  void Remove(Color c, Vertex v) {
    ASSERT_LV2(kColorZero <= c && c < kNumPlayers);
    ASSERT_LV2(is_ok(v) && !in_wall(v));
    ASSERT_LV2(c == next_side_);

    stones_[c][0][v2rv(v)] = 0.0;
    sub_history_[0].Add(v);
  }

  void Update(const Board& b);

  float* Copy(float* oi, bool use_full = true, int symmetry_idx = 0) const;

  /**
   * Outputs Feature information. (for debug)
   */
  friend std::ostream& operator<<(std::ostream& os, const Feature& ft) {
    os << "next_side_=" << ft.next_side_ << std::endl;
    for (int i = 1; i < kNumHistory; ++i) {
      os << "diff(" << i << "): kWhite ";
      for (int j = 0; j < kNumRvts; ++j) {
        if (ft.stones_[kWhite][i - 1][j] != ft.stones_[kWhite][i][j])
          os << rv2v(RawVertex(j)) << " ";
      }
      os << "kBlack ";
      for (int j = 0; j < kNumRvts; ++j) {
        if (ft.stones_[kBlack][i - 1][j] != ft.stones_[kBlack][i][j])
          os << rv2v(RawVertex(j)) << " ";
      }
      os << std::endl;
    }
    return os;
  }

 private:
  std::vector<std::vector<std::vector<float>>> stones_;
  Color next_side_;
  std::vector<Vertex> add_history_;
  std::vector<Bitboard> sub_history_;
  std::vector<std::vector<float>> liberty_;
  std::vector<std::vector<float>> cap_size_;
  std::vector<std::vector<float>> self_atari_;
  std::vector<std::vector<float>> liberty_after_;
  std::vector<float> ladder_esc_;
  std::vector<float> sensibleness_;
};

#endif  // FEATURE_H_
