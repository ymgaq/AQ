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

#include "./network.h"

void TensorEngine::Init(std::string model_path, bool use_full_features,
                        bool value_from_black) {
  cudaSetDevice(gpu_id_);

  if (model_path == "") {
    if (Options["model_path"].get_string() == "default" ||
        Options["model_path"].get_string() == "update") {
      std::string engine_name = Options["rule"].get_int() == kChinese
                                    ? "model_cn.engine"
                                    : "model_jp.engine";
      model_path = JoinPath(Options["working_dir"], "engine", engine_name);
    } else {
      model_path = Options["model_path"].get_string();
    }
  }

  LoadEngine(model_path);
  int max_batch_size = engine_->getMaxBatchSize();
  for (int i = 0; i < engine_->getNbBindings(); ++i) {
    auto dim = engine_->getBindingDimensions(i);
    std::string dim_str = "(";
    int size = 1;
    for (int i = 0; i < dim.nbDims; ++i) {
      if (i) dim_str += ", ";
      dim_str += std::to_string(dim.d[i]);
      if (dim.d[i] > 0) size *= dim.d[i];
    }
    dim_str += ")";

    void *buf;
    cudaMalloc(&buf, max_batch_size * size * sizeof(float));
    device_bufs_.push_back(buf);
  }

  use_full_features_ = use_full_features;
  value_from_black_ = value_from_black;
  feature_size_ = use_full_features_ ? kInputFeatures : 18;
  host_buf_.resize(max_batch_size_ * int{kNumRvts} * feature_size_);
}

void TensorEngine::BuildFromOnnx(std::string onnx_path, std::string save_path) {
  std::cerr << "building engine ... ";

  Logger g_logger;
  nvinfer1::IBuilder *builder = nvinfer1::createInferBuilder(g_logger);
  const auto explicit_batch =
      1U << static_cast<int>(
          nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
  nvinfer1::INetworkDefinition *network =
      builder->createNetworkV2(explicit_batch);
  auto parser = nvonnxparser::createParser(*network, g_logger);
  bool parse_ok = parser->parseFromFile(
      onnx_path.c_str(),
      static_cast<int>(nvinfer1::ILogger::Severity::kERROR) /* kWARNING */);
  if (!parse_ok) {
    std::cerr << "[ERROR] File not found: " << onnx_path << std::endl;
    if (Options["rule"].get_int() == kJapanese) {
      std::cerr << "Use '--model_path' and '--validate_model_path' option to "
                   "specify a non-default engine file."
                << std::endl;
    } else {
      std::cerr
          << "Use '--model_path' option to specify a non-default engine file."
          << std::endl;
    }
    exit(1);
  }

  int batch_size = Options["batch_size"].get_int();
  builder->setMaxBatchSize(batch_size);
  nvinfer1::IBuilderConfig *config = builder->createBuilderConfig();
  config->setMaxWorkspaceSize(uint32_t{1} << 28);
  if (builder->platformHasFastFp16()) {
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
  }

  nvinfer1::Dims input_dims = network->getInput(0)->getDimensions();
  std::string inputs_name = network->getInput(0)->getName();

  nvinfer1::IOptimizationProfile *profile =
      builder->createOptimizationProfile();
  profile->setDimensions(inputs_name.c_str(),
                         nvinfer1::OptProfileSelector::kMIN,
                         nvinfer1::Dims3(batch_size, kInputFeatures, kNumRvts));
  profile->setDimensions(inputs_name.c_str(),
                         nvinfer1::OptProfileSelector::kMAX,
                         nvinfer1::Dims3(batch_size, kInputFeatures, kNumRvts));
  profile->setDimensions(inputs_name.c_str(),
                         nvinfer1::OptProfileSelector::kOPT,
                         nvinfer1::Dims3(batch_size, kInputFeatures, kNumRvts));
  config->addOptimizationProfile(profile);
  engine_ = builder->buildEngineWithConfig(*network, *config);
  SetBufferIndex();

  context_ = engine_->createExecutionContext();
  if (input_dims.d[0] < 0) {
    input_dims.d[0] = batch_size;
    context_->setBindingDimensions(inputs_idx_, input_dims);
  }

  SaveSerializedEngine(save_path);

  builder->destroy();
  network->destroy();
  parser->destroy();
  config->destroy();

  std::cerr << "completed." << std::endl;
}

void TensorEngine::BuildFromUff(std::string uff_path, std::string save_path) {
  std::cerr << "building engine ... ";

  Logger g_logger;
  nvinfer1::IBuilder *builder = nvinfer1::createInferBuilder(g_logger);
  nvinfer1::INetworkDefinition *network = builder->createNetwork();

  auto parser = nvuffparser::createUffParser();
  parser->registerInput("inputs",
                        nvinfer1::DimsCHW(kInputFeatures, kBSize, kBSize),
                        nvuffparser::UffInputOrder::kNCHW);
  auto dtype = builder->platformHasFastFp16() ? nvinfer1::DataType::kHALF
                                              : nvinfer1::DataType::kFLOAT;

  bool parse_ok = parser->parse(uff_path.c_str(), *network, dtype);
  if (!parse_ok) {
    std::cerr << "[ERROR] File not found: " << uff_path << std::endl;
    if (Options["rule"].get_int() == kJapanese) {
      std::cerr << "Use '--model_path' and '--validate_model_path' option to "
                   "specify a non-default engine file."
                << std::endl;
    } else {
      std::cerr
          << "Use '--model_path' option to specify a non-default engine file."
          << std::endl;
    }
    exit(1);
  }
  int batch_size = Options["batch_size"].get_int();
  builder->setMaxBatchSize(std::max(batch_size, 8));
  nvinfer1::IBuilderConfig *config = builder->createBuilderConfig();
  config->setMaxWorkspaceSize(uint32_t{1} << 28);
  if (builder->platformHasFastFp16()) {
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
  }

  nvinfer1::Dims input_dims = network->getInput(0)->getDimensions();
  std::string inputs_name = network->getInput(0)->getName();

  engine_ = builder->buildEngineWithConfig(*network, *config);
  SetBufferIndex();
  SaveSerializedEngine(save_path);
  context_ = engine_->createExecutionContext();

  builder->destroy();
  network->destroy();
  parser->destroy();
  config->destroy();

  std::cerr << "completed." << std::endl;
}

void TensorEngine::LoadEngine(std::string model_path) {
  std::string base_path = model_path;
  if (base_path.find(".engine") != std::string::npos) {
    base_path = base_path.substr(0, base_path.size() - 7);
  } else if (base_path.find(".onnx") != std::string::npos) {
    base_path = base_path.substr(0, base_path.size() - 5);
  } else if (base_path.find(".uff") != std::string::npos) {
    base_path = base_path.substr(0, base_path.size() - 4);
  }
  std::string onnx_path = base_path + ".onnx";
  std::string uff_path = base_path + ".uff";
  std::string engine_path = base_path + ".engine";

  std::ifstream ifs(engine_path, std::ios::binary);

  if (ifs.is_open()) {
    std::ostringstream model_ss(std::ios::binary);
    model_ss << ifs.rdbuf();
    std::string model_str = model_ss.str();

    Logger g_logger;
    runtime_ = nvinfer1::createInferRuntime(g_logger);
    engine_ = runtime_->deserializeCudaEngine(model_str.c_str(),
                                              model_str.size(), nullptr);

    if (engine_->getMaxBatchSize() < Options["batch_size"].get_int()) {
      std::cerr << "Max batch size of the sirialized model"
                   "is smaller than batch_size."
                << std::endl;
      if (use_uff_)
        BuildFromUff(uff_path, engine_path);
      else
        BuildFromOnnx(onnx_path, engine_path);
    }

    SetBufferIndex();
    context_ = engine_->createExecutionContext();

    auto input_dims = engine_->getBindingDimensions(inputs_idx_);
    if (input_dims.d[0] < 0) {
      input_dims.d[0] = Options["batch_size"].get_int();
      context_->setBindingDimensions(0, input_dims);
    }
  } else {
    if (use_uff_)
      BuildFromUff(uff_path, engine_path);
    else
      BuildFromOnnx(onnx_path, engine_path);
  }
}

bool TensorEngine::Infer(const Feature &ft, ValueAndProb *vp,
                         int symmetry_idx) {
  int inputs_size = kNumRvts * feature_size_;
  float *inputs_itr = host_buf_.data();

  if (symmetry_idx == kNumSymmetry) symmetry_idx = RandSymmetry();
  ft.Copy(inputs_itr, use_full_features_, symmetry_idx);

  cudaMemcpy(device_bufs_[inputs_idx_], host_buf_.data(),
             inputs_size * sizeof(float), cudaMemcpyHostToDevice);

  if (use_uff_)
    context_->execute(max_batch_size_, device_bufs_.data());
  else
    context_->executeV2(device_bufs_.data());

  std::vector<float> policy(kNumRvts);
  std::vector<float> value(1);
  cudaMemcpy(policy.data(), device_bufs_[policy_idx_],
             policy.size() * sizeof(float), cudaMemcpyDeviceToHost);
  cudaMemcpy(value.data(), device_bufs_[value_idx_],
             value.size() * sizeof(float), cudaMemcpyDeviceToHost);

  if (symmetry_idx == 0) {
    std::copy_n(policy.begin(), kNumRvts, vp->prob.begin());
  } else {
    auto p_itr = policy.begin();
    for (int j = 0; j < kNumRvts; ++j) {
      vp->prob[rv2sym(j, symmetry_idx)] = *p_itr;
      ++p_itr;
    }
  }
  if (value_from_black_ && ft.next_side() == kWhite) value[0] *= -1;
  vp->value = value[0];

  return true;
}

bool TensorEngine::Infer(std::vector<std::shared_ptr<SyncedEntry>> *entries,
                         int symmetry_idx) {
  int batch_size = entries->size();

  if (batch_size <= 0 || batch_size > max_batch_size_) return false;

  int inputs_size = batch_size * kNumRvts * feature_size_;
  float *inputs_itr = host_buf_.data();

  std::vector<int> symmetries(batch_size);
  if (symmetry_idx != kNumSymmetry) {
    for (int i = 0; i < batch_size; ++i) symmetries[i] = symmetry_idx;
  } else {  // symmetry_idx == kNumSymmetry
    for (int i = 0; i < batch_size; ++i) symmetries[i] = RandSymmetry();
  }

  for (int i = 0; i < batch_size; ++i)
    inputs_itr =
        (*entries)[i]->ft.Copy(inputs_itr, use_full_features_, symmetries[i]);

  cudaMemcpy(device_bufs_[inputs_idx_], host_buf_.data(),
             inputs_size * sizeof(float), cudaMemcpyHostToDevice);

  if (use_uff_)
    context_->execute(max_batch_size_, device_bufs_.data());
  else
    context_->executeV2(device_bufs_.data());

  std::vector<float> policy(batch_size * kNumRvts);
  std::vector<float> value(batch_size);
  cudaMemcpy(policy.data(), device_bufs_[policy_idx_],
             policy.size() * sizeof(float), cudaMemcpyDeviceToHost);
  cudaMemcpy(value.data(), device_bufs_[value_idx_],
             value.size() * sizeof(float), cudaMemcpyDeviceToHost);

  auto p_itr = policy.begin();

  for (int i = 0; i < batch_size; ++i) {
    if (symmetry_idx == 0) {
      std::copy_n(p_itr, kNumRvts, (*entries)[i]->vp.prob.begin());
      std::advance(p_itr, kNumRvts);
    } else {
      for (int j = 0; j < kNumRvts; ++j) {
        (*entries)[i]->vp.prob[rv2sym(j, symmetries[i])] = *p_itr;
        ++p_itr;
      }
    }

    if (value_from_black_ && (*entries)[i]->ft.next_side() == kWhite)
      value[i] *= -1;
    (*entries)[i]->vp.value = value[i];
  }

  return true;
}

bool TensorEngine::Infer(std::vector<RouteEntry> *entries, int symmetry_idx) {
  int batch_size = entries->size();
  if (batch_size <= 0) return false;

  ASSERT_LV3(batch_size <= max_batch_size_);

  int inputs_size = batch_size * kNumRvts * feature_size_;
  float *inputs_itr = host_buf_.data();

  std::vector<int> symmetries(batch_size);
  if (symmetry_idx != kNumSymmetry) {
    for (int i = 0; i < batch_size; ++i) symmetries[i] = symmetry_idx;
  } else {  // symmetry_idx == 8
    for (int i = 0; i < batch_size; ++i) symmetries[i] = RandSymmetry();
  }

  for (int i = 0; i < batch_size; ++i)
    inputs_itr =
        (*entries)[i].ft.Copy(inputs_itr, use_full_features_, symmetries[i]);

  cudaMemcpy(device_bufs_[inputs_idx_], host_buf_.data(),
             inputs_size * sizeof(float), cudaMemcpyHostToDevice);

  if (use_uff_)
    context_->execute(max_batch_size_, device_bufs_.data());
  else
    context_->executeV2(device_bufs_.data());

  std::vector<float> policy(batch_size * kNumRvts);
  std::vector<float> value(batch_size);
  cudaMemcpy(policy.data(), device_bufs_[policy_idx_],
             policy.size() * sizeof(float), cudaMemcpyDeviceToHost);
  cudaMemcpy(value.data(), device_bufs_[value_idx_],
             value.size() * sizeof(float), cudaMemcpyDeviceToHost);

  auto p_itr = policy.begin();

  for (int i = 0; i < batch_size; ++i) {
    if (symmetry_idx == 0) {
      std::copy_n(p_itr, kNumRvts, (*entries)[i].vp.prob.begin());
      std::advance(p_itr, kNumRvts);
    } else {
      for (int j = 0; j < kNumRvts; ++j) {
        (*entries)[i].vp.prob[rv2sym(j, symmetries[i])] = *p_itr;
        ++p_itr;
      }
    }

    if (value_from_black_ && (*entries)[i].ft.next_side() == kWhite)
      value[i] *= -1;
    (*entries)[i].vp.value = value[i];
  }

  return true;
}
