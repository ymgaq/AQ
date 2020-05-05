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

#ifndef NETWORK_H_
#define NETWORK_H_

#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <NvUffParser.h>
#include <cuda_runtime_api.h>
#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "./eval_cache.h"
#include "./option.h"
#include "./route_queue.h"

constexpr int kInputFeatures = 52;
constexpr int kNumSymmetry = 8;

/**
 * @class Logger
 * Logger class of nvinfer.
 * Only outputs Error and ingnores Warning and Info messages.
 */
class Logger : public nvinfer1::ILogger {
  void log(Severity severity, const char *msg) override {
    switch (severity) {
      case Severity::kINTERNAL_ERROR:
        std::cerr << msg << std::endl;
        exit(1);
      case Severity::kERROR:
        std::cerr << msg << std::endl;
        exit(1);
      case Severity::kWARNING:
        break;
      case Severity::kINFO:
        break;
    }
  }
};

/**
 * @class TensorEngine
 * TensorEngine class handles inference order by GPU via TensorRT APIs.
 * If you want to use the same network structure as AlphaGoZero, you can make
 * (18 * 19 * 19) inferences by setting use_full_feature = false.
 * NOTE: Don't call Init() in different programs or threads at the same time.
 */
class TensorEngine {
 public:
  TensorEngine(int gpu_id, int batch_size)
      : engine_(nullptr),
        runtime_(nullptr),
        context_(nullptr),
        gpu_id_(gpu_id),
        use_uff_(true),
        max_batch_size_(batch_size) {}

  ~TensorEngine() {
    if (context_) context_->destroy();
    if (engine_) engine_->destroy();
    if (runtime_) runtime_->destroy();
    for (auto buf : device_bufs_) cudaFree(buf);
  }

  /**
   * Serializes the engine and save it to a file.
   * Serialized engines are incompatible across TensorRT versions and devices.
   */
  void SaveSerializedEngine(std::string save_path) {
    auto serialized_engine = engine_->serialize();
    std::ofstream ofs(save_path, std::ios::binary);
    ofs.write(reinterpret_cast<char *>(serialized_engine->data()),
              serialized_engine->size());
    serialized_engine->destroy();
  }

  /**
   * Assigns an index to the device buffer.
   */
  void SetBufferIndex() {
    if (engine_) {
      std::string inputs_name = engine_->getBindingName(0);
      std::string policy_name = engine_->getBindingName(1);
      std::string value_name = engine_->getBindingName(2);
      if (policy_name.find("value") != std::string::npos) {
        std::string tmp = policy_name;
        policy_name = value_name;
        value_name = tmp;
      }

      inputs_idx_ = engine_->getBindingIndex(inputs_name.c_str());
      policy_idx_ = engine_->getBindingIndex(policy_name.c_str());
      value_idx_ = engine_->getBindingIndex(value_name.c_str());
    }
  }

  /**
   * Builds the TensorRT engine from an ONNX file.
   * This method is called when a serialized engine is not found or does not
   * fit, and generates a serialized file.
   */
  void BuildFromOnnx(std::string onnx_path, std::string save_path);

  /**
   * Builds the TensorRT engine from an UFF file.
   * This method is called when a serialized engine is not found or does not
   * fit, and generates a serialized file.
   */
  void BuildFromUff(std::string uff_path, std::string save_path);

  /**
   * Loads a serialized engine file.
   * If the file is not found, build it from a UFF format file and save it.
   */
  void LoadEngine(std::string model_path);

  void Init(std::string model_path = "", bool use_full_features = true,
            bool value_from_black = false);

  /**
   * Infers a single board.
   */
  bool Infer(const Feature &ft, ValueAndProb *vp, int symmetry_idx = 0);

  /**
   * Infers boards from a list of SyncedEntries.
   */
  bool Infer(std::vector<std::shared_ptr<SyncedEntry>> *entries,
             int symmetry_idx = 0);

  /**
   * Infers boards from a list of RouteEntires.
   */
  bool Infer(std::vector<RouteEntry> *entries, int symmetry_idx = 0);

 private:
  nvinfer1::ICudaEngine *engine_;
  nvinfer1::IRuntime *runtime_;
  nvinfer1::IExecutionContext *context_;
  std::vector<void *> device_bufs_;
  std::vector<float> host_buf_;
  int gpu_id_;
  int max_batch_size_;
  int feature_size_;
  bool use_full_features_;
  bool value_from_black_;
  int inputs_idx_;
  int policy_idx_;
  int value_idx_;
  bool use_uff_;
};

#endif  // NETWORK_H_
