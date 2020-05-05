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

#ifndef EVAL_WORKER_H_
#define EVAL_WORKER_H_

#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "./network.h"
#include "./option.h"

/**
 * @class EvalWorker
 * The EvalWorker class waits for features to be evaluated, and when the queue
 * piles up, it performs inference asynchronously on the GPU for each batch
 * size.
 *
 * This class is implemented with reference to OpenCLScheduler of LeelaZero.
 * https://github.com/leela-zero/leela-zero/blob/next/src/OpenCLScheduler.cpp
 */
class EvalWorker {
 public:
  ~EvalWorker() {
    {
      std::unique_lock<std::mutex> lk(mx_);
      running_ = false;
    }
    cv_.notify_all();
    if (workers_.size() > 0)
      for (auto& th : workers_) th.join();
  }

  EvalWorker() {
    running_ = true;
    wait_time_millisec_ = 10;
    in_single_eval_.store(false);
    batch_size_ = Options["batch_size"].get_int();
    use_full_features_ = Options["use_full_features"].get_bool();
    value_from_black_ = Options["value_from_black"].get_bool();
  }

  void Init(std::vector<int> gpu_ids, std::string model_path = "") {
    int num_threads = 2;
    if (gpu_ids.empty()) {
      int num_gpus = Options["num_gpus"].get_int();
      for (int i = 0; i < num_gpus; ++i) gpu_ids.push_back(i);
    }

    for (auto gpu_id : gpu_ids) {
      for (int i = 0; i < num_threads; ++i) {
        auto th =
            std::thread(&EvalWorker::BatchWorker, this, gpu_id, model_path);
        workers_.push_back(std::move(th));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 50 msec
      }
    }
  }

  void ReplaceModel(std::vector<int> gpu_ids, std::string model_path = "") {
    running_ = false;
    cv_.notify_all();
    for (auto& th : workers_) th.join();
    workers_.clear();

    running_ = true;
    wait_time_millisec_ = 10;
    in_single_eval_.store(false);

    int num_threads = 2;
    for (auto gpu_id : gpu_ids) {
      for (int i = 0; i < num_threads; ++i) {
        auto th =
            std::thread(&EvalWorker::BatchWorker, this, gpu_id, model_path);
        workers_.push_back(std::move(th));
      }
    }
  }

  void Evaluate(const Feature& ft, ValueAndProb* vp) {
    auto entry = std::make_shared<SyncedEntry>(ft);

    std::unique_lock<std::mutex> lk(entry->mx);

    {
      std::lock_guard<std::mutex> lk(mx_);
      synced_queue_.push_back(entry);

      if (in_single_eval_.load() && wait_time_millisec_ < 15)
        wait_time_millisec_ += 2;
    }

    cv_.notify_one();
    entry->cv.wait(lk);

    *vp = entry->vp;
  }

  std::mutex* get_mutex() { return &mx_; }

 private:
  std::atomic<bool> running_;
  std::mutex mx_;
  std::condition_variable cv_;
  bool use_full_features_;
  bool value_from_black_;
  int wait_time_millisec_;
  std::atomic<bool> in_single_eval_;
  int batch_size_;
  std::deque<std::shared_ptr<SyncedEntry>> synced_queue_;
  std::vector<std::thread> workers_;

  std::vector<std::shared_ptr<SyncedEntry>> PickupEntry() {
    std::vector<std::shared_ptr<SyncedEntry>> entry_queue;
    int num_entries = 0;

    std::unique_lock<std::mutex> lk(mx_);
    while (true) {
      if (!running_) return std::move(entry_queue);

      num_entries = synced_queue_.size();
      if (num_entries >= batch_size_) {
        num_entries = batch_size_;
        break;
      }

      bool timeout = !cv_.wait_for(
          lk, std::chrono::milliseconds(wait_time_millisec_), [this]() {
            return !running_ ||
                   static_cast<int>(synced_queue_.size()) >= batch_size_;
          });

      if (!synced_queue_.empty()) {
        if (timeout && in_single_eval_.exchange(true) == false) {
          if (wait_time_millisec_ > 1) {
            wait_time_millisec_--;
          }
          num_entries = 1;
          break;
        }
      }
    }

    auto end = synced_queue_.begin();
    std::advance(end, num_entries);
    std::move(synced_queue_.begin(), end, std::back_inserter(entry_queue));
    synced_queue_.erase(synced_queue_.begin(), end);

    return std::move(entry_queue);
  }

  void BatchWorker(const int gpu_id, std::string model_path) {
    TensorEngine engine(gpu_id, batch_size_);

    {
      std::lock_guard<std::mutex> lock(mx_);
      engine.Init(model_path, use_full_features_, value_from_black_);
    }

    while (true) {
      auto entry_queue = PickupEntry();
      int num_entries = entry_queue.size();

      if (!running_) return;
      engine.Infer(&entry_queue, kNumSymmetry);

      for (auto& entry : entry_queue) {
        std::lock_guard<std::mutex> lk(entry->mx);
        entry->cv.notify_all();
      }

      if (num_entries == 1) in_single_eval_ = false;
    }
  }
};

#endif  // EVAL_WORKER_H_
