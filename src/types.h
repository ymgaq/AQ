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

#ifndef TYPES_H_
#define TYPES_H_

#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "./config.h"

// --------------------
//        Color
// --------------------

/**
 * @enum Color
 * Color type of stones or players.
 */
enum Color : int32_t {
  kWhite = 0,
  kBlack,
  kEmpty,
  kWall,  // 'Wall' means expansion area outside the normal board.

  kColorZero = 0,
  kNumPlayers = 2,
  kNumColors = 4,
};

/**
 * Returns the opponent's turn.
 */
constexpr Color operator~(Color c) { return (Color)(c ^ 1); }

/**
 * Checks to see if c is a valid color. (for debug)
 */
constexpr bool is_ok(Color c) { return kColorZero <= c && c < kNumColors; }

/**
 * Returns whether c is a stone or not. That is, black or white.
 */
constexpr bool is_stone(Color c) { return c == kBlack || c == kWhite; }

// --------------------
//        Vertex
// --------------------

/**
 * @enum BoardSize
 * Board size and related constants.
 */
enum BoardSize : int32_t {
#if BOARD_SIZE == 19
  kBSize = 19,  // Size of board.
#elif BOARD_SIZE == 13
  kBSize = 13,
#else  // BOARD_SIZE == 9
  kBSize = 9,
#endif
  // Expansion board size.
  kEBSize = kBSize + 2,
  // Number of bitboards required to represent a single board.
  kNumBBs = (kBSize * kBSize) / 64 + 1,

#if BOARD_SIZE == 9
  // 81*2=162 is a little bit small for 9x9 games
  kMaxPly = kBSize * kBSize * 3,
#else
  kMaxPly = kBSize * kBSize * 2,
#endif
};

/**
 * @enum Vertex
 * Coordinates on the expansion board.
 *
 * (21x21, lower left origin)
 *   lower left (0) -> .. -> lower right (20) ->
 *   .. -> upper left (420) -> .. -> upper right (440)
 */
enum Vertex : int32_t {
  kVtZero = 0,
  kNumVts = kEBSize * kEBSize,
  kNumVtsPlus1 = kNumVts + 1,

  kPass = kNumVts,
  kVtNull = kNumVtsPlus1,

  // Relative positions.
  kVtU = +kEBSize,  // up
  kVtR = +1,        // right
  kVtD = -kEBSize,  // down
  kVtL = -1,        // left

  kVtRU = kVtU + kVtR,  // right up
  kVtRD = kVtD + kVtR,  // right down
  kVtLU = kVtU + kVtL,  // left up
  kVtLD = kVtD + kVtL,  // left down

  kVtUU = kVtU + kVtU,  // up up
  kVtRR = kVtR + kVtR,  // right right
  kVtDD = kVtD + kVtD,  // down down
  kVtLL = kVtL + kVtL,  // left left
};

/**
 * @enum RawVertex
 * Coordinate on the physical board.
 *
 * (19x19, lower left origin)
 *   lower left (0) -> .. -> lower right (18) ->
 *   .. -> upper left (342) -> .. -> upper right (360)
 */
enum RawVertex : int32_t {
  kRvtZero = 0,
  kNumRvts = kBSize * kBSize,
  kRvtNull = kNumRvts,
};

/**
 * @struct CoordinateTable
 * Tables for coordinate transformation.
 * These tables will be initialized in the constructor.
 */

struct CoordinateTable {
  // --- Vertex
  int v2x_table[kNumVtsPlus1];
  int v2y_table[kNumVtsPlus1];
  Vertex xy2v_table[kEBSize][kEBSize];
  bool in_wall_table[kNumVtsPlus1];
  int dist_table[kNumVtsPlus1][kNumVtsPlus1];
  int dist_edge_table[kNumVtsPlus1];
  Vertex v2sym_table[8][kNumVtsPlus1];

  // --- RawVertex
  int rv2x_table[kNumRvts];
  int rv2y_table[kNumRvts];
  RawVertex xy2rv_table[kBSize][kBSize];
  Vertex rv2v_table[kNumRvts];
  RawVertex v2rv_table[kNumVtsPlus1];
  int rv2sym_table[8][kNumRvts];

  // --- Bitboard
  int v2bb_idx_table[kNumVtsPlus1];
  uint64_t v2bb_bit_table[kNumVtsPlus1];
  Vertex bb2v_table[kNumBBs][64];

  // --- Nakade
  /**
   * Board hash is obtained by taking the xor of the zobrist hash of all
   * vertices. zobrist_table[Symmetrical][Color][Vertex] is initialized
   * randomly.
   *
   *    axis=0: symmetrical index
   *         [0]->original [1]->90deg rotation, ... [4]->inverting, ...
   *    axis=1: stone color index
   *         [0]->white [1]->black [2]->empty [3]->ko
   *    axis=2: vertex position
   */
  uint64_t zobrist_table[8][4][kNumVts];

  /**
   * Hash map containing shapes with N vertices of Nakade.
   *
   *    key   : Zobrist hash
   *    value : vital vertex of Nakade
   */
  std::unordered_map<uint64_t, Vertex> nakade_map;

  /**
   * Hash set containing 'bent four' shapes,
   * which is used on ended board to determin
   * if the stones are Seki or dead.
   */
  std::unordered_set<uint64_t> bent4_set;

  // Constructor
  CoordinateTable();
};
const CoordinateTable kCoordTable;

// -- Vertex conversion

/**
 * Checks to see if v is a valid vertex. (for debug)
 */
constexpr bool is_ok(Vertex v) { return kVtZero <= v && v <= kNumVts; }

/**
 * Returns the x-coordinate of v.
 */
inline int x_of(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return kCoordTable.v2x_table[v];
}

/**
 * Returns the y-coordinate of v.
 */
inline int y_of(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return kCoordTable.v2y_table[v];
}

/**
 * Returns a vertex from (x,y) coordinates.
 */
inline Vertex xy2v(int x, int y) {
  ASSERT_LV2(0 <= x && x < kEBSize);
  ASSERT_LV2(0 <= y && y < kEBSize);
  Vertex v = kCoordTable.xy2v_table[x][y];
  ASSERT_LV2(is_ok(v));
  return v;
}

/**
 * Returns a vertex that has been symmetrically manipulated by a symmetric
 * index.
 */
inline Vertex v2sym(Vertex v, int symmetry_idx) {
  ASSERT_LV2(is_ok(v) && symmetry_idx >= 0 && symmetry_idx < 8);
  return kCoordTable.v2sym_table[symmetry_idx][v];
}

/**
 * Returns whether v is in expansion area.
 */
inline bool in_wall(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return kCoordTable.in_wall_table[v];
}

/**
 * Returns Manhattan distance between v1 and v2.
 */
inline int dist(Vertex v1, Vertex v2) {
  ASSERT_LV2(is_ok(v1));
  ASSERT_LV2(is_ok(v2));
  return kCoordTable.dist_table[v1][v2];
}

/**
 * Returns distance from edge of the expansion board.
 * NOTE: Returns 1 if v = (1,1).
 */
inline int dist_edge(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return kCoordTable.dist_edge_table[v];
}

/**
 * Returns a vertex with 180 degree rotation.
 */
inline Vertex inv(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return (Vertex)((kNumVts - 1) - v);
}

/**
 * Returns a vertex reversed on the y-axis.
 */
inline Vertex mir(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return xy2v(kEBSize - 1 - x_of(v), y_of(v));
}

/**
 * Outputs coordinate on physical board. (for debug)
 * @code
 * Vertex v = xy2v(3,3);
 * std::cout << v << std::endl; // C3
 * @endcode
 */
inline std::ostream& operator<<(std::ostream& os, Vertex v) {
  if (v == kVtNull)
    os << "NULL";
  else if (v == kPass)
    os << "PASS";
  else
    os << "ABCDEFGHJKLMNOPQRST"[x_of(v) - 1] << std::to_string(y_of(v));
  return os;
}

// -- RawVertex conversion

/**
 * Checks to see if rv is valid. (for debug)
 */
constexpr bool is_ok(RawVertex rv) { return kRvtZero <= rv && rv < kNumRvts; }

/**
 * Returns the x-coordinate of rv.
 */
inline int x_of(RawVertex rv) {
  ASSERT_LV2(is_ok(rv));
  return kCoordTable.rv2x_table[rv];
}

/**
 * Returns the y-coordinate of rv.
 */
inline int y_of(RawVertex rv) {
  ASSERT_LV2(is_ok(rv));
  return kCoordTable.rv2y_table[rv];
}

/**
 * Returns a RawVertex from (rx,ry) coordinates.
 */
inline RawVertex xy2rv(int rx, int ry) {
  ASSERT_LV2(0 <= rx && rx < kBSize);
  ASSERT_LV2(0 <= ry && ry < kBSize);
  RawVertex rv = kCoordTable.xy2rv_table[rx][ry];
  ASSERT_LV2(is_ok(rv));
  return rv;
}

/**
 * Converts Vertex to RawVertex.
 */
inline Vertex rv2v(RawVertex rv) {
  ASSERT_LV2(is_ok(rv));
  return kCoordTable.rv2v_table[rv];
}

/**
 * Converts RawVertex to Vertex.
 */
inline RawVertex v2rv(Vertex v) {
  ASSERT_LV2(is_ok(v));
  return kCoordTable.v2rv_table[v];
}

/**
 * Returns a RawVertex that has been symmetrically manipulated by a symmetric
 * index.
 */
inline int rv2sym(int rv, int symmetry_idx) {
  return kCoordTable.rv2sym_table[symmetry_idx][rv];
}

// --------------------
//      Direction
// --------------------

/**
 * @enum Direction
 * Direction between vertices.
 */
enum Direction : int32_t {
  kDirU,
  kDirR,
  kDirD,
  kDirL,
  kDirLU,
  kDirRU,
  kDirRD,
  kDirLD,
  kDirUU,
  kDirRR,
  kDirDD,
  kDirLL,

  kDirZero = 0,
  kNumDir4 = 4,
  kNumDir8 = 8,
  kNumDirs = 12,
};

constexpr Direction dir2opp_[kNumDirs] = {kDirD,  kDirL,  kDirU,  kDirR,
                                          kDirRD, kDirLD, kDirLU, kDirRU,
                                          kDirDD, kDirLL, kDirUU, kDirRR};

constexpr Vertex dir2v_[kNumDirs] = {kVtU,  kVtR,  kVtD,  kVtL,  kVtLU, kVtRU,
                                     kVtRD, kVtLD, kVtUU, kVtRR, kVtDD, kVtLL};

constexpr Direction dir4[4] = {kDirU, kDirR, kDirD, kDirL};

constexpr Direction diag4[4] = {kDirLU, kDirRU, kDirRD, kDirLD};

constexpr Direction dir8[8] = {kDirU,  kDirR,  kDirD,  kDirL,
                               kDirLU, kDirRU, kDirRD, kDirLD};

constexpr Direction dir12[12] = {kDirU,  kDirR,  kDirD,  kDirL,
                                 kDirLU, kDirRU, kDirRD, kDirLD,
                                 kDirUU, kDirRR, kDirDD, kDirLL};

constexpr Vertex dv4[4] = {kVtU, kVtR, kVtD, kVtL};

constexpr Vertex dv8[8] = {kVtU, kVtR, kVtD, kVtL, kVtLU, kVtRU, kVtRD, kVtLD};

constexpr Vertex dv12[12] = {kVtU,  kVtR,  kVtD,  kVtL,  kVtLU, kVtRU,
                             kVtRD, kVtLD, kVtUU, kVtRR, kVtDD, kVtLL};

/**
 * Checks to see if d is a valid direction. (for debug)
 */
constexpr bool is_ok(Direction d) { return kDirZero <= d && d < kNumDirs; }

/**
 * Returns the opposite direction of d.
 */
constexpr Direction operator~(Direction d) { return dir2opp_[d]; }

/**
 * Returns the relative vertex of d.
 */
constexpr Vertex dir2v(Direction d) { return dir2v_[d]; }

// --------------------
//        Rules
// --------------------

/**
 * @enum Rule
 * Game rules.
 *
 *   Chinese: 0
 *   Japanese: 1
 *   Tromp Tralor: 2
 */
enum Rule : int32_t {
  kChinese,
  kJapanese,
  kTrompTraylor,
};

/**
 * @enum RepetitonRule
 * Repetition rules.
 */
enum RepetitionRule : int32_t {
  kRepRuleDraw,     // Draw
  kRepRuleSuperKo,  // Super-Ko
  kRepRuleTromp,    // Tromp-Tralor rule
};

/**
 * @enum RepetitionState
 * State of repetition.
 */
enum RepetitionState : int32_t {
  kRepetitionNone,  // Not repetition.
  kRepetitionWin,   // Win on Super-Ko or Tromp-Tralor rule.
  kRepetitionLose,  // Lose on Super-Ko or Tromp-Tralor rule.
  kRepetitionDraw,  // Draw on Japanese rule.
  kNumRepetitions,
};

inline bool is_ok(RepetitionState rs) {
  return kRepetitionNone <= rs && rs < kNumRepetitions;
}

// --------------------
//   Random generator
// --------------------

/**
 * @class RandomGenerator
 * Returns a random number generated by Mersenne Twister.
 * Iinitialized as a static class and not instantiated.
 */
class RandomGenerator {
 public:
  RandomGenerator() = delete;

  RandomGenerator(const RandomGenerator& rhs) = delete;

  static double RandDouble() { return uniform_double_(mt_); }

  static double RandSymmetry() { return uniform_int_(mt_); }

  static double RandomNoise() { return gamma_double_(mt_); }

  static void SetDirichletNoise(double dirichlet_noise) {
    gamma_double_ =
        std::move(std::gamma_distribution<double>(dirichlet_noise, 1.0));
  }

 private:
  static std::random_device rd_;
  static std::mt19937 mt_;
  static std::uniform_real_distribution<double> uniform_double_;
  static std::uniform_int_distribution<int> uniform_int_;
  static std::gamma_distribution<double> gamma_double_;
};

/**
 * Returns a random real number of [0.0, 1.0).
 */
inline double RandDouble() { return RandomGenerator::RandDouble(); }

/**
 * Returns a random integer of [0, 8).
 */
inline int RandSymmetry() { return RandomGenerator::RandSymmetry(); }

/**
 * Returns a noise value based on the gamma distribution.
 */
inline double RandNoise() { return RandomGenerator::RandomNoise(); }

// --------------------
//        Nakade
// --------------------

/**
 * Hash key of board.
 */
typedef uint64_t Key;

/**
 * Adds a Zobrist hash of empty at v to the hash_key.
 */
inline void AddEmptyHash(uint64_t* hash_key, Vertex v) {
  ASSERT_LV2(0 <= v && v < kNumVts);
  *hash_key ^= kCoordTable.zobrist_table[0][2][v];
}

/**
 * Returns whether hash_key is included in the nakade map.
 */
inline bool IsNakadeHash(uint64_t hash_key) {
  return kCoordTable.nakade_map.find(hash_key) != kCoordTable.nakade_map.end();
}

/**
 * Returns whether hash_key is included in the bent4 set.
 */
inline bool IsBentFourHash(uint64_t hash_key) {
  return kCoordTable.bent4_set.find(hash_key) != kCoordTable.bent4_set.end();
}

// --------------------
//    Operator Macro
// --------------------

/**
 * @def
 * Macro that allows arithmetic operation to type enumerators.
 */
#define ENABLE_BASE_OPERATORS_ON(T)                                 \
  inline T operator+(const T d1, const T d2) {                      \
    return T(static_cast<int>(d1) + static_cast<int>(d2));          \
  }                                                                 \
  inline T operator-(const T d1, const T d2) {                      \
    return T(static_cast<int>(d1) - static_cast<int>(d2));          \
  }                                                                 \
  inline T operator-(const T d) { return T(-static_cast<int>(d)); } \
  inline T& operator+=(T& d1, const T d2) { return d1 = d1 + d2; }  \
  inline T& operator-=(T& d1, const T d2) { return d1 = d1 - d2; }

#define ENABLE_FULL_OPERATORS_ON(T)                                     \
  ENABLE_BASE_OPERATORS_ON(T)                                           \
  inline T operator*(const int i, const T d) {                          \
    return T(i * static_cast<int>(d));                                  \
  }                                                                     \
  inline T operator*(const T d, const int i) {                          \
    return T(static_cast<int>(d) * i);                                  \
  }                                                                     \
  inline T& operator*=(T& d, const int i) {                             \
    return d = T(static_cast<int>(d) * i);                              \
  }                                                                     \
  inline T& operator++(T& d) { return d = T(static_cast<int>(d) + 1); } \
  inline T& operator--(T& d) { return d = T(static_cast<int>(d) - 1); } \
  inline T operator++(T& d, int) {                                      \
    T prev = d;                                                         \
    d = T(static_cast<int>(d) + 1);                                     \
    return prev;                                                        \
  }                                                                     \
  inline T operator--(T& d, int) {                                      \
    T prev = d;                                                         \
    d = T(static_cast<int>(d) - 1);                                     \
    return prev;                                                        \
  }                                                                     \
  inline T operator/(T d, int i) { return T(static_cast<int>(d) / i); } \
  inline int operator/(T d1, T d2) {                                    \
    return static_cast<int>(d1) / static_cast<int>(d2);                 \
  }                                                                     \
  inline T& operator/=(T& d, int i) { return d = T(static_cast<int>(d) / i); }

ENABLE_FULL_OPERATORS_ON(Color)
ENABLE_FULL_OPERATORS_ON(Vertex)
ENABLE_FULL_OPERATORS_ON(RawVertex)
ENABLE_FULL_OPERATORS_ON(Direction)

/**
 * @def
 * Macro that allows range based for-loop to type enumerations.
 */
#define ENABLE_RANGE_OPERATORS_ON(X, ZERO, NB) \
  inline X operator*(X x) { return x; }        \
  inline X begin(X) { return ZERO; }           \
  inline X end(X) { return NB; }

ENABLE_RANGE_OPERATORS_ON(Vertex, kVtZero, kNumVts)
ENABLE_RANGE_OPERATORS_ON(Color, kColorZero, kNumPlayers)

#endif  // TYPES_H_
