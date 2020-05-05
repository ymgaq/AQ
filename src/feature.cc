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

#include "./feature.h"
#include "./board.h"

void Feature::Update(const Board& b) {
  // 1. Initializes features.
  std::fill(ladder_esc_.begin(), ladder_esc_.end(), float{0.0});
  std::fill(sensibleness_.begin(), sensibleness_.end(), float{0.0});
  for (int i = 0; i < kFeatureSize; ++i) {
    std::fill(liberty_[i].begin(), liberty_[i].end(), float{0.0});
    std::fill(cap_size_[i].begin(), cap_size_[i].end(), float{0.0});
    std::fill(self_atari_[i].begin(), self_atari_[i].end(), float{0.0});
    std::fill(liberty_after_[i].begin(), liberty_after_[i].end(), float{0.0});
  }
  Color c_us = b.side_to_move();

  for (RawVertex rv = kRvtZero; rv < kNumRvts; ++rv) {
    Vertex v = rv2v(rv);

    if (b.color_at(v) != kEmpty) {
      liberty_[std::min(kFeatureSize - 1, b.sg_num_liberties_at(v) - 1)][rv] =
          1.0;
    } else if (b.IsLegal(v)) {
      // 2. Updates sensibleness.
      if (!b.IsEyeShape(v) && !b.IsSeki(v)) sensibleness_[rv] = 1.0;

      // 3. Checks sg_id of surrounding stone groups.
      std::vector<int> our_sg_ids;

      Bitboard libs;
      for (Direction d = kDirZero; d < kNumDir4; ++d) {
        Vertex v_nbr = v + dir2v(d);
        if (b.color_at(v_nbr) == kEmpty) {
          libs.Add(v_nbr);
        } else if (b.color_at(v_nbr) == c_us) {
          our_sg_ids.push_back(b.sg_id(v_nbr));
        }
      }
      sort(our_sg_ids.begin(), our_sg_ids.end());
      our_sg_ids.erase(unique(our_sg_ids.begin(), our_sg_ids.end()),
                       our_sg_ids.end());

      // 4. Counts size and liberty of the neighboring groups.
      int num_captured = 0;
      int num_our_stones = 1;
      std::vector<int> checked_ids;

      for (Direction d = kDirZero; d < kNumDir4; ++d) {
        Vertex v_nbr = v + dir2v(d);

        if (b.color_at(v_nbr) == ~c_us) {  // 4-1. Opponent's stone.
          // Adds to num_captured if it is in Atari and not yet checked.
          if (b.sg_atari_at(v_nbr) &&
              find(checked_ids.begin(), checked_ids.end(), b.sg_id(v_nbr)) ==
                  checked_ids.end()) {
            checked_ids.push_back(b.sg_id(v_nbr));
            libs.Add(v_nbr);
            num_captured += b.sg_size_at(v_nbr);

            Vertex v_tmp = v_nbr;
            do {
              for (Direction d2 = kDirZero; d2 < kNumDir4; ++d2) {
                Vertex v_tmp_nbr = v_tmp + dir2v(d2);
                if (b.color_at(v_tmp_nbr) == c_us &&
                    find(our_sg_ids.begin(), our_sg_ids.end(),
                         b.sg_id(v_tmp_nbr)) != our_sg_ids.end())
                  libs.Add(v_tmp);
              }
              v_tmp = b.next_v(v_tmp);
            } while (v_tmp != v_nbr);
          }
        } else if (b.color_at(v_nbr) == c_us) {  // 4-2. Player's stone.
          // Adds to num_our_stones if it is not yet checked.
          if (find(checked_ids.begin(), checked_ids.end(), b.sg_id(v_nbr)) ==
              checked_ids.end()) {
            checked_ids.push_back(b.sg_id(v_nbr));
            num_our_stones += b.sg_size_at(v_nbr);
            libs.Merge(b.sg_liberties_at(v_nbr));
          }
        }
      }

      // 5. Updates capture size.
      if (num_captured != 0)
        cap_size_[std::min(kFeatureSize - 1, num_captured - 1)][rv] = 1.0;

      libs.Remove(v);
      int num_liberties = libs.num_bits();

      // 6. Updates self-atari size.
      if (num_liberties == 1)
        self_atari_[std::min(kFeatureSize - 1, num_our_stones - 1)][rv] = 1.0;
      // 7. Updates liberties after the move.
      liberty_after_[std::min(kFeatureSize - 1, num_liberties - 1)][rv] = 1.0;
    }
  }

  // 8. Updates vertices escaping from ladder.
  constexpr int num_escapes = kBSize == 9 ? 3 : 4;
  Board b_cpy = b;
  auto escape_vertices = b_cpy.LadderEscapes(num_escapes);
  for (auto& v_esc : escape_vertices) ladder_esc_[v2rv(v_esc)] = 1.0;
}

float* Feature::Copy(float* oi, bool use_full, int symmetry_idx) const {
  auto copy_n_symmetry =
      [symmetry_idx](std::vector<float>::const_iterator input, float* output) {
        if (symmetry_idx == 0) {
          output = std::copy_n(input, kNumRvts, output);
        } else {
          for (int j = 0; j < kNumRvts; ++j) {
            *output = *(input + rv2sym(j, symmetry_idx));
            ++output;
          }
        }
        return output;
      };

  for (int i = 0; i < kNumHistory; ++i)
    oi = copy_n_symmetry(stones_[next_side_][i].begin(), oi);
  for (int i = 0; i < kNumHistory; ++i)
    oi = copy_n_symmetry(stones_[~next_side_][i].begin(), oi);

  if (next_side_ == kWhite) {
    oi = std::fill_n(oi, kNumRvts, float{0.0});
    oi = std::fill_n(oi, kNumRvts, float{1.0});
  } else {
    oi = std::fill_n(oi, kNumRvts, float{1.0});
    oi = std::fill_n(oi, kNumRvts, float{0.0});
  }

  if (use_full) {
    for (int i = 0; i < kFeatureSize; ++i)
      oi = copy_n_symmetry(liberty_[i].begin(), oi);
    for (int i = 0; i < kFeatureSize; ++i)
      oi = copy_n_symmetry(cap_size_[i].begin(), oi);
    for (int i = 0; i < kFeatureSize; ++i)
      oi = copy_n_symmetry(self_atari_[i].begin(), oi);
    for (int i = 0; i < kFeatureSize; ++i)
      oi = copy_n_symmetry(liberty_after_[i].begin(), oi);
    oi = copy_n_symmetry(ladder_esc_.begin(), oi);
    oi = copy_n_symmetry(sensibleness_.begin(), oi);
  }

  return oi;
}
