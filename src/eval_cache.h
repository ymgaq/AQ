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

#ifndef EVAL_CACHE_H_
#define EVAL_CACHE_H_

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "./node.h"

/**
 * @struct ValueAndProb
 * Structure that holds value and policy.
 */
struct ValueAndProb {
  double value;
  std::array<float, kNumRvts> prob;  // Doesn't include kPass.

  // Constructor
  ValueAndProb() : value(0.0), prob{0.0} {}

  ValueAndProb(const ValueAndProb& rhs) : value(rhs.value), prob(rhs.prob) {}

  ValueAndProb& operator=(const ValueAndProb& rhs) {
    value = rhs.value;
    prob = rhs.prob;
    return *this;
  }
};

/**
 * @struct SyncedEntry
 * Feature and ValueAndProb structures for synchronization in evaluation.
 */
struct SyncedEntry {
  std::mutex mx;
  std::condition_variable cv;
  Feature ft;
  ValueAndProb vp;

  // Constructor
  explicit SyncedEntry(const Feature& ft_) : ft(ft_) {}
};

/**
 * @class EvalCache
 * EvalCache class exclusively manages caches of ValueAndProb.
 *
 * @code
 *  ValueAndProb vp;
 *  bool found = eval_cache.Probe(b, &vp);
 *  if(!found) {
 *    engine.Infer(b.get_feature(), &vp);
 *    eval_cache.Insert(b.key(), vp);
 *  }
 * @endcode
 */
class EvalCache {
 public:
  // Constructor
  explicit EvalCache(int size = 10000) { max_size_ = size; }

  void Resize(int size) {
    max_size_ = size;
    while (order_.size() > max_size_) {
      vp_map_.erase(order_.front());
      order_.pop_front();
    }
  }

  void Init() {
    vp_map_.clear();
    order_.clear();
  }

  bool Probe(const Board& b, ValueAndProb* vp, bool check_sym = true) {
    Key key = b.key();
    bool found = false;

    std::lock_guard<std::mutex> lk(mx_);

    if (check_sym && b.game_ply() < kNumRvts / 12) {
      for (int i = 0; i < 8; ++i) {
        Key sym_hash = b.key(i);
        auto itr = vp_map_.find(sym_hash);
        found = (itr != vp_map_.end());
        if (found) {
          if (i == 0) {
            *vp = *itr->second;
          } else {
            vp->value = itr->second->value;
            for (int j = 0; j < kNumRvts; ++j)
              vp->prob[rv2sym(j, i)] = itr->second->prob[j];

            AddCache(key, *vp);
          }
          return true;
        }
      }

      return false;
    } else {
      auto itr = vp_map_.find(key);
      if (itr == vp_map_.end()) return false;
      *vp = *itr->second;
    }

    return true;
  }

  bool Probe(Key key, ValueAndProb* vp) {
    std::lock_guard<std::mutex> lk(mx_);
    auto itr = vp_map_.find(key);
    if (itr == vp_map_.end()) return false;
    *vp = *itr->second;
    return true;
  }

  void Insert(Key key, const ValueAndProb& vp) {
    std::lock_guard<std::mutex> lk(mx_);
    if (vp_map_.find(key) == vp_map_.end()) AddCache(key, vp);
  }

 private:
  std::mutex mx_;
  size_t max_size_;
  std::unordered_map<Key, std::unique_ptr<const ValueAndProb>> vp_map_;
  std::deque<Key> order_;

  void AddCache(Key key, const ValueAndProb& vp) {
    vp_map_.emplace(key,
                    std::unique_ptr<const ValueAndProb>(new ValueAndProb(vp)));
    order_.push_back(key);

    if (order_.size() > max_size_) {
      vp_map_.erase(order_.front());
      order_.pop_front();
    }
  }
};

#endif  // EVAL_CACHE_H_
