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

#include "./search.h"

void SearchTree::UpdateNodeVP(Node* nd, const ValueAndProb& vp) {
  nd->set_value(vp.value);
  ChildNode* child;
  double sum_probs = 0.0;
  double sum_ladder_probs = 0.0;
  int num_ladder_cns = 0;
  int ladder_cns[32] = {-1};

  for (int i = 0, n = nd->num_children(); i < n; ++i) {
    child = &nd->children[i];

    if (child->move() == kPass) {
      if (consider_pass_ && nd->game_ply() >= 2 * kNumRvts / 3)
        child->set_prob(0.03);
      else
        child->set_prob(0.0);
      sum_probs += child->prob();
      continue;
    }

    if (child->prob() < 0 && ladder_reduction_ != 1.0) {
      if (num_ladder_cns < 32) {
        ladder_cns[num_ladder_cns++] = i;
        sum_ladder_probs += vp.prob[v2rv(child->move())];
      }
    } else {
      sum_probs += vp.prob[v2rv(child->move())];
    }

    float p = child->prob() < 0
                  ? vp.prob[v2rv(child->move())] * ladder_reduction_
                  : vp.prob[v2rv(child->move())];
    child->set_prob(p);
  }

  double inv_sum =
      sum_probs > 0 ? (1.0 - ladder_reduction_ * sum_ladder_probs) / sum_probs
                    : 1.0;
  for (int i = 0, n = nd->num_children(); i < n; ++i) {
    bool skip_rescale = false;
    for (int j = 0; j < num_ladder_cns; ++j)
      if (ladder_cns[j] == i) {
        skip_rescale = true;
        break;
      }

    if (!skip_rescale)
      nd->children[i].set_prob(nd->children[i].prob() * inv_sum);
  }
}

template <bool NNSearch>
double SearchTree::SearchBranch(Node* nd, Board* b, SearchRoute* route,
                                RouteQueue* eq, EvalCache* cache) {
  ChildNode* child;

  // 1. Chooses the move with the highest action value.
  int selected_id = 0;
  double max_action_value = -128;

  double nd_rollout_rate = nd->rollout_rate();
  double nd_value_rate = nd->value_rate();
  int num_nd_games =
      NNSearch ? nd->num_total_values() : nd->num_total_rollouts();
  bool is_initial = b->game_ply() == 0;
  double cp_nd = log((1 + num_nd_games + cp_base_) / cp_base_) + cp_init_;

  int imax = nd->num_children();
  // Excludes pass move.
  if (NNSearch && imax > 1) {
    if (search_limit_ > 0 && nd->num_total_values() < search_limit_ &&
        (!consider_pass_ || b->game_ply() < 2 * kNumRvts / 3)) {
      imax--;
    } else if (b->move_before() == kPass && !consider_pass_ && imax > 3) {
      imax--;
    }
  }

  double reduction = 0.0;
  if (NNSearch && (!use_dirichlet_noise_ || route->depth > 0 || is_initial)) {
    // Sum of probability of children whose visit count > 0.
    double sum_p = 0.0;
    for (int i = 0; i < imax; ++i) {
      int num_visits = nd->children[i].num_values();
      if (num_visits == 0) continue;

      sum_p += nd->children[i].prob();
    }
    reduction = 0.25 * sqrt(std::abs(sum_p));
  }

  for (int i = 0; i < imax; ++i) {
    // a. Searchs in descending order of probability.
    child = &nd->children[i];
    int num_cn_rollouts = child->num_rollouts();
    int num_cn_values = child->num_values();

    // b. Calculates winning rate of this move.
    double rollout_rate = (/*!is_initial && */ num_cn_rollouts == 0)
                              ? nd_rollout_rate
                              : child->rollout_rate();
    double value_rate = (/*!is_initial &&*/ num_cn_values == 0)
                            ? nd_value_rate - reduction
                            : child->value_rate();
    double rate = (1 - lambda_) * rollout_rate + lambda_ * value_rate;

    // c. Calculates action value.
    double num_cn_games = NNSearch ? num_cn_values : num_cn_rollouts;
    double action_value =
        rate + cp_nd * child->prob() * sqrt(num_nd_games) / (1 + num_cn_games);

    // d. Updates max_action_value.
    if (action_value > max_action_value && b->IsLegal(child->move())) {
      max_action_value = action_value;
      selected_id = i;
    }
  }

  // 2. Searches for the move with the maximum action value.
  child = &nd->children[selected_id];
  nd->VirtualLoss<NNSearch>(selected_id, virtual_loss_);

  child->WaitForComplete();
  Node* nnd = child->has_next() ? child->next_ptr() : nullptr;

  route->Add(child->move(), selected_id);
  Vertex next_move = child->move();
  Vertex prev_move = b->move_before();
  double result = 0.0;

  // 3. Places next move.
  ASSERT_LV3(b->IsLegal(next_move));

  bool reach_end = false;
  RepetitionState repetition = kRepetitionNone;
  bool double_pass = next_move == kPass && prev_move == kPass;
  if (NNSearch && nnd == nullptr && !double_pass) {
    repetition = b->CheckRepetition(next_move);
    reach_end = b->game_ply() + 1 >= kMaxPly || repetition != kRepetitionNone;
  }
  Color c_nd = b->side_to_move();
  b->MakeMove<kOneWay>(next_move);

  // 4. Adds the node to eval_worker_ if not expanded.
  if (NNSearch && nnd == nullptr) {
    if (double_pass || reach_end) ++num_reach_ends_;

    if (reach_end) {
      if (b->game_ply() >= kMaxPly) {
        result = nd->value();
      } else {
        result = repetition == kRepetitionDraw
                     ? 0
                     : repetition == kRepetitionLose ? -1 : 1;
      }
      route->leaf = kReachEnd;
    } else if (double_pass) {
      bool remain_sensible = false;
      for (Vertex v : b->empties()) {
        if (b->IsSensible(v)) {
          remain_sensible = true;
          break;
        }
      }

      if (!remain_sensible) {
        Color nd_color = ~b->side_to_move();
        Color winner = b->Rollout(komi_);
        result = winner == kEmpty ? 0 : winner == nd_color ? 1 : -1;
      } else {
        result = nd->value();
      }

      route->leaf = kReachEnd;
    } else {
      if (stop_think_ || !child->SetCreatingState()) {
        route->leaf = kFailToPush;
        nd->VirtualLoss<true>(selected_id, -virtual_loss_);
        return 0.0;
      }

      ValueAndProb vp;
      bool found_cache = false;
      if (cache == nullptr)
        found_cache = eval_cache_.Probe(*b, &vp);
      else
        found_cache = cache->Probe(*b, &vp);

      if (!found_cache) {
        if (eq == nullptr) {
          eval_worker_->Evaluate(b->get_feature(), &vp);
          if (cache == nullptr)
            eval_cache_.Insert(b->key(), vp);
          else
            cache->Insert(b->key(), vp);
        } else {
          eq->push(*b, *route);
          route->leaf = kWaitEval;
          return 0.0;
        }
      }

      std::unique_ptr<Node> pnd =
          std::move(std::unique_ptr<Node>(new Node(*b)));
      pnd->AddValueOnce(vp.value);
      SetNextNode(nd, selected_id, &pnd, vp);
      child->SetCompleteState();

      route->leaf = kEvaluated;
      result = -vp.value;
      ++num_evaluated_;
    }
  }

  // 6. Proceeds to the next node.
  if (nnd != nullptr) {  // Goes to next node.
    result = -SearchBranch<NNSearch>(child->next_ptr(), b, route, eq, cache);
  } else if (!NNSearch) {  // Rollout.
    Color winner = b->Rollout(komi_);
    result = winner == kEmpty ? 0 : winner == c_nd ? 1 : -1;
  }

  if (!NNSearch) {
    nd->VirtualWin<false>(selected_id, virtual_loss_, 1, result);
  } else if (route->leaf == kFailToPush) {
    nd->VirtualLoss<true>(selected_id, -virtual_loss_);
  } else if (route->leaf != kWaitEval) {
    nd->VirtualWin<true>(selected_id, virtual_loss_, 1, result);
    if (route->leaf == kEvaluated) nd->increment_entries();
  }

  return result;
}

Vertex SearchTree::Search(const Board& b, double time_limit,
                          double* winning_rate, bool is_errout, bool ponder) {
  const auto t0 = std::chrono::system_clock::now();

  // 1. Updates root node.
  if (b.game_ply() == 0 && !ponder) RootNode::Init();

  // 2. If the root node is not evaluated, evaluates the probability.
  UpdateRoot(b);
  Node* nd = root_node();

  // 3. Returns pass if there is no legal move.
  if (nd->num_children() <= 1) {
    if (is_errout) {
      std::stringstream ss;
      PrintCandidates(nd, kVtNull, ss, false);
      PrintLog("%s", ss.str().c_str());
    }

    *winning_rate = 0.5;
    return kPass;
  }

  // 4. Adjusts lambda to progress.
  UpdateLambda(b.game_ply());
  num_evaluated_ = 0;
  num_reach_ends_ = 0;

  // 5. Sorts child nodes in descending order of search count.
  std::vector<ChildNode*> candidates = SortChildren(*nd);

  // 6. Calculates the winning percentage of pc0.
  *winning_rate = WinningRate(*candidates[0]);
  int num_cand0_games = candidates[0]->num_values();
  int num_cand1_games = candidates[1]->num_values();
  bool has_no_time = false;

  // 7-1. Returns best move without searching when not having enough time.
  if (!ponder && time_limit == 0.0 && byoyomi() == 0.0 &&
      left_time() < Options["emergency_time"].get_double()) {
    has_no_time = true;

    // a. Returns pass if the previous move is pass in Japanese rule.
    if (consider_pass_ && b.move_before() == kPass) return kPass;

    // b. Returns the move with highest probability if search count is not
    //    enough.
    if (num_cand0_games < 300) {
      Vertex v = candidates[0]->move();
      if (is_errout) {
        PrintLog(
            "game ply=%d: emagency mode: left time=%.1f[sec], move=%s, "
            "prob=.1%f[%%]\n",
            b.game_ply() + 1, left_time(), v2str(v).c_str(),
            nd->children[0].prob() * 100);
      }
      *winning_rate = 0.5;
      return v;
    }
  } else {  // 7-2. Normal search
    {
      int num_prev_games = nd->num_total_values();

      double think_time = time_limit;
      bool extendable = false;

      // b. Gets maximum thinking time.
      if (!ponder && think_time == 0.0) {
        auto elapsed_time = ElapsedTime(t0);

        think_time = ThinkingTime(b.game_ply(), &extendable, elapsed_time);

        // Thinks only for 1sec when either winning percentage is over 95%.
        if ((*winning_rate < 0.01 || *winning_rate > 0.95) &&
            (komi_ != 0.0 || b.game_ply() > kNumRvts / 4) &&
            search_limit_ <= 0) {
          if (byoyomi() > 0)
            think_time = std::min(think_time, std::max(std::min(3.0, byoyomi()),
                                                       byoyomi() * 0.1));
          else
            think_time =
                std::min(think_time, std::max(1.0, main_time() * 0.001));
        }
        extendable &= (think_time > 1 && b.game_ply() >= 7);
      }

      // c. Searchs in parallel with thread_cnt threads.
      AllocateThreads(b, think_time, ponder);
      candidates = std::move(SortChildren(*nd));

      // d. Extends thinking time when the trial number of first move
      //    and second move is close.
      if (extendable) {
        num_cand0_games = candidates[0]->num_values();
        num_cand1_games = candidates[1]->num_values();

        double rate_cand0 = candidates[0]->value_rate();
        double rate_cand1 = candidates[1]->value_rate();

        if (num_cand0_games < num_cand1_games * 1.5 ||
            (num_cand0_games < num_cand1_games * 2.5 &&
             rate_cand0 < rate_cand1)) {
          if (byoyomi() > 0 && left_time() <= byoyomi()) {
            set_num_extensions(num_extensions() - 1);
          } else {
            think_time *= 0.7;
          }

          stop_think_ = false;
          AllocateThreads(b, think_time, ponder);
        }
      }

      stop_think_ = false;
      candidates = std::move(SortChildren(*nd));

      // e. Outputs search information
      auto elapsed_time = ElapsedTime(t0);
      if (is_errout) {
        PrintLog(
            "game ply=%d: left time=%.1f[sec]\n%d[nodes] %.1f[sec] "
            "%d[playouts] %.1f[pps/thread]\n",
            b.game_ply() + 1, std::max(0.0, left_time() - elapsed_time),
            num_entries(), elapsed_time,
            (nd->num_total_values() - num_prev_games),
            (nd->num_total_values() - num_prev_games) / elapsed_time /
                num_gpus_);
      }
    }
  }

  // 8. Checks whether pass should be returned. (for Japanese rule)
  if (consider_pass_ && !ponder && !has_no_time &&
      (b.move_before() == kPass || b.game_ply() >= kNumRvts / 2)) {
    int num_policy_moves = 32;
    if (b.move_before() == kPass || candidates[0]->move() == kPass)
      num_policy_moves = -1;

    Vertex nv = ShouldPass(b, candidates[0]->move(), num_policy_moves, 1024);
    if (nv == kPass && num_policy_moves > 0)
      nv = ShouldPass(b, candidates[0]->move(), -1, 1024);

    if (nv != candidates[0]->move()) {
      if (is_errout) {
        PrintLog("total games=%d, evaluated =%d\n", nd->num_total_values(),
                 num_evaluated_.load());

        std::stringstream ss;
        PrintCandidates(root_node(), nv, ss, false);
        PrintLog("%s", ss.str().c_str());
      }

      *winning_rate = 0.5;
      return nv;
    }
  } else if (!consider_pass_ && candidates[0]->move() == kPass) {
    // 9. When the best move is pass and the result is not much different,
    //    returns the second move. (for Chinese rule)
    if (candidates[0]->win_values() < 0 && candidates[1]->win_values() > 0)
      std::swap(candidates[0], candidates[1]);
  }

  // 10. Updates winning_rate.
  *winning_rate = WinningRate(*candidates[0]);

  // 11. Outputs information of top-5 child nodes.
  if (is_errout) {
    PrintLog("total games=%d, evaluated =%d\n", nd->num_total_values(),
             num_evaluated_.load());

    std::stringstream ss;
    PrintCandidates(root_node(), kVtNull, ss, false);
    PrintLog("%s", ss.str().c_str());
  }

  return candidates[0]->move();
}

void SearchTree::EvaluateWorker(const Board& b, double time_limit, bool ponder,
                                int th_id) {
  const auto t0 = std::chrono::system_clock::now();
  auto nd = root_node();
  int num_initial_games = nd->num_total_values();
  Board b_;

  while (!stop_think_) {
    b_ = b;
    SearchRoute route;
    SearchBranch<true>(root_node(), &b_, &route);
    double elapsed_time = th_id == 0 ? ElapsedTime(t0) : 0.0;

    bool reach_limit =
        (search_limit_ > 0 && !ponder && num_evaluated_ >= search_limit_);
    bool reach_ends =
        (num_reach_ends_ >= 10000 ||
         (search_limit_ > 0 && num_reach_ends_ >= 2 * search_limit_));
    bool exceed_time = elapsed_time > time_limit || entry_rate() > 0.9;

    if (reach_limit || reach_ends || exceed_time) {
      stop_think_ = true;
      break;
    }

    if (ponder || elapsed_time < 1.0 || search_limit_ > 0) continue;

    int num_games = nd->num_total_values() - num_initial_games;
    if (nd->num_total_values() < num_games / elapsed_time * 10) continue;

    std::vector<ChildNode*> rc = SortChildren(*nd);
    int num_cand0_games = rc[0]->num_values();
    int num_cand1_games = rc[1]->num_values();
    double max_cand1_games = num_cand1_games + num_games *
                                                   (time_limit - elapsed_time) /
                                                   elapsed_time;

    double winning_rate = WinningRate(*rc[0]);
    bool stand_out = num_cand0_games > 100 * num_cand1_games;
    bool cannot_catchup = num_cand0_games > 1.5 * max_cand1_games;
    bool almost_win = (winning_rate < 0.01 || winning_rate > 0.95) &&
                      (komi_ != 0.0 || b.game_ply() > kNumRvts / 4);

    if (stand_out || cannot_catchup || almost_win) {
      stop_think_ = true;
      break;
    }
  }
}

double SearchTree::FinalScore(const Board& b, Vertex next_move,
                              int num_policy_moves, int num_playouts,
                              Board::OwnerMap* owner, TensorEngine* engine,
                              EvalCache* cache) {
  Board b_policy = b;
  if (next_move <= kPass && b.IsLegal(next_move))
    b_policy.MakeMove<kOneWay>(next_move);

  // Adds turn bonus of white.
  if (b_policy.side_to_move() == kWhite) b_policy.increment_passes(kWhite);

  if (cache == nullptr) {
    if (validate_engine_)
      cache = &validate_cache_;
    else
      cache = &eval_cache_;
  }

  auto DoesInduceNakade = [](const Board& b, Vertex v) {
    if (b.count_neighbors(v, kBlack) == 0 || b.count_neighbors(v, kWhite) == 0)
      return false;

    std::vector<int> sg_ids;
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (!is_stone(b.color_at(v_nbr))) continue;
      int id_ = b.sg_id(v_nbr);
      if (b.sg_num_liberties_at(v_nbr) != 3) return false;
      if (b.color_at(v_nbr) == ~b.side_to_move() && b.sg_size_at(v_nbr) >= 5)
        return false;

      if (std::find(sg_ids.begin(), sg_ids.end(), id_) == sg_ids.end())
        sg_ids.push_back(id_);

      if (sg_ids.size() > 2) return false;
    }

    Bitboard libs;
    for (auto& id_ : sg_ids) libs.Merge(b.sg_liberties_at(Vertex(id_)));
    if (libs.num_bits() != 3) return false;

    Board b_ = b;
    b_.MakeMove<kOneWay>(v);
    for (auto& v_lib : libs.Vertices()) {
      if (v_lib == v) continue;
      if (b_.IsSelfAtariNakade(v_lib)) return true;
    }

    return false;
  };

  auto IsFatalSelfAtari = [](const Board& b, Vertex v) {
    if (!b.IsSelfAtariWithoutNakade(b.side_to_move(), v)) return false;

    int nbr_size = 0;
    bool checked[kNumVts] = {false};
    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      if (b.color_at(v_nbr) != b.side_to_move()) continue;
      if (checked[b.sg_id(v_nbr)]) continue;
      checked[b.sg_id(v_nbr)] = true;

      nbr_size += b.sg_size_at(v_nbr);
      if (nbr_size > 1) return true;
    }

    return false;
  };

  int max_num_moves =
      num_policy_moves < 0 ? kMaxPly - 2 : b.game_ply() + num_policy_moves;
  int num_policy_passes = 0;

  ValueAndProb vp;
  while (b_policy.game_ply() < max_num_moves) {
    if (cache->Probe(b_policy, &vp) == false) {
      Feature ft(b_policy.get_feature());
      if (engine)
        engine->Infer(ft, &vp);
      else if (validate_engine_)
        validate_engine_->Infer(ft, &vp);
      else
        eval_worker_->Evaluate(ft, &vp);

      cache->Insert(b_policy.key(), vp);
    }

    std::vector<std::pair<float, int>> sorted_candidates;
    for (int i = 0; i < kNumRvts; ++i) {
      Vertex vi = rv2v((RawVertex)i);
      if (b_policy.color_at(vi) == kEmpty && b_policy.IsSensible(vi) &&
          !DoesInduceNakade(b_policy, vi))
        sorted_candidates.push_back({vp.prob[i], i});
    }

    std::sort(sorted_candidates.begin(), sorted_candidates.end(),
              std::greater<std::pair<float, int>>());

    bool has_sensible = false;

    if (b_policy.SelectMoveAny(b_policy.side_to_move()) != kVtNull) {
      for (auto& p : sorted_candidates) {
        Vertex v = rv2v((RawVertex)p.second);
        if (sorted_candidates.size() > 3 || !IsFatalSelfAtari(b_policy, v)) {
          b_policy.MakeMove<kOneWay>(v);
          has_sensible = true;
          break;
        }
      }
    }

    if (!has_sensible) {
      b_policy.MakeMove<kOneWay>(kPass);
      b_policy.decrement_passes(~b_policy.side_to_move());
      num_policy_passes++;
    }

    if (b_policy.double_pass()) break;
  }

  std::unordered_map<double, int> scores;
  if (b_policy.game_ply() == kMaxPly - 2) {
    if (num_policy_passes < 32) return 0;  // Draw

    std::vector<Vertex> ko_vertices;
    int num_enclosed[kNumPlayers] = {0};
    // Double ko seki.
    for (Vertex v : b_policy.empties()) {
      if (!b_policy.has_atari_neighbor(v)) continue;

      if (b_policy.enclosed_by(v, kBlack))
        num_enclosed[kBlack]++;
      else if (b_policy.enclosed_by(v, kWhite))
        num_enclosed[kWhite]++;
      else
        continue;

      for (const auto& d : dir4) {
        if (b_policy.has_atari_neighbor_at(v, d) &&
            b_policy.sg_size_at(v + dir2v(d)) == 1)
          ko_vertices.push_back(v);
      }
    }

    if (ko_vertices.size() != 2) return 0;

    if (num_enclosed[kWhite] == 2) {
      if (b_policy.IsLegal(ko_vertices[0]))
        b_policy.MakeMove<kOneWay>(ko_vertices[0]);
      else if (b_policy.IsLegal(ko_vertices[1]))
        b_policy.MakeMove<kOneWay>(ko_vertices[1]);
      else
        return 0;
    }

    for (int i = 0; i < num_playouts; ++i) {
      double s = b_policy.Score(komi_, owner);
      if (scores.count(s) == 0)
        scores[s] = 1;
      else
        scores[s]++;
    }

    for (auto& vi : ko_vertices) {
      (*owner)[kBlack][vi] = 0;
      (*owner)[kWhite][vi] = 0;
      for (const auto& d : dir4) {
        Vertex vi_nbr = vi + dir2v(d);
        Color c_vi = b_policy.color_at(vi);
        Color c_nbr = b_policy.color_at(vi_nbr);
        if ((c_vi == kEmpty && b_policy.has_atari_neighbor_at(vi, d)) ||
            (c_vi != kEmpty && c_nbr == kEmpty)) {
          (*owner)[kBlack][vi_nbr] = 0;
          (*owner)[kWhite][vi_nbr] = 0;
          break;
        }
      }
    }
  } else {
    // Rollout
    Board b_rollout;
    for (int i = 0; i < num_playouts; ++i) {
      b_rollout = b_policy;
      if (b_rollout.side_to_move() == kWhite)
        b_rollout.decrement_passes(kWhite);
      b_rollout.Rollout(komi_);
      double s = b_rollout.Score(komi_, owner);
      if (scores.count(s) == 0)
        scores[s] = 1;
      else
        scores[s]++;
    }
  }

  // Sorts scores.
  std::vector<std::pair<int, double>> desc_scores;
  for (auto& sc : scores) desc_scores.push_back({sc.second, sc.first});
  std::sort(desc_scores.begin(), desc_scores.end(),
            std::greater<std::pair<int, double>>());

  double best_score = desc_scores[0].second;

  // Chinese rule.
  if (!consider_pass_) return best_score;

  // Japanese rule.
  auto vc_fills = b.NeedToBeFilled(num_playouts, *owner);

  bool filled[kNumVts] = {false};
  for (auto& vc : vc_fills) {
    Vertex v_fill = vc.first;
    Color c_fill = vc.second;
    if (c_fill != kEmpty) filled[v_fill] = true;
  }

  bool checked[kNumVts] = {false};
  for (RawVertex rv = kRvtZero; rv < kNumRvts; ++rv) {
    Vertex v = rv2v(rv);
    if (b_policy.color_at(v) != kEmpty) continue;
    if (!b_policy.IsSeki(v) && !((*owner)[kBlack][v] / num_playouts < 0.2 &&
                                 (*owner)[kWhite][v] / num_playouts < 0.2))
      continue;

    for (const auto& dv : dv4) {
      Vertex v_nbr = v + dv;
      Color c_nbr = b_policy.color_at(v_nbr);
      if (!is_stone(c_nbr) || checked[b_policy.sg_id(v_nbr)]) continue;
      checked[b_policy.sg_id(v_nbr)] = true;

      Vertex v_tmp = v_nbr;
      do {
        Color prev_c = b.color_at(v_tmp);
        // Needs to fill in seki.
        if (prev_c != c_nbr && !filled[v_tmp]) {
          filled[v_tmp] = true;
          vc_fills.push_back({v_tmp, c_nbr});
        }

        // Eye shape in seki.
        for (const auto& dv_tmp : dv4) {
          Vertex v_tmp2 = v_tmp + dv_tmp;
          if (b_policy.color_at(v_tmp2) != kEmpty) continue;

          if (!filled[v_tmp2] && (b_policy.IsEyeShape(c_nbr, v_tmp2) ||
                                  b_policy.IsFalseEye(v_tmp2))) {
            filled[v_tmp2] = true;
            vc_fills.push_back({v_tmp2, c_nbr});
          }
        }

        v_tmp = b_policy.next_v(v_tmp);
      } while (v_tmp != v_nbr);
    }
  }

  for (auto& vc : vc_fills) {
    Vertex v_fill = vc.first;
    Color c_fill = vc.second;
    if (c_fill == kBlack)
      best_score -= 1;
    else if (c_fill == kWhite)
      best_score += 1;
  }

  return best_score;
}

Vertex SearchTree::ShouldPass(const Board& b, Vertex next_move,
                              int num_policy_moves, int num_playouts,
                              TensorEngine* engine, EvalCache* cache) {
  auto scores = b.RolloutScores(num_playouts, kPass, -1, true, false);
  double total_wins = 0.0;
  for (auto& score_and_games : scores) {
    double s = score_and_games.first;
    int games = score_and_games.second;
    if ((b.side_to_move() == kBlack && s > 0) ||
        (b.side_to_move() == kWhite && s < 0))
      total_wins += games;
    else if (s == 0)
      total_wins += 0.5 * games;
  }
  double rollout_rate = total_wins / num_playouts;

  Board::OwnerMap owner_pass = {0};
  double s_pass = FinalScore(b, kPass, num_policy_moves, num_playouts,
                             &owner_pass, engine, cache);
  if (b.side_to_move() == kWhite) s_pass = -s_pass;

  if (s_pass == 0) return next_move;  // Draw

  auto need_fills = b.NeedToBeFilled(num_playouts, owner_pass);

  if (num_policy_moves > 0 && need_fills.empty()) {
    Board::OwnerMap owner_pass_rep = {0};
    double s_pass_rep =
        FinalScore(b, kPass, -1, num_playouts, &owner_pass_rep, engine, cache);
    if (b.side_to_move() == kWhite) s_pass_rep = -s_pass_rep;

    auto need_fills_rep = b.NeedToBeFilled(num_playouts, owner_pass_rep);
    if (!need_fills_rep.empty() || s_pass_rep != s_pass) {
      s_pass = s_pass_rep;
      num_policy_moves = -1;
      for (auto& nfr : need_fills_rep) need_fills.push_back(nfr);
    }
  }

  // Almost win and no verteces to fill.
  if (rollout_rate >= 0.95 && s_pass > 0 && need_fills.empty()) {
    if (s_pass >= 1.5) {
      Vertex v = b.move_before();
      if (v < kPass && owner_pass[b.side_to_move()][v] / num_playouts > 0.65)
        return next_move;
    }

    return kPass;
  }

  Vertex nv = next_move;
  Vertex v_fill = kPass;
  if (nv >= kPass || !need_fills.empty()) {
    ValueAndProb vp;
    Board b_ = b;
    if (cache == nullptr) cache = &eval_cache_;

    if (cache->Probe(b_, &vp) == false) {
      Feature ft(b_.get_feature());
      if (engine)
        engine->Infer(ft, &vp);
      else
        eval_worker_->Evaluate(ft, &vp);

      cache->Insert(b_.key(), vp);
    }

    std::vector<std::pair<float, int>> sorted_candidates;
    for (int i = 0; i < kNumRvts; ++i)
      sorted_candidates.push_back({vp.prob[i], i});
    std::sort(sorted_candidates.begin(), sorted_candidates.end(),
              std::greater<std::pair<float, int>>());

    for (auto& po : sorted_candidates) {
      Vertex v = rv2v((RawVertex)po.second);
      if (b.IsSensible(v)) {
        nv = v;
        break;
      }
    }

    double max_prob = -1;
    for (auto& nf : need_fills) {
      Vertex v = nf.first;
      Color c = nf.second;
      if (c == ~b.side_to_move()) continue;

      if (vp.prob[v2rv(v)] > max_prob && b.IsLegal(v)) {
        v_fill = v;
        max_prob = vp.prob[v2rv(v)];
      }
    }

    if (nv == kVtNull) nv = kPass;
  }

  auto score_with_move = [&](Vertex v) {
    Board b_ = b;
    b_.MakeMove<kOneWay>(v);
    Board::OwnerMap owner_ = {0};
    double s = FinalScore(b_, kPass, num_policy_moves, num_playouts, &owner_,
                          engine, cache);
    if (b.side_to_move() == kWhite) s = -s;

    return s;
  };

  if (s_pass != 0.5 && b.move_before() != kPass) {
    if (next_move == kPass && v_fill != kPass) {
      double s_fill = score_with_move(v_fill);
      if (std::abs(s_pass - s_fill) <= 1 || (s_pass < 0 && s_fill > 0))
        return v_fill;
    } else if (s_pass <= 0 && v_fill == kPass) {
      double s_nv = score_with_move(nv);
      if (!b.has_counter_move() && s_pass - s_nv == 1) return kPass;
    }

    return next_move;
  }

  double s_nv = score_with_move(nv);

  // Lose.
  if (b.move_before() == kPass && s_nv < 0) return v_fill;  // kPass or fill

  // Needs pass.
  if (s_pass == 0.5 && s_nv == -0.5) {
    if (v_fill != kPass) {
      if (next_move == kPass) {
        double s_fill = score_with_move(v_fill);
        if (s_fill >= -0.5) return v_fill;
      }

      return next_move;
    } else {
      return kPass;
    }
  }

  return next_move == kPass ? v_fill : next_move;
}

std::string SearchTree::PV(const Node* nd, Vertex head_move,
                           int max_move) const {
  std::string seq = "";
  if (head_move > kPass) return seq;

  std::string head_str = v2str(head_move);
  if (head_str.length() == 2) head_str += " ";

  seq += head_str;
  Vertex prev_move = head_move;

  std::vector<ChildNode*> child_list;
  for (int i = 0; i < max_move; ++i) {
    if (nd->num_children() <= 1) break;
    child_list = std::move(SortChildren(*nd));

    if (child_list[0]->num_values() == 0) break;

    std::string move_str = v2str(child_list[0]->move());
    if (move_str.length() == 2) move_str += " ";

    seq += "->" + move_str;

    if (!child_list[0]->has_next())
      break;
    else if (prev_move == kPass && child_list[0]->move() == kPass)
      break;

    prev_move = child_list[0]->move();
    nd = child_list[0]->next_ptr();
  }

  return seq;
}

void SearchTree::PrintCandidates(const Node* nd, int next_move,
                                 std::ostream& ost, bool flip_value) const {
  std::vector<ChildNode*> candidates = SortChildren(*nd);
  bool pickup_next = (next_move != kVtNull);
  std::vector<int> candidate_ids;

  if (pickup_next) {
    bool found = false;
    for (int i = 0, imax = candidates.size(); i < imax; ++i) {
      if (candidates[i]->move() == next_move) {
        candidate_ids.push_back(i);
        found = true;
        break;
      }
    }

    if (!found) {
      ost << "move not found\n";
      return;
    }
  }

  int imax = std::min(nd->num_children(), pickup_next ? 3 : 5);
  for (int i = 0; i < imax; ++i) {
    candidate_ids.push_back(i);
  }

  ost << "|move|count  |value|roll |prob |depth| best sequence" << std::endl;
  for (size_t i = 0; i < candidate_ids.size(); ++i) {
    ChildNode* child = candidates[candidate_ids[i]];

    int num_games = child->num_values();
    if (num_games == 0) break;

    double rollout_rate = (child->rollout_rate() + 1) / 2;
    double value_rate = (child->value_rate() + 1) / 2;
    if (flip_value) {
      rollout_rate = 1.0 - rollout_rate;
      value_rate = 1.0 - value_rate;
    }

    int depth = 1;
    std::string seq;
    if (child->has_next()) {
      depth = MaxDepth(*child->next_ptr(), kVtNull, depth);
      seq = PV(child->next_ptr(), child->move());
    }

    ost << "|" << std::left << std::setw(4) << v2str(child->move());
    ost << "|" << std::right << std::setw(7) << std::min(9999999, num_games);
    auto prc = ost.precision();
    ost.precision(1);
    if (child->num_values() == 0)
      ost << "|" << std::setw(5) << "N/A";
    else
      ost << "|" << std::setw(5) << std::fixed << value_rate * 100;
    if (child->num_rollouts() == 0)
      ost << "|" << std::setw(5) << "N/A";
    else
      ost << "|" << std::setw(5) << std::fixed << rollout_rate * 100;
    ost << "|" << std::setw(5) << std::fixed << child->prob() * 100;
    ost.precision(prc);
    ost << "|" << std::setw(5) << depth;
    ost << "| " << seq;
    ost << std::endl;
    if (pickup_next && i == 0) ost << "--" << std::endl;
  }
}

void SearchTree::LizzieInfo(const Node* nd, std::ostream& ost) const {
  if (!nd) return;
  std::vector<ChildNode*> candidates = SortChildren(*nd);
  std::regex re(" ->|->");
  bool need_space = false;
  int cutoff = 0;

  for (int i = 0, imax = nd->num_children(); i < imax; ++i) {
    ChildNode* child = candidates[i];
    int num_games = child->num_values();

    if (i != 0 && num_games == 0) break;
    if (!cutoff) cutoff = static_cast<int>(sqrt(num_games));
    if (num_games < cutoff) break;

    double rollout_rate = (child->rollout_rate() + 1) / 2;
    double value_rate = (child->value_rate() + 1) / 2;
    double rate = (1 - lambda_) * rollout_rate + lambda_ * value_rate;
    int depth = 1;
    std::string seq;
    if (child->has_next()) {
      depth = MaxDepth(*child->next_ptr(), kVtNull, 1);
      seq = PV(child->next_ptr(), child->move(), depth);
      seq = std::regex_replace(seq, re, " ");
    }

    if (need_space) ost << " ";
    ost << "info move " << v2str(child->move());
    ost << " visits " << num_games;
    ost << " winrate " << static_cast<int>(rate * 10000);
    ost << " prior " << static_cast<int>(child->prob() * 10000);
    ost << " order " << i;
    ost << " pv " << seq;
    need_space = seq.back() != ' ';
  }

  ost << std::endl;
}
