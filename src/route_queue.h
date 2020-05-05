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

#ifndef ROUTE_QUEUE_H_
#define ROUTE_QUEUE_H_

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "./eval_cache.h"
#include "./node.h"

/**
 * @enum LeafType
 * States at the end of a search.
 */
enum LeafType { kEvaluated, kWaitEval, kFailToPush, kReachEnd, kLeafNone };

/**
 * @struct SearchRoute
 * Record of the paths taken in the search tree.
 */
struct SearchRoute {
  int tree_id;
  int depth;
  int num_requests;
  LeafType leaf;
  std::vector<Vertex> moves;
  std::vector<int> child_ids;

  SearchRoute() : depth(0), num_requests(1), tree_id(0), leaf(kLeafNone) {}

  SearchRoute(const SearchRoute& rhs)
      : depth(rhs.depth),
        num_requests(rhs.num_requests),
        tree_id(rhs.tree_id),
        leaf(rhs.leaf),
        moves(rhs.moves),
        child_ids(rhs.child_ids) {}

  bool operator==(const SearchRoute& rhs) const {
    return depth == rhs.depth && tree_id == rhs.tree_id && moves == rhs.moves &&
           child_ids == rhs.child_ids;
  }

  void Add(Vertex v, int child_id) {
    moves.push_back(v);
    child_ids.push_back(child_id);
    ++depth;
  }
};

/**
 * @struct RouteEntry
 * Structure that holds the path and terminal node information (pnd, ft, vp,
 * key) of the search.
 */
struct RouteEntry {
  std::vector<SearchRoute> routes;
  std::unique_ptr<Node> pnd;
  Feature ft;
  ValueAndProb vp;
  Key key;

  RouteEntry() : key(UINT64_MAX) {}

  RouteEntry(const Board& b, const SearchRoute& sr)
      : ft(b.get_feature()), key(b.key()) {
    pnd = std::move(std::unique_ptr<Node>(new Node(b)));
    routes.push_back(sr);
  }

  bool has_node_ptr() const { return static_cast<bool>(pnd); }

  void AddRoute(const SearchRoute& sr) {
    auto itr = find(routes.begin(), routes.end(), sr);
    if (itr != routes.end())
      itr->num_requests++;
    else
      routes.push_back(sr);
  }
};

/**
 * @class
 * Queue class that exclusively stores a RouteEntry.
 * Used for managing common node information while searching in multiple search
 * trees during training.
 * See ParallelManager::Search() and ParallelManager::BackUpNodes().
 */
class RouteQueue {
 public:
  RouteQueue() {}

  void clear() { entries_.clear(); }

  int size() const { return entries_.size(); }

  void push(const Board& b, const SearchRoute& route) {
    Key key = b.key();

    std::lock_guard<std::mutex> lock(mx_);
    auto itr =
        find_if(entries_.begin(), entries_.end(), [key](const RouteEntry& e) {
          return e.key == key && e.has_node_ptr();
        });

    if (itr != entries_.end()) {
      itr->AddRoute(route);
    } else {
      entries_.emplace_back(b, route);
    }
  }

  std::vector<RouteEntry>* get_entries() { return &entries_; }

 private:
  std::mutex mx_;
  std::vector<RouteEntry> entries_;
};

#endif  // ROUTE_QUEUE_H_
