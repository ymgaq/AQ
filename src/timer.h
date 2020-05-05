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

#ifndef TIMER_H_
#define TIMER_H_

#include <algorithm>
#include "./option.h"

/**
 * @class Timer
 * A class for control of holding time.
 * Adjusts the maximum consideration time according to the time remaining and
 * the degree of progress.
 */
class Timer {
 public:
  Timer() {
    main_time_ = Options["main_time"].get_double();
    byoyomi_ = Options["byoyomi"].get_double();
    byoyomi_margin_ = Options["byoyomi_margin"].get_double();
    num_extensions_ = Options["num_extensions"].get_int();
    left_time_ = main_time_;
  }

  double main_time() const { return main_time_; }
  double byoyomi() const { return byoyomi_; }
  int num_extensions() const { return num_extensions_; }
  double left_time() const { return left_time_; }

  void set_main_time(double val) { main_time_ = val; }
  void set_num_extensions(int val) { num_extensions_ = val; }
  void set_byoyomi(double val) { byoyomi_ = val; }
  void set_left_time(double val) { left_time_ = val; }

  void Init() {
    main_time_ = Options["main_time"].get_double();
    byoyomi_ = Options["byoyomi"].get_double();
    byoyomi_margin_ = Options["byoyomi_margin"].get_double();
    num_extensions_ = Options["num_extensions"].get_int();
    left_time_ = main_time_;
  }

  double ThinkingTime(int ply, bool* extendable, double lost_time = 0.0) {
    double t = 1.0;
    *extendable = false;

    if (main_time_ == 0.0) {  // Byoyomi only.
      // Takes margin.
      if (byoyomi_ >= 10)
        t = byoyomi_ - byoyomi_margin_;
      else
        t = std::max(byoyomi_, 0.1);
      *extendable = (num_extensions_ > 0);
    } else {  // Main time + byoyomi or sudden death.
      if (left_time_ < byoyomi_ * 2.0) {
        t = std::max(byoyomi_ - byoyomi_margin_, 1.0);  // Takes margin.
        *extendable = (num_extensions_ > 0);
      } else {
        // Calculates from remaining time if sudden death
        // otherwise set that of 0.5-1 times of byoyomi
        //
        // 1-16 moves : 0.5     * byoyomi
        // 17-32 moves: 0.5-2.0 * byoyomi
        // > 32 moves : 2.0     * byoyomi
        t = std::max(
            left_time_ / (55.0 + std::max(50.0 - ply, 0.0)),
            byoyomi_ *
                (0.5 + 1.5 * std::min(1.0, std::max(0.0, (ply - 16) /
                                                             (32.0 - 16.0)))));
        // Does not extend thinking time if the remaining time is 30% or less.
        *extendable = (left_time_ > main_time_ * 0.3) || (byoyomi_ >= 10);
      }
    }

    t = std::max(t - lost_time, 0.1);
    return t;
  }

 private:
  double main_time_;
  double byoyomi_;
  double byoyomi_margin_;
  int num_extensions_;
  double left_time_;
};

/**
 * @class SearchParameter
 * Structure that stores hyperparameters for search.
 */
class SearchParameter {
 public:
  SearchParameter() {
    batch_size_ = Options["batch_size"].get_int();
    lambda_init_ = Options["lambda_init"].get_double();
    lambda_delta_ = Options["lambda_delta"].get_double();
    lambda_move_start_ = Options["lambda_move_start"].get_int();
    lambda_move_end_ = Options["lambda_move_end"].get_int();
    cp_init_ = Options["cp_init"].get_double();
    cp_base_ = Options["cp_base"].get_double();
    virtual_loss_ = Options["virtual_loss"].get_int();
    search_limit_ = Options["search_limit"].get_int();
    ladder_reduction_ = Options["ladder_reduction"].get_double();
  }

  SearchParameter& operator=(const SearchParameter& rhs) {
    batch_size_ = rhs.batch_size_;
    lambda_init_ = rhs.lambda_init_;
    lambda_delta_ = rhs.lambda_delta_;
    lambda_move_start_ = rhs.lambda_move_start_;
    lambda_move_end_ = rhs.lambda_move_end_;
    cp_init_ = rhs.cp_init_;
    cp_base_ = rhs.cp_base_;
    virtual_loss_ = rhs.virtual_loss_;
    search_limit_ = rhs.search_limit_;
    ladder_reduction_ = rhs.ladder_reduction_;

    return *this;
  }

#if defined(LEARN)
  friend class MySQLConnector;
#endif

 protected:
  int batch_size_;
  double lambda_init_;
  double lambda_delta_;
  int lambda_move_start_;
  int lambda_move_end_;
  double cp_init_;
  double cp_base_;
  int virtual_loss_;
  int search_limit_;
  double ladder_reduction_;
};

#endif  // TIMER_H_
