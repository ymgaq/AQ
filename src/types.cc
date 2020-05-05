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

#include "./types.h"

// --- Random generator

std::random_device RandomGenerator::rd_;
std::mt19937 RandomGenerator::mt_(RandomGenerator::rd_());
std::uniform_real_distribution<double> RandomGenerator::uniform_double_(0.0,
                                                                        1.0);
std::uniform_int_distribution<int> RandomGenerator::uniform_int_(0, 7);
std::gamma_distribution<double> RandomGenerator::gamma_double_(0.03, 1.0);

/**
 * @namespace
 * Obscure namespace for defining auxiliary functions for initialization of
 * CoordinateTable.
 */
namespace {
template <int N, int M>
void AddNakadeHash(const Vertex (&space)[N][M], const Vertex (&vital)[N],
                   const uint64_t (&zobrist)[8][4][kNumVts],
                   std::unordered_map<uint64_t, Vertex>* nakade);

void InitNakade(const uint64_t (&zobrist)[8][4][kNumVts],
                std::unordered_map<uint64_t, Vertex>* nakade,
                std::unordered_set<uint64_t>* bent4);
}  // namespace

/**
 * Initializes tables for coordinate transformation.
 */
CoordinateTable::CoordinateTable() {
  // Vertex -> x,y,rv
  for (int i = 0; i < kNumVts; ++i) {
    v2x_table[i] = i % int{kEBSize};
    v2y_table[i] = i / int{kEBSize};
    in_wall_table[i] =
        (v2x_table[i] == 0 || v2y_table[i] == 0 ||
         v2x_table[i] == int{kEBSize} - 1 || v2y_table[i] == int{kEBSize} - 1);

    if (in_wall_table[i])
      v2rv_table[i] = kRvtNull;
    else
      v2rv_table[i] =
          RawVertex((v2x_table[i] - 1) + (v2y_table[i] - 1) * int{kBSize});
  }

  v2x_table[kNumVts] = kEBSize - 1;
  v2y_table[kNumVts] = kEBSize - 1;
  in_wall_table[kNumVts] = true;
  v2rv_table[kNumVts] = kRvtNull;

  // x,y -> Vertex
  for (int i = 0; i < kEBSize; ++i) {
    for (int j = 0; j < kEBSize; ++j) {
      xy2v_table[i][j] = Vertex(i + j * int{kEBSize});
    }
  }

  // RawVertex -> x,y
  for (int i = 0; i < kNumRvts; ++i) {
    rv2x_table[i] = i % int{kBSize};
    rv2y_table[i] = i / int{kBSize};
    rv2v_table[i] =
        Vertex((rv2x_table[i] + 1) + (rv2y_table[i] + 1) * int{kEBSize});
  }

  // x,y -> RawVertex
  for (int i = 0; i < kBSize; ++i) {
    for (int j = 0; j < kBSize; ++j) {
      xy2rv_table[i][j] = RawVertex(i + j * int{kBSize});
    }
  }

  // v,rv -> sym
  for (int i = 0; i < 8; ++i) {
    for (Vertex v = kVtZero; v < kNumVts; ++v) {
      int x = kEBSize - 1 - v2x_table[v];
      int y = v2y_table[v];

      if (i == 0)
        v2sym_table[i][v] = v;
      else if (i == 4)
        v2sym_table[i][v] = v2sym_table[0][xy2v_table[x][y]];  // Inverts.
      else
        v2sym_table[i][v] = v2sym_table[i - 1][xy2v_table[y][x]];  // Rotates.
    }
    for (RawVertex rv = kRvtZero; rv < kNumRvts; ++rv) {
      int x = kBSize - 1 - rv2x_table[rv];
      int y = rv2y_table[rv];

      if (i == 0)
        rv2sym_table[i][rv] = int{rv};
      else if (i == 4)
        rv2sym_table[i][rv] = rv2sym_table[0][xy2rv_table[x][y]];  // Inverts.
      else
        rv2sym_table[i][rv] =
            rv2sym_table[i - 1][xy2rv_table[y][x]];  // Rotates.
    }
    v2sym_table[i][kNumVts] = kPass;
  }

  // Distance
  for (int i = 0; i < kNumVtsPlus1; ++i) {
    int dx_edge =
        (std::min)(v2x_table[i], std::abs(kEBSize - 1 - v2x_table[i]));
    int dy_edge =
        (std::min)(v2y_table[i], std::abs(kEBSize - 1 - v2y_table[i]));
    dist_edge_table[i] = (std::min)(dx_edge, dy_edge);

    for (int j = 0; j < kNumVtsPlus1; ++j) {
      if (i == kNumVts || j == kNumVts) {
        dist_table[i][j] = 3 * (kEBSize - 1);
      } else {
        int dx = std::abs(v2x_table[j] - v2x_table[i]);
        int dy = std::abs(v2y_table[j] - v2y_table[i]);
        dist_table[i][j] = dx + dy + (std::max)(dx, dy);
      }
    }
  }

  // Bitboard
  for (int i = 0; i < kNumVtsPlus1; ++i) {
    v2bb_idx_table[i] = v2rv_table[i] / 64;
    if (v2rv_table[i] == kRvtNull)
      v2bb_bit_table[i] = 0;
    else
      v2bb_bit_table[i] = 0x1ULL << (v2rv_table[i] % 64);
  }

  for (int i = 0; i < kNumBBs; ++i) {
    for (int j = 0; j < 64; ++j) {
      RawVertex rv = RawVertex(i * 64 + j);
      if (kRvtZero <= rv && rv < kNumRvts)
        bb2v_table[i][j] = rv2v_table[rv];
      else
        bb2v_table[i][j] = kVtNull;
    }
  }

  // Zobrist hash
  std::mt19937_64 mt_64_(123);

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 4; ++j) {
      for (Vertex v = kVtZero; v < kNumVts; ++v) {
        int x = kEBSize - 1 - v2x_table[v];
        int y = v2y_table[v];

        if (i == 0)
          zobrist_table[i][j][v] = mt_64_();
        else if (i == 4)
          zobrist_table[i][j][v] =
              zobrist_table[0][j][xy2v_table[x][y]];  // Inverts.
        else
          zobrist_table[i][j][v] =
              zobrist_table[i - 1][j][xy2v_table[y][x]];  // Rotates.
      }
    }
  }

  // Nakade
  InitNakade(zobrist_table, &nakade_map, &bent4_set);
}

namespace {

template <int N, int M>
void AddNakadeHash(const Vertex (&space)[N][M], const Vertex (&vital)[N],
                   const uint64_t (&zobrist)[8][4][kNumVts],
                   std::unordered_map<uint64_t, Vertex>* nakade) {
  Vertex sym_pos[8][kNumVts];

  for (int i = 0; i < 8; ++i) {
    for (Vertex v = kVtZero; v < kNumVts; ++v) {
      int x = kEBSize - 1 - int{v} % int{kEBSize};
      int y = int{v} / int{kEBSize};

      if (i == 0)
        sym_pos[i][v] = v;
      else if (i == 4)
        sym_pos[i][v] = sym_pos[0][x + y * int{kEBSize}];  // Inverts.
      else
        sym_pos[i][v] = sym_pos[i - 1][y + x * int{kEBSize}];  // Rotates.
    }
  }

  auto is_in_wall = [](Vertex v) {
    int x = int{v} % int{kEBSize};
    int y = int{v} / int{kEBSize};
    return (x == 0 || x == int{kEBSize} - 1 || y == 0 || y == int{kEBSize} - 1);
  };

  for (Vertex v = kVtZero; v < kNumVts; ++v) {
    if (is_in_wall(v)) continue;

    for (int i = 0; i < 8; ++i) {
      for (int j = 0; j < N; ++j) {
        uint64_t space_hash = 0;
        bool inside = true;

        for (int k = 0; k < M; ++k) {
          Vertex v_k = v + space[j][k];
          if (0 <= v_k && v_k < kNumVts && !is_in_wall(v_k))
            space_hash ^= zobrist[i][2][v_k];
          else
            inside = false;
        }

        if (inside) nakade->insert({space_hash, sym_pos[i][v + vital[j]]});
      }
    }
  }  // for v
}

/**
 * Initializes nakade tables.
 */
void InitNakade(const uint64_t (&zobrist)[8][4][kNumVts],
                std::unordered_map<uint64_t, Vertex>* nakade,
                std::unordered_set<uint64_t>* bent4) {
  const Vertex space_3[4][3] = {
      {kVtL, kVtZero, kVtR},   // kVtZero <- vital position
      {kVtZero, kVtR, kVtU},   // kVtZero
      {kVtZero, kVtR, kVtRR},  // kVtR
      {kVtZero, kVtR, kVtRU}   // kVtR
  };

  const Vertex space_4[4][4] = {
      {kVtL, kVtZero, kVtR, kVtU},    // kVtZero
      {kVtZero, kVtR, kVtRD, kVtRU},  // kVtR
      {kVtZero, kVtR, kVtRR, kVtRU},  // kVtR
      {kVtZero, kVtR, kVtU, kVtRU}    // kVtZero
  };

  const Vertex space_5[7][5] = {
      {kVtL, kVtZero, kVtR, kVtLU, kVtU},           // kVtZero
      {kVtL, kVtZero, kVtR, kVtD, kVtU},            // kVtZero
      {kVtZero, kVtR, kVtRR, kVtU, kVtRU},          // kVtR
      {kVtZero, kVtR, kVtRR, kVtRU, kVtRU + kVtR},  // kVtR
      {kVtZero, kVtR, kVtRD, kVtU, kVtRU},          // kVtR
      {kVtZero, kVtR, kVtRR, kVtRD, kVtRU},         // kVtR
      {kVtZero, kVtR, kVtU, kVtRU, kVtRU + kVtR}    // RU
  };

  const Vertex space_6[4][6] = {
      {kVtLD, kVtD, kVtL, kVtZero, kVtR, kVtU},                 // kVtZero
      {kVtZero, kVtR, kVtRR, kVtRD, kVtU, kVtRU},               // kVtR
      {kVtRD, kVtZero, kVtR, kVtRR, kVtRU, kVtRU + kVtR},       // kVtR
      {kVtZero, kVtR, kVtU, kVtRU, kVtRU + kVtR, kVtUU + kVtR}  // RU
  };

  const Vertex vital_3[4] = {kVtZero, kVtZero, kVtR, kVtR};
  const Vertex vital_4[4] = {kVtZero, kVtR, kVtR, kVtZero};
  const Vertex vital_5[7] = {kVtZero, kVtZero, kVtR, kVtR, kVtR, kVtR, kVtRU};
  const Vertex vital_6[4] = {kVtZero, kVtR, kVtR, kVtRU};

  AddNakadeHash<4, 3>(space_3, vital_3, zobrist, nakade);
  AddNakadeHash<4, 4>(space_4, vital_4, zobrist, nakade);
  AddNakadeHash<7, 5>(space_5, vital_5, zobrist, nakade);
  AddNakadeHash<4, 6>(space_6, vital_6, zobrist, nakade);

  Vertex v_corner = kVtZero + kVtRU;
  Vertex space_bend[2][3] = {
      {v_corner, v_corner + kVtR, v_corner + kVtU},
      {v_corner, v_corner + kVtU, v_corner + kVtUU},
  };

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 2; ++j) {
      uint64_t space_hash = 0;
      for (int k = 0; k < 3; ++k) {
        space_hash ^= zobrist[i][2][space_bend[j][k]];
      }
      bent4->insert(space_hash);
    }
  }
}

}  // namespace
