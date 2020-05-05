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

#ifndef BITBOARD_H_
#define BITBOARD_H_

#include <utility>
#include <vector>

#include "./types.h"

#ifdef __GNUC__
/**
 * Function for finding an unsigned 64-integer NTZ.
 * Returns closest position of 1 from lower bit.
 *
 *   0b01011000 -> 3
 *   0b0        -> 64
 */
static constexpr int ntz(uint64_t x) noexcept { return __builtin_ctzll(x); }

static constexpr int PopCount(uint64_t x) noexcept {
  return __builtin_popcountll(x);
}
#else  // Linux
constexpr uint64_t kNtzMagic64 = 0x03F0A933ADCBD8D1ULL;
constexpr int kNtzTable64[127] = {
    64, 0,  -1, 1,  -1, 12, -1, 2,  60, -1, 13, -1, -1, 53, -1, 3,  61, -1, -1,
    21, -1, 14, -1, 42, -1, 24, 54, -1, -1, 28, -1, 4,  62, -1, 58, -1, 19, -1,
    22, -1, -1, 17, 15, -1, -1, 33, -1, 43, -1, 50, -1, 25, 55, -1, -1, 35, -1,
    38, 29, -1, -1, 45, -1, 5,  63, -1, 11, -1, 59, -1, 52, -1, -1, 20, -1, 41,
    23, -1, 27, -1, -1, 57, 18, -1, 16, -1, 32, -1, 49, -1, -1, 34, 37, -1, 44,
    -1, -1, 10, -1, 51, -1, 40, -1, 26, 56, -1, -1, 31, 48, -1, 36, -1, 9,  -1,
    39, -1, -1, 30, 47, -1, 8,  -1, -1, 46, 7,  -1, 6,
};
/**
 * Function for finding an unsigned 64-integer NTZ.
 * Returns closest position of 1 from lower bit.
 *
 *   0b01011000 -> 3
 *   0b0        -> 64
 */
static constexpr int ntz(uint64_t x) noexcept {
  return kNtzTable64[static_cast<uint64_t>(kNtzMagic64 *
                                           static_cast<uint64_t>(x & -x)) >>
                     57];
}

static int PopCount(uint64_t x) noexcept { return __popcnt64(x); }
#endif

/**
 * Returns the index of the bitboard containing v.
 */
inline int v2bb_idx(Vertex v) { return kCoordTable.v2bb_idx_table[v]; }

/**
 * Returns the bit where v is located on the bitboard.
 */
inline uint64_t v2bb_bit(Vertex v) { return kCoordTable.v2bb_bit_table[v]; }

/**
 * Return a vertex from the bitboard.
 */
inline Vertex bb2v(int bb_idx, int bit) {
  return kCoordTable.bb2v_table[bb_idx][bit];
}

// --------------------
//       Bitboard
// --------------------

/**
 * @class Bitboard
 * Bitboard class is composed of kNumBBs of 64-bit integers.
 * (kNumBBs=6 in 19x19 board, 2 in 9x9 board)
 *
 * Mainly, the board coordinates are handled in a one-dimensional
 * system, but sometimes Bitboard may be faster, such as when
 * checking liberty verteces of stones.
 */
class Bitboard {
 public:
  // Constructor
  Bitboard() : p_{0}, num_bits_(0) {}

  Bitboard(const Bitboard& rhs) : num_bits_(rhs.num_bits_) {
    for (int i = 0; i < kNumBBs; ++i) p_[i] = rhs.p_[i];
  }

  Bitboard& operator=(const Bitboard& rhs) {
    for (int i = 0; i < kNumBBs; ++i) p_[i] = rhs.p_[i];
    num_bits_ = rhs.num_bits_;

    return *this;
  }

  bool operator==(const Bitboard& rhs) const {
    bool is_equal = (num_bits_ == rhs.num_bits_);
    for (int i = 0; i < kNumBBs; ++i) is_equal &= (p_[i] == rhs.p_[i]);

    return is_equal;
  }

  int num_bits() const { return num_bits_; }

  void set_num_bits(int val) { num_bits_ = val; }

  uint64_t p(int idx) const { return p_[idx]; }

  void set_p(int idx, uint64_t val) { p_[idx] = val; }

  void Init() {
    for (int i = 0; i < kNumBBs; ++i) p_[i] = 0;
    num_bits_ = 0;
  }

  /**
   * Adds a vertex to bitboard.
   */
  void Add(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kNumVts);
    ASSERT_LV2(0 <= v2bb_idx(v) && v2bb_idx(v) < kNumBBs);
    ASSERT_LV2(((v2bb_bit(v) - 1) & v2bb_bit(v)) == 0);  // single bit

    auto& p_op = p_[v2bb_idx(v)];
    uint64_t bit_v = v2bb_bit(v);

    num_bits_ += static_cast<int>((p_op & bit_v) == 0);
    p_op |= bit_v;
  }

  /**
   * Deletes a vertex from bitboard.
   */
  void Remove(Vertex v) {
    ASSERT_LV2(kVtZero <= v && v < kNumVts);
    ASSERT_LV2(0 <= v2bb_idx(v) && v2bb_idx(v) < kNumBBs);
    ASSERT_LV2(((v2bb_bit(v) - 1) & v2bb_bit(v)) == 0);  // single bit

    auto& p_op = p_[v2bb_idx(v)];
    uint64_t bit_v = v2bb_bit(v);

    num_bits_ -= static_cast<int>((p_op & bit_v) != 0);
    p_op &= ~bit_v;
  }

  /**
   * Merges with another bitboard.
   */
  void Merge(const Bitboard& rhs) {
    num_bits_ = 0;
    for (int i = 0; i < kNumBBs; ++i) {
      p_[i] |= rhs.p_[i];

      if (p_[i]) num_bits_ += PopCount(p_[i]);
    }
  }

  /**
   * Gets list of vertices.
   */
  std::vector<Vertex> Vertices() const {
    std::vector<Vertex> vs;
    int num_op = num_bits_;
    int bb_idx = 0;
    uint64_t p_op = p_[0];

    while (num_op > 0) {
      if (p_op == 0) {
        p_op = p_[++bb_idx];
        continue;
      }

      int bit_idx = ntz(p_op);

      ASSERT_LV2(0 <= bb_idx && bb_idx < kNumBBs);
      ASSERT_LV2(0 <= bit_idx && bit_idx < 64);
      ASSERT_LV2((p_op & (0x1ULL << bit_idx)) != 0);

      vs.push_back(bb2v(bb_idx, bit_idx));
      p_op ^= (0x1ULL << bit_idx);
      --num_op;
    }

    ASSERT_LV2(static_cast<int>(vs.size()) == num_bits_);

    return std::move(vs);
  }

  /**
   * Returns the first vertex.
   */
  Vertex FirstVertex() const {
    for (int i = 0; i < kNumBBs; ++i)
      if (p_[i] != 0) return bb2v(i, ntz(p_[i]));

    return kVtNull;
  }

  /**
   * Outputs bitboard information. (for debug)
   */
  friend std::ostream& operator<<(std::ostream& os, const Bitboard& bb) {
    os << "num_bits_=" << bb.num_bits_;
    os << " p_: ";
    for (auto& rs : bb.Vertices()) os << rs << " ";
    os << std::endl;
    return os;
  }

 private:
  uint64_t p_[kNumBBs];  // 64-bit integers that make up the bitboard.
  int num_bits_;         // Number of vertex bits in the bitboard.
};

// --------------------
//      StoneGroup
// --------------------

/**
 * @class StoneGroup
 * StoneGroup class contains information in adjacent stones,
 * such as liberty points or stone count.
 *
 * Stones are taken when adjacent liberties becomes 0.
 */
class StoneGroup {
 public:
  // Constructor
  StoneGroup() : liberty_atari_(kVtNull), num_stones_(1) {}

  StoneGroup(const StoneGroup& rhs)
      : liberty_atari_(rhs.liberty_atari_),
        num_stones_(rhs.num_stones_),
        bb_liberties_(rhs.bb_liberties_) {}

  StoneGroup& operator=(const StoneGroup& rhs) {
    num_stones_ = rhs.num_stones_;
    liberty_atari_ = rhs.liberty_atari_;
    bb_liberties_ = rhs.bb_liberties_;

    return *this;
  }

  bool operator==(const StoneGroup& rhs) const {
    return num_stones_ == rhs.num_stones_ &&
           liberty_atari_ == rhs.liberty_atari_ &&
           bb_liberties_ == rhs.bb_liberties_;
  }

  /**
   * Returns number of stones.
   */
  int size() const { return num_stones_; }

  Vertex liberty_atari() const { return liberty_atari_; }

  Bitboard bb_liberties() const { return bb_liberties_; }

  int num_liberties() const { return bb_liberties_.num_bits(); }

  std::vector<Vertex> lib_vertices() const {
    return std::move(bb_liberties_.Vertices());
  }

  bool captured() const { return bb_liberties_.num_bits() == 0; }

  bool atari() const { return bb_liberties_.num_bits() == 1; }

  bool pre_atari() const { return bb_liberties_.num_bits() == 2; }

  void Init() {
    liberty_atari_ = kVtNull;
    num_stones_ = 1;
    bb_liberties_.Init();
  }

  void SetNull() {
    liberty_atari_ = kVtNull;
    bb_liberties_.Init();
    bb_liberties_.set_num_bits(int{kVtNull});
    num_stones_ = int{kVtNull};
  }

  /**
   * Adds a stone at v to this group.
   */
  void Add(Vertex v) {
    if (bb_liberties_.num_bits() == kVtNull) return;  // wall

    bb_liberties_.Add(v);
    // liberty_atari_ is called only when num_liberties == 1
    liberty_atari_ = v;
  }

  /**
   * Remove a stone at v from this group.
   */
  void Remove(Vertex v) {
    if (bb_liberties_.num_bits() == kVtNull) return;  // wall

    bb_liberties_.Remove(v);
    if (bb_liberties_.num_bits() == 1)
      liberty_atari_ = bb_liberties_.FirstVertex();
  }

  /**
   * Merge with another stone group.
   */
  void Merge(const StoneGroup& rhs) {
    bb_liberties_.Merge(rhs.bb_liberties_);
    if (bb_liberties_.num_bits() == 1)
      liberty_atari_ = bb_liberties_.FirstVertex();
    num_stones_ += rhs.num_stones_;
  }

  /**
   * Outputs StoneGroup information. (for debug)
   */
  friend std::ostream& operator<<(std::ostream& os, const StoneGroup& sg) {
    os << "liberty_atari_=" << sg.liberty_atari_
       << " num_stones_=" << sg.num_stones_ << std::endl;
    os << "bb_liberties_: " << sg.bb_liberties_ << std::endl;
    return os;
  }

 private:
  Bitboard bb_liberties_;  // Bitboard of liberties.
  Vertex liberty_atari_;   // Vertex of liberty when in Atari.
  int num_stones_;         // Number of stones in this stone group.
};

#endif  // BITBOARD_H_
