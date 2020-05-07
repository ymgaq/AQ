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

#ifndef PATTERN_H_
#define PATTERN_H_

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "./types.h"

/**
 * @class Pattern
 * A structure for recognizing a pattern of stone arrangement
 * at a certain coordinate and surrounding 8 points quickly.
 * For example, the judgment of a legal hand can be judged
 * simply by referring to the legal_ptn_ table.
 *
 * [bit field]
 *    0-15  : 3x3 colors. U/R/D/L/RU/RD/LD/LU (2 bits each)
 *    16-23 : extra 4 colors. UU/RR/DD/LL (2 bits each)
 *    24-31 : atari/pre-atari state. U/R/D/L (2 bits each)
 *
 *    color types : kWhite(0b00) / kBlack(0b01) / kEmpty(0b10) / kWall(0b11)
 *    atari types : atari(0b01) / pre-atari(0b10) / others(0b00)
 */
class Pattern {
 public:
  // Constructor
  Pattern() { stones_ = 0x00aaaaaa; }  // all empty

  Pattern(const Pattern& rhs) : stones_(rhs.stones_) {}

  explicit Pattern(const uint32_t st_) { stones_ = st_; }

  Pattern& operator=(const Pattern& rhs) {
    stones_ = rhs.stones_;
    return *this;
  }

  bool operator==(const Pattern& rhs) const { return stones_ == rhs.stones_; }

  /**
   * Initialize the legal_ptn_ and count_ptn_ tables, and read out the
   * probability distribution for each pattern.
   */
  static void Init(std::string prob_dir);

  /**
   * Initializes bits.
   */
  void SetEmpty() { stones_ = 0x00aaaaaa; }

  /**
   * Sets null value (= UINT_MAX)
   */
  void SetNull() { stones_ = 0xffffffff; }

  uint32_t stones() const { return stones_; }

  void set_stones(uint32_t val) { stones_ = val; }

  /**
   * Returns stone color in a direction.
   *
   * d: 0(U),1(R),2(D),3(L),
   *    4(LU),5(RU),6(RD),7(LD),
   *    8(UU),9(RR),10(DD),11(LL)
   */
  Color color_at(Direction d) const { return Color((stones_ >> (2 * d)) & 3); }

  bool is_stone(Direction d) const { return ((stones_ >> (2 * d)) & 3) < 2; }

  /**
   * Updates stone color in a direction.
   *
   * c: 0(kWhite), 1(kBlack), 2(kEmpty), 3(kWall)
   */
  void set_color(Direction d, Color c) {
    stones_ &= ~(3 << (2 * d));
    stones_ |= c << (2 * d);
  }

  /**
   * Flips color of all black/white stones.
   */
  void FlipColor() {
    // 0xa = 0b1010
    // 1. ~stones_ & 0x00aaaaaa: flag of black or white
    // 2. >> 1: make lower bit mask
    // 3. take xor with stone and convert 0b00(kWhite) <-> 0b01(kBlack)
    stones_ ^= (((~stones_) & 0x00aaaaaa) >> 1);
  }

  int CountImpl(Color c) const {
    return static_cast<int>(Color((stones_)&3) == c) +
           static_cast<int>(Color((stones_ >> 2) & 3) == c) +
           static_cast<int>(Color((stones_ >> 4) & 3) == c) +
           static_cast<int>(Color((stones_ >> 6) & 3) == c);
  }

  /**
   * Returns number of stones/empties/walls.
   */
  int count(Color c) const {
    ASSERT_LV2(kColorZero <= c && c < kNumColors);
    return count_ptn_[stones_ & 0xff][c];
  }

  /**
   * Returns whether it is surrounded by stones of c.
   */
  bool enclosed_by(Color c) const {
    ASSERT_LV2(c < kNumPlayers);
    return (count(c) + count(kWall)) == 4;
  }

  /**
   * Sets atari in each direction (URDL).
   */
  void set_atari(bool bn, bool be, bool bs, bool bw) {
    // 1. eliminate pre-atari bit (0b10) of the stone to be atari
    stones_ &= ~((bn << 25) | (be << 27) | (bs << 29) | (bw << 31));
    // 2. add atari (0b01)
    stones_ |= (bn << 24) | (be << 26) | (bs << 28) | (bw << 30);
  }

  /**
   * Clear atari in a single direction (URDL).
   */
  void cancel_atari(bool bn, bool be, bool bs, bool bw) {
    stones_ &= ~((bn << 24) | (be << 26) | (bs << 28) | (bw << 30));
  }

  /**
   * Clear atari in all directions.
   */
  void clear_atari() {
    // 0xa = 0b1010, 0xf = 0b1111
    stones_ &= 0xaaffffff;
  }

  /**
   * Returns whether the neighbor stone is in atari.
   */
  bool atari_at(Direction d) const { return (stones_ >> (24 + 2 * d)) & 1; }

  /**
   * Returns whether any of neighbor stones is atari.
   */
  bool atari() const {
    // 0x55 = 0b01010101
    return (stones_ >> 24) & 0x55;
  }

  /**
   * Sets pre-atari (liberty = 2) in each direction (URDL).
   */
  void set_pre_atari(bool bn, bool be, bool bs, bool bw) {
    // 1. eliminate atari bit (01) of the stone to be pre-atari
    stones_ &= ~((bn << 24) | (be << 26) | (bs << 28) | (bw << 30));
    // 2. add pre-atari (10)
    stones_ |= (bn << 25) | (be << 27) | (bs << 29) | (bw << 31);
  }

  /**
   * Clears pre-atari in a single direction (URDL).
   */
  void cancel_pre_atari(bool bn, bool be, bool bs, bool bw) {
    stones_ &= ~((bn << 25) | (be << 27) | (bs << 29) | (bw << 31));
  }

  /**
   * Clears pre-atari in all directions.
   */
  void clear_pre_atari() {
    // 0x5 = 0b0101, 0xf = 0b1111
    stones_ &= 0x55ffffff;
  }

  /**
   * Returns whether the neighbor stone is pre-atari.
   */
  bool pre_atari_at(Direction d) const { return (stones_ >> (24 + 2 * d)) & 2; }

  /**
   * Returns whether any of next stones is pre-atari.
   */
  bool pre_atari() const {
    // 0xaa = 0b10101010
    return (stones_ >> 24) & 0xaa;
  }

  /**
   * Returns whether player's move into this is legal.
   */
  bool LegalImpl(Color c) const {
    ASSERT_LV2(c < kNumPlayers);

    // 1. Legal if blank vertexes exist in neighbor.
    if (count(kEmpty) != 0) return true;

    int num_stones[2] = {0, 0};  // 0: white, 1: black
    int num_atari[2] = {0, 0};

    // 2. Counts neighbor stones and atari.
    for (Direction d = kDirZero; d < kNumDir4; ++d) {
      Color ci = color_at(d);
      if (ci < kNumPlayers) {
        ++num_stones[ci];
        if (atari_at(d)) ++num_atari[ci];
      }
    }

    // 3. Legal if opponent's stone is atari,
    //    or any of her stones_ is not atari.
    return (num_atari[~c] != 0 || num_atari[c] < num_stones[c]);
  }

  /**
   * Returns whether player's move into this is legal.
   */
  bool legal(Color c) const {
    ASSERT_LV2(c < kNumPlayers);
    ASSERT_LV2((stones_ >> 24) == ((stones_ >> 24) & 0xff));
    return legal_ptn_[stones_ & 0xff][stones_ >> 24][c];
  }

  /**
   * Returns probability of this pettern.
   */
  double prob(Color c, bool restore) const {
    ASSERT_LV2(c < kNumPlayers);
    ASSERT_LV2((stones_ >> 24) == ((stones_ >> 24) & 0xff));
    return prob_ptn3x3_[stones_ & 0xffff][stones_ >> 24][c]
                       [static_cast<int>(restore)];
  }

  /**
   * Returns response probability of this pettern.
   */
  void ResponseProb(double* ptn_prob, double* inv_prob) const {
    if (prob_ptn_rsp_.find(stones_) != prob_ptn_rsp_.end()) {
      auto p = prob_ptn_rsp_.at(stones_);
      *ptn_prob = p[0];
      *inv_prob = p[1];
    } else {
      *ptn_prob = *inv_prob = -1;
    }
  }

  /**
   * Returns Pattern which is rotated clockwise by 90 degrees.
   */
  Pattern Rotate() const {
    // 0x3 = 0b0011, 0xc = 0b1100, 0xf = 0b1111
    Pattern rot_ptn(((stones_ << 2) & 0xfcfcfcfc) |
                    ((stones_ >> 6) & 0x03030303));

    return rot_ptn;
  }

  /**
   * Returns Pattern which is horizontally inverted.
   */
  Pattern Invert() const {
    // 0x3 = 0b0011, 0xc = 0b1100
    Pattern mir_ptn((stones_ & 0x33330033) | ((stones_ << 4) & 0xc0c000c0) |
                    ((stones_ >> 4) & 0x0c0c000c) |
                    ((stones_ << 2) & 0x0000cc00) |
                    ((stones_ >> 2) & 0x00003300));

    return mir_ptn;
  }

  /**
   * Returns Pattern which has the minimum number.
   */
  Pattern MinimumSym() const {
    Pattern tmp_ptn(stones_);
    Pattern min_ptn = tmp_ptn;

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 4; ++j) {
        if (tmp_ptn.stones_ < min_ptn.stones_) min_ptn = tmp_ptn;

        tmp_ptn = tmp_ptn.Rotate();
      }
      tmp_ptn = tmp_ptn.Invert();
    }

    return min_ptn;
  }

  /**
   * Outputs Pattern information. (for debug)
   */
  friend std::ostream& operator<<(std::ostream& os, const Pattern& ptn) {
    auto cl = [&ptn](Direction d) {
      std::string str_[] = {"O", "X", ".", "%"};
      return str_[ptn.color_at(d)];
    };

    auto ap = [&ptn](Direction d) {
      return (ptn.atari_at(d) ? "a" : ptn.pre_atari_at(d) ? "p" : ".");
    };

    os << "  " << cl(kDirUU) << std::endl;
    os << " " << cl(kDirLU) << cl(kDirU) << cl(kDirRU) << "   " << ap(kDirU)
       << std::endl;
    os << cl(kDirLL) << cl(kDirL) << "." << cl(kDirR) << cl(kDirRR) << " ";
    os << ap(kDirL) << " " << ap(kDirR) << std::endl;
    os << " " << cl(kDirLD) << cl(kDirD) << cl(kDirRD) << "   " << ap(kDirD)
       << std::endl;
    os << "  " << cl(kDirDD) << std::endl;

    return os;
  }

  friend void PrintPatternProb();

 private:
  uint32_t stones_;
  // Static tables are initialized in Pattern::Init().
  static double prob_ptn3x3_[65536][256][2][2];
  static bool legal_ptn_[256][256][2];
  static int count_ptn_[256][4];
  static std::unordered_map<uint32_t, std::array<double, 2>> prob_ptn_rsp_;
};

#endif  // PATTERN_H_
