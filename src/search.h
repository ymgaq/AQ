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

#ifndef SEARCH_H_
#define SEARCH_H_

#include <stdarg.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "./board.h"
#include "./eval_cache.h"
#include "./eval_worker.h"
#include "./network.h"
#include "./node.h"
#include "./option.h"
#include "./timer.h"

/**
 * @class SearchTree
 * SearchTree class manages search trees and performs parallel search and output
 * search information.
 * Need to call SetGPUAndMemory() to allocate cache memory and initialize the
 * GPU inference engine before you can search.
 */
class SearchTree : public RootNode, public Timer, public SearchParameter {
 public:
  // Constructor. GPU and node table are not yet set.
  SearchTree() : RootNode(), Timer(), SearchParameter() { Init(); }

  // Treats it the same as the default constructor, since the copy constructor
  // is called in std::vector::resize() of C++11.
  SearchTree(const SearchTree& rhs) : RootNode(), Timer(), SearchParameter() {
    Init();
  }

  double lambda() const { return lambda_; }

  double num_virtual_loss() const { return virtual_loss_; }

  double ladder_reduction() const { return ladder_reduction_; }

  double komi() const { return komi_; }

  int num_reach_ends() const { return num_reach_ends_; }

  bool reflesh_root() const { return reflesh_root_; }

  bool consider_pass() const { return consider_pass_; }

  std::ofstream* log_file() const { return log_file_.get(); }

  bool has_eval_worker() const { return static_cast<bool>(eval_worker_); }

  Node* root_node() const { return RootNode::node(); }

  void set_komi(double val) { komi_ = val; }

  void set_num_reach_ends(int val) { num_reach_ends_.store(val); }

  void UpdateLambda(int ply) {
    lambda_ =
        lambda_init_ -
        lambda_delta_ *
            std::min(
                1.0,
                std::max(0.0, static_cast<double>(ply - lambda_move_start_) /
                                  (lambda_move_end_ - lambda_move_start_)));
  }

  void StopToThink() { stop_think_.store(true); }

  void PrepareToThink() { stop_think_.store(false); }

  void SetLogFile(std::string log_path) {
    if (log_file_) {
      log_file_->close();
      log_file_.reset();
    }
    log_file_ = std::move(std::unique_ptr<std::ofstream>(
        new std::ofstream(log_path, std::ofstream::out)));
  }

  void PrintBoardLog(const Board& b) {
    if (Options["save_log"]) std::cerr << b;
    if (log_file_) *(log_file_.get()) << b;
  }

  void InitEvalCache() { eval_cache_.Init(); }

  void ReplaceModel(std::vector<int> gpu_ids, std::string model_path = "") {
    eval_worker_->ReplaceModel(gpu_ids, model_path);
  }

  void InitEvalWorker(std::vector<int> list_gpus, std::string model_path = "") {
    if (list_gpus.empty()) {
      for (int i = 0; i < num_gpus_; ++i) {
        list_gpus.push_back(i);
      }
    }

    eval_worker_ = std::move(std::unique_ptr<EvalWorker>(new EvalWorker()));
    eval_worker_->Init(list_gpus, model_path);
    eval_cache_.Resize(300000);

    if (Options["rule"].get_int() == kJapanese &&
        Options["validate_model_path"].get_string() != "") {
      std::lock_guard<std::mutex>(*(eval_worker_->get_mutex()));
      std::string validate_model_path =
          Options["validate_model_path"].get_string();
      if (validate_model_path == "default") {
        validate_model_path =
            JoinPath(Options["working_dir"], "engine", "model_cn.engine");
      }
      validate_engine_ = std::move(std::unique_ptr<TensorEngine>(
          new TensorEngine(list_gpus[0], Options["batch_size"].get_int())));
      validate_engine_->Init(validate_model_path);
    }

    stop_think_ = false;
  }

  void SetGPUAndMemory() {
    std::vector<int> list_gpus;
    for (int i = 0; i < num_gpus_; ++i) list_gpus.push_back(i);

    InitEvalWorker(list_gpus);
    RootNode::Resize(Options["node_size"].get_int());
  }

  void Init() {
    num_gpus_ = Options["num_gpus"].get_int();
    num_threads_ = Options["num_threads"].get_int();
    komi_ = Options["komi"].get_double();
    use_dirichlet_noise_ = Options["use_dirichlet_noise"].get_bool();
    reflesh_root_ = false;  // (bool)Options["use_dirichlet_noise"];
    consider_pass_ = Options["rule"].get_int() == kJapanese;
    log_file_.reset();
    stop_think_ = false;
    InitRoot();
  }

  void InitRoot() {
    lambda_ = lambda_init_;
    num_evaluated_ = 0;
    num_reach_ends_ = 0;
    RootNode::Init();
    Timer::Init();
  }

  void UpdateNodeVP(Node* nd, const ValueAndProb& vp);

  /**
   * Creates node in node table from board or another node.
   */
  void CreateNode(Node* nd, const Board& b, const ValueAndProb& vp) {
    *nd = b;
    UpdateNodeVP(nd, vp);
    increment_entries();
  }

  /**
   * Sets a pointer to the next node to a child node.
   */
  void SetNextNode(Node* nd, int child_id, std::unique_ptr<Node>* pnd,
                   const ValueAndProb& vp) {
    UpdateNodeVP(pnd->get(), vp);
    {
      std::lock_guard<std::mutex> lock(nd->mutex());
      if (!nd->children[child_id].has_next())
        nd->children[child_id].set_next_ptr(pnd);
    }
    increment_entries();
  }

  /**
   * Adds Dirichlet noise to a node.
   */
  void AddDirichletNoise(Node* nd) {
    std::vector<double> dirichlet_list;
    double sum_noises = 0.0;
    int imax = nd->num_children();
    for (int i = 0; i < imax; ++i) {
      double noise = RandNoise();
      sum_noises += noise;
      dirichlet_list.emplace_back(noise);
    }

    ChildNode* child;
    double sum_probs = 0.0;
    for (int i = 0; i < imax; ++i) {
      child = &nd->children[i];
      child->set_prob(0.75 * child->prob() +
                      0.25 * dirichlet_list[i] / sum_noises);
      sum_probs += child->prob();
      if (reflesh_root_) child->InitValueStat();
    }

    double inv_sum = sum_probs > 0 ? 1.0 / sum_probs : 1.0;
    for (int i = 0; i < imax; ++i)
      nd->children[i].set_prob(nd->children[i].prob() * inv_sum);
    if (reflesh_root_) nd->set_num_total_values(1);
  }

  /**
   * Updates the root node.
   */
  void UpdateRoot(const Board& b, TensorEngine* engine = nullptr) {
    Vertex v = b.move_before();
    bool has_child = RootNode::ShiftRootNode(v, b);
    if (!has_child) {
      ValueAndProb vp;
      Feature ft(b.get_feature());

      if (engine) {
        engine->Infer(ft, &vp);
      } else {
        eval_worker_->Evaluate(ft, &vp);
      }

      CreateNode(root_node(), b, vp);
    }

    if (use_dirichlet_noise_) AddDirichletNoise(root_node());
  }

  /**
   * The search tree is searched once to the end according to the Q value, and
   * evaluated or rolled out.
   *
   *  NNSearch == true : Evaluation with GPU
   *  NNSearch == false: Rollout
   */
  template <bool NNSearch>
  double SearchBranch(Node* nd, Board* b, SearchRoute* route,
                      RouteQueue* eq = nullptr, EvalCache* cache = nullptr);

  /**
   * Writes out text to the log file.
   * Outputs to standard error output when save_log mode.
   */
  void PrintLog(const char* output_text, ...) {
    va_list args;
    char buf[1024];

    va_start(args, output_text);
    vsprintf(buf, output_text, args);
    va_end(args);

    if (Options["save_log"]) std::cerr << buf;
    if (log_file_) *(log_file_.get()) << buf;
  }

  /**
   * Returns the maximum depth of the search.
   * Considering repetition, the seach is closed at 128 moves.
   */
  int MaxDepth(const Node& nd, Vertex prev_move, int depth) const {
    if (nd.num_children() == 0 || depth >= 128) return depth;

    ChildNode* best_child = SortChildren(nd).front();
    int max_depth = depth;

    if (best_child->has_next()) {
      if (prev_move == kPass && best_child->move() == kPass) return max_depth;
      max_depth = std::max(max_depth, MaxDepth(*best_child->next_ptr(),
                                               best_child->move(), depth + 1));
    }

    return max_depth;
  }

  /**
   * Sorts the child nodes by the number of visits.
   */
  std::vector<ChildNode*> SortChildren(const Node& nd) const {
    std::vector<ChildNode*> sorted;
    if (nd.num_children() > 0) {
      for (auto& ch : nd.children)
        sorted.emplace_back(const_cast<ChildNode*>(&ch));
      std::stable_sort(sorted.begin(), sorted.end(),
                       [](const ChildNode* lhs, const ChildNode* rhs) {
                         if (lhs->num_values() == rhs->num_values())
                           return lhs->prob() > rhs->prob();
                         return lhs->num_values() > rhs->num_values();
                       });
    }

    return std::move(sorted);
  }

  /**
   * Returns winning rate of child node scaled to [0, 1].
   */
  double WinningRate(const ChildNode& child) const {
    double winning_rate = child.num_values() == 0
                              ? child.rollout_rate()
                              : child.num_rollouts() == 0
                                    ? child.value_rate()
                                    : child.winning_rate(lambda_);

    return (winning_rate + 1) / 2;
  }

  /**
   * Converts a vertex to GTP-style string. e.g. (1,1) -> A1
   */
  std::string v2str(Vertex v) const {
    return v == kPass
               ? "PASS"
               : v > kPass ? "NULL"
                           : std::string("ABCDEFGHJKLMNOPQRST")[x_of(v) - 1] +
                                 std::to_string(y_of(v));
  }

  /**
   * Returns elapsed time from t0 in seconds.
   */
  double ElapsedTime(const std::chrono::system_clock::time_point& t0) {
    auto t1 = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count() /
           1000.0;
  }

  /**
   * Performs a search with a time limit and return the best move and winning
   * rate.
   */
  Vertex Search(const Board& b, double time_limit, double* winning_rate,
                bool is_errout, bool ponder, int lizzie_interval = -1);

  /**
   * Repeats searching with a single thread.
   */
  void EvaluateWorker(const Board& b, double time_limit, bool ponder,
                      int th_id);

  /**
   * Rollouts in a single thread.
   */
  void RolloutWorker(const Board& b) {
    Board b_;
    while (!stop_think_) {
      b_ = b;
      SearchRoute route;
      SearchBranch<false>(root_node(), &b_, &route);
    }
  }

  /**
   * Assigns rollout and evaluation workers to multiple threads.
   */
  void AllocateThreads(const Board& b, double time_limit, bool ponder,
                       int lizzie_interval = -1) {
    Node* nd = root_node();
    if (nd->num_children() <= 1) {
      stop_think_ = true;
      return;
    }

    int num_rollout_threads = 1;
    int num_evaluate_threads = batch_size_ * num_gpus_ * 2;
    num_evaluate_threads = std::min(num_threads_, num_evaluate_threads);
    num_rollout_threads =
        std::max(num_rollout_threads, num_threads_ - num_evaluate_threads);
    int num_total_threads = num_evaluate_threads + num_rollout_threads;

    std::vector<std::thread> ths;
    for (int i = 0; i < num_total_threads; ++i) {
      if (i < num_evaluate_threads)
        ths.push_back(std::thread(&SearchTree::EvaluateWorker, this, b,
                                  time_limit, ponder, i));
      else
        ths.push_back(std::thread(&SearchTree::RolloutWorker, this, b));
    }

    if (ponder && lizzie_interval > 0) {
      do {
        LizzieInfo(nd, std::cout);
        std::this_thread::sleep_for(std::chrono::milliseconds(lizzie_interval));
      } while (!stop_think_);
    }

    for (std::thread& th : ths) th.join();
  }

  /**
   * Returns final score.
   */
  double FinalScore(const Board& b, Vertex next_move, int num_policy_moves,
                    int num_playouts, Board::OwnerMap* owner,
                    TensorEngine* engine = nullptr, EvalCache* cache = nullptr);

  /**
   * Returns whether or not a pass should be made.
   * If it should not pass and there is a suitable move other than next_move,
   * returns it.
   */
  Vertex ShouldPass(const Board& b, Vertex next_move, int num_policy_moves,
                    int num_playouts, TensorEngine* engine = nullptr,
                    EvalCache* cache = nullptr);

  /**
   * Returns string of best sequence.
   *  e.g. D4 ->D16->Q16->Q4 ->...
   */
  std::string PV(const Node* nd, Vertex head_move, int max_move = 6) const;

  /**
   * Outputs information on candidate moves.
   * The actual move is shown at the top.
   */
  void PrintCandidates(const Node* nd, int next_move, std::ostream& ost,
                       bool flip_value = false) const;

  /**
   * Outputs information on candidate moves for Lizzie.
   */
  void LizzieInfo(const Node* nd, std::ostream& ost) const;

 private:
  double lambda_;
  int num_threads_;
  int num_gpus_;
  double komi_;
  bool use_dirichlet_noise_;
  bool reflesh_root_;
  bool consider_pass_;
  std::atomic<bool> stop_think_;
  std::atomic<int> num_evaluated_;
  std::atomic<int> num_reach_ends_;

  EvalCache validate_cache_;
  EvalCache eval_cache_;

  std::unique_ptr<std::ofstream> log_file_;
  std::unique_ptr<TensorEngine> validate_engine_;
  std::unique_ptr<EvalWorker> eval_worker_;
};

#endif  // SEARCH_H_
