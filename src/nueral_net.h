#pragma once

#ifdef _WIN32
	#define COMPILER_MSVC
	#define NOMINMAX
#endif

#include <vector>
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/graph/default_device.h"
#include "tensorflow/core/graph/graph_def_builder.h"

#include "feed_tensor.h"


void PolicyNet(tensorflow::Session* sess,
		std::vector<FeedTensor>& ft_list,
		std::vector<std::array<double,EBVCNT>>& prob_list,
		double temp=0.67, int sym_idx=0);

void ValueNet(tensorflow::Session* sess, std::vector<FeedTensor>& ft_list,
		std::vector<float>& eval_list, int sym_idx=0);

extern int cfg_sym_idx;
