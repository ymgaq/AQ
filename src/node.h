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

#ifndef NODE_H_
#define NODE_H_

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "./board.h"

/**
 * Function to perform arithmetic addition for atomic<float>, atomic<double>,
 * and other atomic variables for which there is no += operator available.
 */
template <typename T>
inline T FetchAdd(std::atomic<T>* obj, T arg) {
  T expected = obj->load();
  while (!atomic_compare_exchange_weak(obj, &expected, expected + arg)) {
  }
  return expected;
}

// --------------------
//      RateStat
// --------------------

/**
 * @class RateStat
 * Base class that holds the number of visits to a node and the cumulative value
 * of the results, which is inherited by the ChildNode and Node class.
 */
class RateStat {
 public:
  RateStat() {
    num_rollouts_.store(0, std::memory_order_relaxed);
    num_values_.store(0, std::memory_order_relaxed);
    win_rollouts_.store(0.0, std::memory_order_relaxed);
    win_values_.store(0.0, std::memory_order_relaxed);
  }
  RateStat(const RateStat& rhs) { *this = rhs; }

  void Init() {
    num_rollouts_.store(0, std::memory_order_relaxed);
    num_values_.store(0, std::memory_order_relaxed);
    win_rollouts_.store(0.0, std::memory_order_relaxed);
    win_values_.store(0.0, std::memory_order_relaxed);
  }

  RateStat& operator=(const RateStat& rhs) {
    num_rollouts_.store(rhs.num_rollouts_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
    num_values_.store(rhs.num_values_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    win_rollouts_.store(rhs.win_rollouts_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
    win_values_.store(rhs.win_values_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);

    return *this;
  }

  RateStat& operator+=(const RateStat& rhs) {
    num_rollouts_ += rhs.num_rollouts_;
    num_values_ += rhs.num_values_;
    FetchAdd(&win_rollouts_, rhs.win_rollouts_.load());
    FetchAdd(&win_values_, rhs.win_values_.load());

    return *this;
  }

  int num_rollouts() const { return num_rollouts_.load(); }
  int num_values() const { return num_values_.load(); }
  double win_rollouts() const { return win_rollouts_.load(); }
  double win_values() const { return win_values_.load(); }

  double rollout_rate() const {
    int rc = num_rollouts_.load();
    return rc == 0 ? 0.0 : win_rollouts_.load() / rc;
  }

  double value_rate() const {
    int vc = num_values_.load();
    return vc == 0 ? 0.0 : win_values_.load() / vc;
  }

  double winning_rate(double lambda_) const {
    return (1 - lambda_) * rollout_rate() + lambda_ * value_rate();
  }

  void InitValueStat() {
    num_values_.store(0, std::memory_order_relaxed);
    win_values_.store(0.0, std::memory_order_relaxed);
  }

  void AddFlipedStat(const RateStat& rs) {
    num_rollouts_ += rs.num_rollouts_;
    num_values_ += rs.num_values_;
    FetchAdd(&win_rollouts_, rs.win_rollouts_.load());
    FetchAdd(&win_values_, rs.win_values_.load());
  }

  void AddValueOnce(float win) {
    num_values_ += 1;
    FetchAdd(&win_values_, win);
  }

 protected:
  std::atomic<int> num_rollouts_;     // The number of rollout execution.
  std::atomic<int> num_values_;       // The number of board evaluation.
  std::atomic<float> win_rollouts_;  // Sum of rollout results.
  std::atomic<float> win_values_;    // Sum of evaluation values.
};

// --------------------
//      ChildNode
// --------------------

class Node;

/**
 * @enum CreateState
 * Creation status of ChildNode.
 */
enum CreateState : uint8_t {
  kInitial = 0,
  kCreating,
  kComplete,
};

/**
 * @class ChildNode
 * Class to store moves and probabilities on child nodes and pointers to the
 * next node. ChildNodes are expanded as Node::children when a Node is created.
 */
class ChildNode : public RateStat {
 public:
  ChildNode() {
    move_.store(kPass, std::memory_order_relaxed);
    prob_.store(0.0, std::memory_order_relaxed);
    next_ptr_.reset();
    create_state_.store(kInitial, std::memory_order_relaxed);
  }

  ChildNode(const ChildNode& rhs) : RateStat(rhs) {
    move_.store(rhs.move_.load());
    prob_.store(rhs.prob_.load());
    // next_ptr_ = std::move(rhs.next_ptr_);
    next_ptr_.reset();
    create_state_.store(rhs.create_state_.load());
  }

  ChildNode(ChildNode&& rhs) : RateStat(rhs) {
    move_.store(rhs.move_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
    prob_.store(rhs.prob_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
    next_ptr_ = std::move(rhs.next_ptr_);
    create_state_.store(rhs.create_state_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
  }

  ChildNode(Vertex v, float p) {
    move_.store(v, std::memory_order_relaxed);
    prob_.store(p, std::memory_order_relaxed);
    next_ptr_.reset();
    create_state_.store(kInitial, std::memory_order_relaxed);
  }

  ~ChildNode() { next_ptr_.reset(); }

  bool operator<(const ChildNode& rhs) const {
    return prob_.load() < rhs.prob_.load();
  }

  bool operator>(const ChildNode& rhs) const {
    return prob_.load() > rhs.prob_.load();
  }

  Vertex move() const { return move_.load(); }

  float prob() const { return prob_.load(); }

  Node* next_ptr() const { return next_ptr_.get(); }

  int num_entries() const;

  bool has_next() const { return static_cast<bool>(next_ptr_); }

  void set_move(Vertex v) { move_.store(v); }

  void set_prob(float val) { prob_.store(val); }

  void set_next_ptr(std::unique_ptr<Node>* pnd_) {
    next_ptr_ = std::move(*pnd_);
  }

  bool SetCreatingState() {
    uint8_t expected = kInitial;
    uint8_t desired = kCreating;
    return create_state_.compare_exchange_strong(expected, desired);
  }

  void SetCompleteState() { create_state_.exchange(kComplete); }

  void WaitForComplete() {
    while (create_state_.load() == kCreating) {
    }
  }

  friend class Node;
  friend class RootNode;

 private:
  std::atomic<Vertex> move_;        // Move to the child board.
  std::atomic<float> prob_;         // Probability of the move.
  std::unique_ptr<Node> next_ptr_;  // Pointer of the next node.
  std::atomic<uint8_t> create_state_;
};

// --------------------
//        Node
// --------------------

/**
 * @class Node
 * This node represents a board state, initialized in the Board class, and
 * expands the legal moves to an array of child nodes. It is necessary to update
 * the results of GPU evaluation to value_ and children prob_.
 */
class Node : public RateStat {
 public:
  std::vector<ChildNode> children;

  // Constructor
  Node()
      : ply_(0),
        num_total_values_(1),
        num_total_rollouts_(1),
        value_(0.0),
        key_(UINT64_MAX),
        num_entries_(1) {}

  Node(const Node& rhs) : RateStat(rhs) {
    ply_.store(rhs.ply_.load());
    num_total_values_.store(rhs.num_total_values_.load());
    num_total_rollouts_.store(rhs.num_total_rollouts_.load());
    value_.store(rhs.value_.load());
    key_.store(rhs.key_.load());
    children.clear();
    for (auto& ch : rhs.children) children.emplace_back(ChildNode(ch));
    num_entries_.store(rhs.num_entries_.load());
  }

  explicit Node(const Board& b) { *this = b; }

  ~Node() { children.clear(); }

  /**
   * Generates node from board.
   */
  Node& operator=(const Board& b) {
    ply_ = b.game_ply();
    num_total_values_ = 1;
    num_total_rollouts_ = 1;
    key_ = b.key();
    children.clear();
    RateStat::Init();
    value_ = 0.0;

    Board b_cpy = b;
    constexpr int num_escapes = kBSize == 9 ? 3 : 4;
    std::vector<Vertex> esc_list = b_cpy.LadderEscapes(num_escapes);

    std::vector<Vertex> legals;

    for (Vertex v : b.empties()) {
      bool is_sensible =
          (b.IsLegal(v) && !b.IsEyeShape(b.side_to_move(), v, true) &&
           !b.IsSeki(v));
      if (!is_sensible || b.CheckRepetition(v) == kRepetitionLose) continue;
      legals.push_back(v);
    }
    legals.push_back(kPass);

    int num_childern = legals.size();
    children.resize(num_childern);
    for (int i = 0; i < num_childern; ++i) {
      Vertex v = legals[i];
      children[i].set_move(v);
      if (!esc_list.empty() &&
          std::find(esc_list.begin(), esc_list.end(), v) != esc_list.end())
        children[i].set_prob(-0.1);
    }

    num_entries_ = 1;

    return *this;
  }

  int num_children() const { return children.size(); }

  int game_ply() const { return ply_.load(); }

  int num_total_values() const { return num_total_values_.load(); }

  int num_total_rollouts() const { return num_total_rollouts_.load(); }

  float value() const { return value_.load(); }

  Key key() const { return key_.load(); }

  int num_entries() const { return num_entries_.load(); }

  std::mutex& mutex() { return mx_; }

  void set_num_total_values(int val) { num_total_values_.store(val); }

  void set_num_total_rollouts(int val) { num_total_rollouts_.store(val); }

  void set_value(float val) { value_.store(val); }

  void increment_entries() { ++num_entries_; }

  template <bool NNSearch>
  void VirtualLoss(int child_id, float virtual_loss) {
    if (NNSearch) {
      FetchAdd(&(children[child_id].win_values_), -virtual_loss);
      children[child_id].num_values_ += virtual_loss;
      FetchAdd(&(win_values_), -virtual_loss);
      num_values_ += virtual_loss;
      num_total_values_ += virtual_loss;
    } else {
      FetchAdd(&(children[child_id].win_rollouts_), -virtual_loss);
      children[child_id].num_rollouts_ += virtual_loss;
      FetchAdd(&(win_rollouts_), -virtual_loss);
      num_rollouts_ += virtual_loss;
      num_total_rollouts_ += virtual_loss;
    }
  }

  template <bool NNSearch>
  void VirtualWin(int child_id, float virtual_loss, int num_requests,
                  float win = 0.0) {
    if (NNSearch) {
      FetchAdd(&(children[child_id].win_values_),
               (win + virtual_loss) * num_requests);
      children[child_id].num_values_ -= (virtual_loss - 1) * num_requests;
      FetchAdd(&(win_values_), (win + virtual_loss) * num_requests);
      num_values_ -= (virtual_loss - 1) * num_requests;
      num_total_values_ -= (virtual_loss - 1) * num_requests;
    } else {
      FetchAdd(&(children[child_id].win_rollouts_),
               (win + virtual_loss) * num_requests);
      children[child_id].num_rollouts_ -= (virtual_loss - 1) * num_requests;
      FetchAdd(&(win_rollouts_), (win + virtual_loss) * num_requests);
      num_rollouts_ -= (virtual_loss - 1) * num_requests;
      num_total_rollouts_ -= (virtual_loss - 1) * num_requests;
    }
  }

 private:
  std::atomic<int> ply_;  // Number of moves in the game.
  // Sum of evaluation visits of all child nodes.
  std::atomic<int> num_total_values_;
  // Sum of rollout visits of all child nodes.
  std::atomic<int> num_total_rollouts_;
  std::atomic<float> value_;     // Evaluated value of the child board.
  std::atomic<Key> key_;          // Board hash of the node.
  std::atomic<int> num_entries_;  // Total node number under this node.
  std::mutex mx_;                 // Mutex for lock of this node.
};

inline int ChildNode::num_entries() const {
  return has_next() ? next_ptr_->num_entries() : 0;
}

// --------------------
//      RootNode
// --------------------

/**
 * @class RootNode
 * The RootNode class holds a pointer to the root node of the search tree. When
 * the board is advanced, it transitions to the corresponding child node.
 */
class RootNode {
 public:
  RootNode() : max_num_entries_(0), num_entries_(0), pnd_(nullptr) {}

  RootNode(const RootNode& rhs) = delete;

  int num_entries() const { return num_entries_.load(); }

  double entry_rate() const {
    return max_num_entries_ == 0 ? 0.0 : num_entries_.load() / max_num_entries_;
  }

  Node* node() const { return pnd_.get(); }

  void increment_entries() { ++num_entries_; }

  void set_node(std::unique_ptr<Node>* pnd) { pnd_ = std::move(*pnd); }

  void Init() {
    num_entries_ = 0;
    pnd_.reset();
  }

  void Resize(int max_size) {
    max_num_entries_ = max_size;
    num_entries_ = 0;
    pnd_.reset();
  }

  bool ShiftRootNode(Vertex v, const Board& b, bool create_if_not_found = true);

 private:
  int max_num_entries_;
  std::atomic<int> num_entries_;
  std::unique_ptr<Node> pnd_;
};

inline bool RootNode::ShiftRootNode(Vertex v, const Board& b,
                                    bool create_if_not_found) {
  // Already updated.
  if (static_cast<bool>(pnd_) && pnd_->game_ply() == b.game_ply() &&
      pnd_->key() == b.key())
    return true;

  bool found_next = false;

  if (static_cast<bool>(pnd_) && pnd_->game_ply() + 1 == b.game_ply()) {
    for (int i = 0, n = pnd_->num_children(); i < n; ++i) {
      ChildNode* cn = &pnd_->children[i];
      if (cn->move() == v && cn->has_next()) {
        found_next = true;
        num_entries_ = std::max(1, cn->num_entries());

        auto prev_pnd = std::move(pnd_);
        pnd_ = std::move(prev_pnd->children[i].next_ptr_);

        // Deletes previous pnd_ in another thread, which
        // takes 400,000 nodes per sec.
        auto p = prev_pnd.release();
        auto th = std::thread([p]() { delete p; });
        th.detach();

        break;
      }
    }
  } else if (static_cast<bool>(pnd_) && pnd_->game_ply() + 2 == b.game_ply()) {
    Vertex v_prev = b.move_before_2();

    for (size_t i = 0, n = pnd_->num_children(); i < n; ++i) {
      ChildNode* cn = &pnd_->children[i];
      if (cn->move() == v_prev && cn->has_next()) {
        Node* nnd = cn->next_ptr();
        for (size_t j = 0, n = nnd->num_children(); j < n; ++j) {
          ChildNode* gcn = &nnd->children[j];
          if (gcn->move() == v && gcn->has_next()) {
            found_next = true;
            num_entries_ = std::max(1, gcn->num_entries());

            auto prev_pnd = std::move(pnd_);
            gcn = &(prev_pnd->children[i].next_ptr()->children[j]);
            pnd_ = std::move(gcn->next_ptr_);

            // Deletes previous pnd_ in another thread, which
            // takes 400,000 nodes per sec.
            auto p = prev_pnd.release();
            auto th = std::thread([p]() { delete p; });
            th.detach();

            break;
          }
        }
      }

      if (found_next) break;
    }
  }

  if (!found_next) {
    if (static_cast<bool>(pnd_)) {
      auto prev_pnd = std::move(pnd_);
      auto p = prev_pnd.release();
      auto th = std::thread([p]() { delete p; });
      th.detach();
    }

    if (create_if_not_found) {
      pnd_ = std::move(std::unique_ptr<Node>(new Node(b)));
      num_entries_ = 1;
    } else {
      pnd_.reset();
      num_entries_ = 0;
    }
  }

  return found_next;
}

#endif  // NODE_H_
