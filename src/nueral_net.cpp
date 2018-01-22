#include "nueral_net.h"

using namespace tensorflow;
using std::string;
using std::cerr;
using std::endl;

int cfg_sym_idx = 0;

#ifdef USE_52FEATURE
	constexpr int feature_cnt = 52;
#else
	constexpr int feature_cnt = 49;
#endif

/**
 *  着手確率を予測するネットワーク
 *  Calculate probability distribution with the Policy Network.
 */
void PolicyNet(Session* sess, std::vector<FeedTensor>& ft_list,
		std::vector<std::array<double,EBVCNT>>& prob_list,
		double temp, int sym_idx)
{

	prob_list.clear();
	int ft_cnt = (int)ft_list.size();
	Tensor x(DT_FLOAT, TensorShape({ft_cnt, BVCNT, feature_cnt}));
	auto x_eigen = x.tensor<float, 3>();
	Tensor sm_temp(DT_FLOAT, TensorShape());
	sm_temp.scalar<float>()() = (float)temp;

	std::vector<std::pair<string, Tensor>> inputs;
	std::vector<Tensor> outputs;

	std::vector<int> sym_idxs;
	if(sym_idx > 7){
		for(int i=0;i<ft_cnt;++i){
			sym_idxs.push_back(mt_int8(mt_32));
		}
	}

	for(int i=0;i<ft_cnt;++i){
		for(int j=0;j<BVCNT;++j){
			for(int k=0;k<feature_cnt;++k){

				if(sym_idx == 0) x_eigen(i, j, k) = ft_list[i].feature[j][k];
				else if(sym_idx > 7) x_eigen(i, j, k) = ft_list[i].feature[sv.rv[sym_idxs[i]][j][0]][k];
				else x_eigen(i, j, k) = ft_list[i].feature[sv.rv[sym_idx][j][0]][k];

			}
		}
	}
#ifdef USE_52FEATURE
	inputs = {{"x", x},{"temp", sm_temp}};
	sess->Run(inputs,{"pfc/policy"},{}, &outputs);
#else
	inputs = {{"x_input", x},{"temp", sm_temp}};
	sess->Run(inputs,{"fc/yfc"},{}, &outputs);
#endif

	auto policy = outputs[0].matrix<float>();

	for(int i=0;i<ft_cnt;++i){
		std::array<double, EBVCNT> prob;
		prob.fill(0.0);

		for(int j=0;j<BVCNT;++j){
			int v = rtoe[j];

			if(sym_idx == 0) prob[v] = (double)policy(i, j);
			else if(sym_idx > 7) prob[v] = (double)policy(i, sv.rv[sym_idxs[i]][j][1]);
			else prob[v] = (double)policy(i, sv.rv[sym_idx][j][1]);

			// 3線より中央側のシチョウを逃げる手の確率を下げる
			// Reduce probability of moves escaping from Ladder.
			if(ft_list[i].feature[j][LADDERESC] != 0 && DistEdge(v) > 2) prob[v] *= 0.001;
		}
		prob_list.push_back(prob);
	}

}


/**
 *  局面評価値を予測するネットワーク
 *  Calculate value of the board with the Value Network.
 */
void ValueNet(Session* sess, std::vector<FeedTensor>& ft_list,
		std::vector<float>& eval_list, int sym_idx)
{

	eval_list.clear();
	int ft_cnt = (int)ft_list.size();
	Tensor x(DT_FLOAT, TensorShape({ft_cnt, BVCNT, feature_cnt}));
	auto x_eigen = x.tensor<float, 3>();
#ifndef USE_52FEATURE
	Tensor vn_c(DT_FLOAT, TensorShape({ft_cnt, BVCNT, 1}));
	auto c_eigen = vn_c.tensor<float, 3>();
#endif

	std::vector<std::pair<string, Tensor>> inputs;
	std::vector<Tensor> outputs;
	int sym_idx_rand = mt_int8(mt_32);

	for(int i=0;i<ft_cnt;++i){
		for(int j=0;j<BVCNT;++j){
			for(int k=0;k<feature_cnt;++k){
				if(sym_idx == 0) x_eigen(i, j, k) = ft_list[i].feature[j][k];
				else if(sym_idx > 7) x_eigen(i, j, k) = ft_list[i].feature[sv.rv[sym_idx_rand][j][0]][k];
				else x_eigen(i, j, k) = ft_list[i].feature[sv.rv[sym_idx][j][0]][k];
			}
#ifndef USE_52FEATURE
			c_eigen(i,j,0) = (float)ft_list[i].color;
#endif
		}
	}

#ifdef USE_52FEATURE
	inputs = {{"x", x},};
	sess->Run(inputs,{"vfc/value"},{}, &outputs);
#else
	inputs = {{"vn_x", x},{"vn_c", vn_c}};
	sess->Run(inputs,{"fc2/yfc"},{}, &outputs);
#endif // USE_52FEATURE

	auto value = outputs[0].matrix<float>();
	for(int i=0;i<ft_cnt;++i){
		eval_list.push_back((float)value(i));
	}

}
