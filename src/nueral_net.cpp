#include "nueral_net.h"

using namespace tensorflow;
using std::string;
using std::cerr;
using std::endl;

int cfg_sym_idx = 0;
constexpr int feature_cnt = 49;

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
	Tensor pn_x(DT_FLOAT, TensorShape({ft_cnt, BVCNT, feature_cnt}));
	auto x_eigen = pn_x.tensor<float, 3>();
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

	inputs = {{"x_input", pn_x},{"temp", sm_temp}};
	sess->Run(inputs,{"fc/yfc"},{}, &outputs);

	auto output_v = outputs[0].matrix<float>();

	for(int i=0;i<ft_cnt;++i){
		std::array<double, EBVCNT> prob;
		prob.fill(0.0);

		for(int j=0;j<BVCNT;++j){
			int v = rtoe[j];

			if(sym_idx == 0) prob[v] = (double)output_v(i, j);
			else if(sym_idx > 7) prob[v] = (double)output_v(i, sv.rv[sym_idxs[i]][j][1]);
			else prob[v] = (double)output_v(i, sv.rv[sym_idx][j][1]);

			// 3線より中央側のシチョウを逃げる手の確率を下げる
			// Reduce probability of moves escaping from Ladder.
			if(ft_list[i].feature[j][46] != 0 && DistEdge(v) > 2) prob[v] *= 0.001;
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
	Tensor vn_x(DT_FLOAT, TensorShape({ft_cnt, BVCNT, feature_cnt}));
	Tensor vn_c(DT_FLOAT, TensorShape({ft_cnt, BVCNT, 1}));
	auto x_eigen = vn_x.tensor<float, 3>();
	auto c_eigen = vn_c.tensor<float, 3>();

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
			c_eigen(i,j,0) = (float)ft_list[i].color;
		}
	}

	inputs = {{"vn_x", vn_x},{"vn_c", vn_c}};
	sess->Run(inputs,{"fc2/yfc"},{}, &outputs);
//	sess->Run(inputs,{"v_fc2/yfc"},{}, &outputs);

	auto out_eigen = outputs[0].matrix<float>();
	for(int i=0;i<ft_cnt;++i){
		eval_list.push_back((float)out_eigen(i));
	}

}
