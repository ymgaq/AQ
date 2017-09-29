#include <algorithm>
#include <thread>
#include <stdarg.h>
#include <iostream>
#include <fstream>

#include "search.h"

using std::string;
using std::cerr;
using std::endl;


double cfg_main_time = 0;
double cfg_byoyomi = 5;
int cfg_thread_cnt = 4;
int cfg_gpu_cnt = 1;
double cfg_komi = 7.5;

bool self_match = false;
bool is_master = false;
bool is_worker = false;
int cfg_worker_cnt = 1;
bool cfg_mimic = false;
bool never_resign = false;
std::string resume_sgf_path = "";
std::string pb_dir = "";


/**
 *  exp�̋ߎ��֐�
 *  �e�C���[�W�Je^x = lim[n->inf](1+x/n)^n�𗘗p
 *  n=256=2^8�̏ꍇ�𗘗p
 *
 *  Approximate function of std::exp.
 *  e^x = lim[n->inf](1+x/n)^n
 *  where n = 2^8 = 256.
 */
inline double exp256(double x){

	double x1 = 1.0 + x / 256.0;
	x1 *= x1; x1 *= x1; x1 *= x1; x1 *= x1;
	x1 *= x1; x1 *= x1; x1 *= x1; x1 *= x1;
	return x1;

}


/**
 *  �アCAS���J��Ԃ��ĉ��Z���� double�p
 *  Repeat weak CAS for floating-type addition.
 */
template<typename T>
T FetchAdd(std::atomic<T> *obj, T arg) {
	T expected = obj->load();
	while (!atomic_compare_exchange_weak(obj, &expected, expected + arg))
		;
	return expected;
};


Tree::Tree(){

	Tree::Clear();
	std::vector<std::string> sl_path;
	std::vector<std::string> vl_path;
	std::stringstream ss;

	for(int i=0;i<gpu_cnt;++i){
		ss.str("");
		if(pb_dir != "") ss << pb_dir << "/";
		ss << "sl_" << i << ".pb";
		sl_path.push_back(ss.str());

		ss.str("");
		if(pb_dir != "") ss << pb_dir << "/";
		ss << "vl_" << i << ".pb";
		vl_path.push_back(ss.str());
	}

	SetGPU(sl_path, vl_path);

}


Tree::Tree(std::vector<std::string>& sl_path, std::vector<std::string>& vl_path){

	Tree::Clear();
	SetGPU(sl_path, vl_path);

}


void Tree::SetGPU(std::vector<std::string>& sl_path, std::vector<std::string>& vl_path){

	{
		using namespace tensorflow;

		sess_policy.clear();
		sess_value.clear();

		for(int i=0;i<gpu_cnt;++i){

			Session* sess_p(NewSession(SessionOptions()));
			GraphDef graph_p;
			ReadBinaryProto(Env::Default(), sl_path[i], &graph_p);
			sess_p->Create(graph_p);

			Session* sess_v(NewSession(SessionOptions()));
			GraphDef graph_v;
			ReadBinaryProto(Env::Default(), vl_path[i], &graph_v);
			sess_v->Create(graph_v);

			sess_policy.push_back(sess_p);
			sess_value.push_back(sess_v);

		}
	}

}


void Tree::Clear(){

	thread_cnt = cfg_thread_cnt;
	main_time = cfg_main_time;
	byoyomi = cfg_byoyomi;
	komi = cfg_komi;
	gpu_cnt = cfg_gpu_cnt;
	sym_idx = cfg_sym_idx;

	vloss_cnt = 3;
	lambda = 0.7;
	cp = 3.0;
	policy_temp = 0.7;
	expand_cnt = 18;

	Tree::InitBoard();

	node.clear();
	node.resize(node_limit);

	log_file = NULL;
	stop_think = false;

}


void Tree::InitBoard(){

	for(auto& nd: node) nd.Clear();
	node_hash_list.clear();
	node_cnt = 0;
	node_depth = 0;
	root_node_idx = 0;
	value_que.clear();
	policy_que.clear();
	value_que_cnt = 0;
	policy_que_cnt = 0;
	eval_policy_cnt = 0;
	eval_value_cnt = 0;

	lgr.Clear();
	stat.Clear();

	left_time = main_time;
	extension_cnt = 0;
	move_cnt = 0;

}


/**
 *  policy_que�ɋǖʂ�ǉ�����
 *  Add new entry to policy_que.
 */
void Tree::AddPolicyQue(int node_idx, Board& b){

	PolicyEntry pe;
	pe.node_idx = node_idx;
	pe.ft.Set(b, PASS);
	{
		std::lock_guard<std::mutex> lock(mtx_pque);
		policy_que.push_back(pe);
		++policy_que_cnt;
	}

}


/**
 *  value_que�ɋǖʂ�ǉ�����
 *  Add new entry to value_que.
 */
void Tree::AddValueQue(std::vector<std::pair<int,int>>& upper_list, Board& b){

	ValueEntry ve;
	ve.depth = std::min(128, (int)upper_list.size());
	auto ritr = upper_list.rbegin();
	for(int i=0, n=ve.depth;i<n;++i){
		ve.node_idx[i] = ritr->first;
		ve.child_idx[i] = ritr->second;
		++ritr;
	}

	{
		std::lock_guard<std::mutex> lock(mtx_vque);
		auto itr = find(value_que.begin(), value_que.end(), ve);
		if(itr != value_que.end()){
			itr->request_cnt++;
			return;
		}
	}

	ve.ft.Set(b, PASS);

	{
		std::lock_guard<std::mutex> lock(mtx_vque);
		auto itr = find(value_que.begin(), value_que.end(), ve);
		if(itr == value_que.end()){
			value_que.push_back(ve);
			++value_que_cnt;
		}
		else{
			itr->request_cnt++;
		}
	}

}


/**
 *  �m�[�h��V�K�쐬����
 *  ���ɓo�^����Ă���Ƃ��͂���index��Ԃ�
 *
 *  Create a new Node and returns the index.
 */
int Tree::CreateNode(Board& b) {

	// ���͔Ֆʂ̃n�b�V�������߂�. Calculate board hash.
	int64 hash_b = BoardHash(b);
	int node_idx;

	{
		Node *pn;
		{
			std::lock_guard<std::mutex> lock(mtx_node);

			if(node_hash_list.find(hash_b) != node_hash_list.end()){

				// �ʂ̃X���b�h�ł��̋ǖʃm�[�h�𐶐���
				// Return -1 if another thread is creating this node.
				if(node[node_hash_list[hash_b]].is_creating) return -1;

				// ���ɓo�^�ς̂Ƃ��A����index��Ԃ�
				// Return the index if the key is already registered.
				else{
					// �Ֆʃn�b�V�������ꂩ�m�F
					// Confirm whether the board hashes are the same.
					if(node[node_hash_list[hash_b]].hash == hash_b){
						return node_hash_list[hash_b];
					}
				}
			}

			node_idx = int(hash_b % (int64)node_limit);
			node_idx = std::max(0, std::min(node_idx, node_limit - 1));
			pn = &node[node_idx];

			// �ʂ̓o�^������Ă���or�쐬���̂Ƃ��Anode_idx��ύX
			// Update node_idx if another node is registered or been creating.
			while (pn->child_cnt != 0 || pn->is_creating) {
				++node_idx;
				if(node_idx >= node_limit) node_idx = 0;
				pn = &node[node_idx];
			}

			node_hash_list[hash_b] = node_idx;
			pn->is_creating = true;
			++node_cnt;
		}

		pn->child_cnt = 0;
		for(int i=0;i<BVCNT+1;++i) pn->prob_order[i] = i;
		pn->total_game_cnt = 0;
		pn->rollout_cnt = 0;
		pn->value_cnt = 0;
		pn->rollout_win = 0.0;
		pn->value_win = 0.0;
		pn->is_visit = false;
		pn->is_policy_eval = false;
		pn->hash = hash_b;
		pn->pl = b.my;
		pn->prev_move[0] = b.prev_move[b.her];
		pn->prev_move[1] = b.prev_move[b.my];

		pn->prev_ptn[0] = b.prev_ptn[0].bf;
		pn->prev_ptn[1] = b.prev_ptn[1].bf;

		int my = b.my;
		int her = b.her;
		memcpy(pn->w_prob[my], b.w_prob[my], sizeof(pn->w_prob[my]));
		memcpy(pn->w_prob[her], b.w_prob[her], sizeof(pn->w_prob[her]));

		int prev_move_[3] = {b.prev_move[her], b.prev_move[my], PASS};
		if(b.move_cnt >= 3) prev_move_[2] = b.move_history[b.move_cnt - 3];

		for(int i=0, n=b.empty_cnt;i<n;++i){
			int v = b.empty[i];
			double my_dp = 0.0;
			double her_dp = 0.0;
			my_dp += prob_dist[0][DistBetween(v, prev_move_[0])][0];
			my_dp += prob_dist[1][DistBetween(v, prev_move_[1])][0];
			her_dp += prob_dist[0][DistBetween(v, prev_move_[2])][0];
			her_dp += prob_dist[1][DistBetween(v, prev_move_[1])][0];
			Pattern3x3 ptn12 = b.ptn[v];
			ptn12.SetColor(8, b.color[std::min(EBVCNT - 1, (v + EBSIZE * 2))]);
			ptn12.SetColor(9, b.color[v + 2]);
			ptn12.SetColor(10, b.color[std::max(0, (v - EBSIZE * 2))]);
			ptn12.SetColor(11, b.color[v - 2]);
			if(prob_ptn12.find(ptn12.bf) != prob_ptn12.end()){
				my_dp = prob_ptn12[ptn12.bf][my];
				her_dp = prob_ptn12[ptn12.bf][her];
			}

			if(pn->w_prob[my][v] == 0){
				pn->prob_roll[my][v] = 0.0;
			}
			else{
				FetchAdd(&pn->w_prob[my][v], my_dp);
				pn->prob_roll[my][v] = exp256(pn->w_prob[my][v]);
			}
			if(pn->w_prob[her][v] == 0){
				pn->prob_roll[her][v] = 0.0;
			}
			else{
				FetchAdd(&pn->w_prob[her][v], her_dp);
				pn->prob_roll[her][v] = exp256(pn->w_prob[her][v]);
			}

		}

		memcpy(pn->prob, pn->prob_roll[my], sizeof(pn->prob));
		double sum_prob = 0.0;
		for(int i=0;i<EBVCNT;++i) sum_prob += pn->prob[i].load();
		double inv_sum = 1.0;
		if(sum_prob != 0.0) inv_sum = 1.0 / sum_prob;

		int v;
		std::vector<std::pair<double, int>> prob_list;
		for (int i = 0; i < b.empty_cnt; ++i) {
			v = b.empty[i];
			if(	!b.IsLegal(b.my,v)	||
				b.IsEyeShape(b.my,v)	||
				b.IsSeki(v)) 	continue;

			prob_list.push_back(std::make_pair(pn->prob[v].load() * inv_sum, v));
		}
		std::sort(prob_list.begin(), prob_list.end(), std::greater<std::pair<double,int>>());

		for (int i=0, n=(int)prob_list.size(); i<n; ++i) {
			Child new_child;
			new_child.move = prob_list[i].second;
			new_child.prob = prob_list[i].first;

			// �q�ǖʂ�o�^. Register the child.
			pn->children[i] = new_child;
			pn->child_cnt++;
		}

		// PASS
		Child new_child;
		pn->children[pn->child_cnt] = new_child;
		pn->child_cnt++;

		pn->is_creating = false;
	}

	AddPolicyQue(node_idx, b);

	// �쐬�����m�[�h��index��Ԃ�
	// Return the Node index.
	return node_idx;

}


/**
 *  �m�[�h�̊m�����z��policy net�ɒu��������
 *  Update probability of node with that evaluated
 *  by the policy network.
 */
void Tree::UpdateNodeProb(int node_idx, std::array<double, EBVCNT>& prob_list) {

	// 1. node[node_idx]�̊m�����z��prob_list�ɍX�V
	//    Replace probability of node[node_idx] with prob_list.
	Node* pn = &node[node_idx];
	for(int i=0;i<BVCNT;++i){
		int v = rtoe[i];
		pn->prob[v] = prob_list[v];
	}

	// 2. (prob,idx)�̃y�A���~���Ƀ\�[�g���Aprob_order���X�V
	//    Update prob_order after sorting.
	int child_cnt = pn->child_cnt.load();
	std::vector<std::pair<double, int>> prob_idx_pair;
	for (int i=0,n=child_cnt-1; i<n; ++i) {
		pn->children[i].prob = (double)pn->prob[pn->children[i].move];
		prob_idx_pair.push_back(std::make_pair((double)pn->children[i].prob, i));
	}
	prob_idx_pair.push_back(std::make_pair(0.0, child_cnt-1));
	std::sort(prob_idx_pair.begin(), prob_idx_pair.end(),  std::greater<std::pair<double, int>>());
	for(int i=0, n=child_cnt;i<n;++i){
		pn->prob_order[i] = prob_idx_pair[i].second;
	}

	// 3. LGR�̒����lgr.policy�ɓo�^
	//    Register LGR move in lgr.policy.
	if(lambda != 1.0){
		std::array<int,4> lgr_seed = {pn->prev_ptn[0], pn->prev_move[0], pn->prev_ptn[1], pn->prev_move[1]};
		if(pn->children[pn->prob_order[0]].move < PASS)
		{
			std::lock_guard<std::mutex> lock(mtx_lgr);
			lgr.policy[pn->pl][lgr_seed] = pn->children[pn->prob_order[0]].move;
		}
	}

	pn->is_policy_eval = true;

}


/**
 *  node[node_idx]�ȉ��ɘA�Ȃ�m�[�h��index�����W����
 *  Collect all indexes of nodes under node[node_idx].
 */
int Tree::CollectNodeIndex(int node_idx, int depth, std::unordered_set<int>& node_list) {

	int max_depth = depth;
	++depth;

	node_list.insert(node_idx);
	Node* pn = &node[node_idx];
	pn->is_visit = false;

	if(pn->child_cnt == 0) return max_depth;
	else if (depth > 720) return max_depth;

	for(int i=0, n=pn->child_cnt; i<n; ++i){
		Child* pc = &pn->children[i];
		if(	pc->is_next &&
			((int64)pc->next_hash == (int64)node[(int)pc->next_idx].hash))
		{
			int prev_move = pn->prev_move[0];
			int next_move = pc->move;
			if(	!(prev_move == PASS && next_move == PASS) &&
				node_list.find((int)pc->next_idx) == node_list.end())
			{
				// ���̃m�[�h�����݂���Ƃ��͍ċA�Ăяo��.
				// Call recursively if next node exits.
				int tmp_depth = CollectNodeIndex((int)pc->next_idx, depth, node_list);
				if(tmp_depth > max_depth) max_depth = tmp_depth;
			}
		}
	}

	return max_depth;

}


/**
 *  �m�[�h�ȉ��̃C���f�b�N�X�𒲂ׁA����ȊO������
 *  Delete indexes other than that connected to the root node. *
 */
void Tree::DeleteNodeIndex(int node_idx){

	// 1. node_idx�Ɍq����C���f�b�N�X�𒲂ׁA����ȊO������
	//    Delete indexes other than that connected to the root node.
	std::unordered_set<int> node_list;
	CollectNodeIndex(node_idx, 0, node_list);

	{
		std::lock_guard<std::mutex> lock(mtx_node);
		for (int i = 0; i < node_limit; ++i) {
			if (node[i].child_cnt != 0) {
				if (node_list.find(i) == node_list.end()) {
					node_hash_list.erase((int64)node[i].hash);

					node[i].Clear();
					--node_cnt;
				}
				else node[i].is_visit = false;
			}
		}
	}

	// 2. policy_que����폜
	//    Remove entries from policy_que.
	std::deque<PolicyEntry> remain_pque;
	for(auto i:policy_que){
		if(node_list.find(i.node_idx) != node_list.end()){
			remain_pque.push_back(i);
		}
	}
	policy_que.swap(remain_pque);
	policy_que_cnt = (int)policy_que.size();

	// 3. value_que����폜
	//    Remove entries from value_que.
	std::deque<ValueEntry> remain_vque;
	for(auto i:value_que){
		if(node_list.find(i.node_idx[0]) != node_list.end()){

			bool is_remain = true;

			for(int j=1, j_max=i.depth-1;j<j_max;++j){
				if(	node_list.find(i.node_idx[j]) == node_list.end())
				{
					// �o�H�ɍ폜���ꂽ�m�[�h���܂܂��Ƃ��͍폜
					is_remain = false;
					break;
				}
			}

			if(is_remain){
				i.depth = std::max(1, i.depth-1);
				remain_vque.push_back(i);
			}

			//remain_vque.push_back(i);
		}
	}
	value_que.swap(remain_vque);
	value_que_cnt = (int)value_que.size();

	if (node_cnt < 0) node_cnt = 1;

}

/**
 *  ���[�g�m�[�h����͔Ֆʂ̂��̂ɕύX����
 *  Update the root node with the input board.
 */
int Tree::UpdateRootNode(Board&b){

	int node_idx = CreateNode(b);

	if(root_node_idx != node_idx) DeleteNodeIndex(node_idx);
	root_node_idx = node_idx;
	move_cnt = b.move_cnt;

	return node_idx;

}


/**
 *  �T���؂̒��ŁA�e�m�[�h �� �q�m�[�h�̈ړ���1��s��
 *  �q�m�[�h�����݂��Ȃ��Ƃ��A�V�K�ɍ쐬���邩�𔻒f����
 *  ���[�ł̓v���C�A�E�g�EValueNet�]�����s���A���̌��ʂ�Ԃ�
 *
 *  Proceed to a child node from the parent node.
 *  Create new node if there is no corresponding child node.
 *  At the leaf node, rollout and evaluation are performed,
 *  and the results are returned.
 */
double Tree::SearchBranch(Board& b, int node_idx, double& value_result,
		std::vector<std::pair<int,int>>& serch_route, LGR& lgr_, Statistics& stat_)
{

	Node *pn = &node[node_idx];
	Child *pc;
	bool use_rollout = (lambda != 1.0);

	// 1. action value����ԍ������I��
	//    Choose the move with the highest action value.
	int max_idx = 0;
	double max_avalue = -128;

	double pn_rollout_rate = (double)pn->rollout_win/((double)pn->rollout_cnt + 0.01);
	double pn_value_rate = (double)pn->value_win/((double)pn->value_cnt + 0.01);

	double rollout_cnt, rollout_win, value_cnt, value_win;
	double rollout_rate, value_rate, game_cnt, rate, action_value;

	for (int i=0, n=(int)pn->child_cnt;i<n;++i) {

		// a. �m�����������ɒ��ׂ�
		//    Search in descending order of probability.
		int child_idx = pn->prob_order[i];
		pc = &pn->children[child_idx];
		int next_idx = (int)pc->next_idx;

		// b. �q�m�[�h���W�J�ς̏ꍇ�͂��̏��𗘗p����
		//    �Ȃ���Ό��݂̃m�[�h�̃u�����`�̏����g��
		//    Use the child node information if already expanded,
		//    otherwise use that of the child branch in the current
		//    node.
		if(pc->is_next &&
			next_idx >= 0 && next_idx < node_limit &&
			pc->next_hash == node[next_idx].hash)
		{
			// �q�m�[�h�̒T�����.
			// Search information in the child node.
			rollout_cnt = (double)node[next_idx].rollout_cnt;
			value_cnt = (double)node[next_idx].value_cnt;
			// ��Ԃ��ς��̂ŕ]���l�𔽓]
			// Reverse value as the turn changes.
			rollout_win = -(double)node[next_idx].rollout_win;
			value_win = -(double)node[next_idx].value_win;
		}
		else{
			// �u�����`�̒T�����.
			// Search information in the child branch.
			rollout_cnt = (double)pc->rollout_cnt;
			value_cnt = (double)pc->value_cnt;
			rollout_win = (double)pc->rollout_win;
			value_win = (double)pc->value_win;
		}

		// c. ���̎�̏������v�Z����
		//    Calculate winning rate of this move.
		if(rollout_cnt == 0) 	rollout_rate = pn_rollout_rate;
		else					rollout_rate = rollout_win / rollout_cnt;
		if(value_cnt == 0)		value_rate = pn_value_rate;
		else					value_rate = value_win / value_cnt;

		rate = (1-lambda) * rollout_rate + lambda * value_rate;

		// d. action value�����߂�
		//    Calculate action value.
		game_cnt = use_rollout? (double)pc->rollout_cnt : (double)pc->value_cnt;
		action_value = rate + cp * pc->prob * sqrt((double)pn->total_game_cnt) / (1 + game_cnt);

		// e. max_idx���X�V. Update max_idx.
		if (action_value > max_avalue) {
			max_avalue = action_value;
			max_idx = child_idx;
		}

	}

	// 2. action value���ő�̎��T������
	//    Search for the move with the maximum action value.
	pc = &pn->children[max_idx];
	int next_idx = std::max(0, std::min(node_limit - 1, (int)pc->next_idx));
	Node* npn = &node[next_idx]; // Next pointer of the node.

	serch_route.push_back(std::make_pair(node_idx, max_idx));
	int next_move = pc->move;
	int prev_move = b.prev_move[b.her];
	// ���s���ʂ�(0,�}1)��(-0.5,+0.5)�ɕ␳����o�C�A�X
	// Bias of winning rate that corrects result of (0, +/-1) to (-0.5, +0.5).
	double win_bias = (b.my == 0)? -0.5 : 0.5;

	// 3. LGR���X�V. Update LGR of policy.
	if(use_rollout && !pn->is_visit && pn->is_policy_eval)
	{
		// ���݂�root node�ɂȂ��Ă���ŏ��ɒT������Ƃ��ɍX�V
		// Update when searching first after becoming the current root node.
		pn->is_visit = true;
		int max_prob_idx = pn->prob_order[0];

		if(pn->children[max_prob_idx].move < PASS)
		{
			std::array<int,4> lgr_seed = {	pn->prev_ptn[0],
											pn->prev_move[0],
											pn->prev_ptn[1],
											pn->prev_move[1] };
			lgr_.policy[pn->pl][lgr_seed] = pn->children[max_prob_idx].move;
		}
	}

	// 4. �v���C�A�E�g�����邩�𔻒f
	//    Check if rollout is necessary.
	bool need_rollout = false;
	if(	(!pc->is_next && (std::max((int)pc->rollout_cnt, (int)pc->value_cnt) < expand_cnt))	||
		(next_move==PASS && prev_move==PASS) 			||
		(b.move_cnt > 720)								||
		(pn->child_cnt <= 1 && pc->is_next && npn->child_cnt <= 1))
	{
		need_rollout = true;
	}

	// 5. �m�[�h�W�J���邩�𔻒f
	//    Check whether the next node can be expanded.
	bool expand_node = false;
	if(	!need_rollout && (pc->is_next == false	||
		(int64)pc->next_hash != (int64)npn->hash) )
	{
		// �u���\��8�����܂��Ă���Ƃ��͐V�K�쐬���Ȃ�
		// New node is not chreated when the transposition table is filled by 80%.
		if(node_cnt < 0.8 * node_limit) expand_node = true;
		else need_rollout = true;
	}

	// 6. �ǖʂ�i�߂�. Play next_mvoe.
	b.PlayLegal(next_move);

	// 7. �m�[�h��W�J����
	//    Expand the next node.
	if(expand_node){
		int next_idx_exp = CreateNode(b);
		// ���܂ɒu���\�����ĕs����index��Ԃ��̂ł��̑΍�
		if(next_idx_exp < 0 || next_idx_exp >= node_limit) need_rollout = true;
		else{
			npn = &node[next_idx_exp];
			pc->next_idx = next_idx_exp;
			pc->next_hash = (int64)npn->hash;
			pc->is_next = true;

			// pc -> npn�֑΋Ǐ��𔽉f. Reflect game information.
			npn->total_game_cnt += (int)pc->rollout_cnt;
			npn->rollout_cnt += (int)pc->rollout_cnt;
			npn->value_cnt += (double)pc->value_cnt;
			// ��Ԃ��ς��̂ŕ]���l�𔽓]
			// Reverse evaluation value since turn changes.
			FetchAdd(&npn->rollout_win, -(double)pc->rollout_win);
			FetchAdd(&npn->value_win, -(double)pc->value_win);
		}
	}

	// 8. virtual loss��������. Add virtual loss.
	if(use_rollout){
		FetchAdd(&pc->rollout_win, -(double)vloss_cnt);
		pc->rollout_cnt += vloss_cnt;
		pn->total_game_cnt += vloss_cnt;
	}
	else{
		FetchAdd(&pc->value_win, -(double)vloss_cnt);
		pc->value_cnt += vloss_cnt;
		pn->total_game_cnt += vloss_cnt;
	}


	// 9. ���[�ł���΃v���C�A�E�g���s���A���̃m�[�h�����݂���ΒT����i�߂�
	//    Roll out if it is the leaf node, otherwise proceed to the next node.
	double rollout_result = 0.0;
	if (need_rollout)
	{
		// a-1. �ǖʂ����]���ł���΃L���[�ɒǉ�����
		//      Add into the queue if the board is not evaluated.
		value_result = 0;
		if(pc->is_value_eval){
			value_result = (double)pc->value;
		}
		else{
			AddValueQue(serch_route, b);
		}

		if(use_rollout){
			// b. �m�����z���R�s�[. Copy probability.
			if(pc->is_next && (int64)pc->next_hash == (int64)npn->hash){
				for(int i=0, n=b.empty_cnt;i<n;++i){
					int v = b.empty[i];
					b.w_prob[0][v] = (double)npn->w_prob[0][v];
					b.w_prob[1][v] = (double)npn->w_prob[1][v];
					b.prob[0][v] = (double)npn->prob_roll[0][v];
					b.prob[1][v] = (double)npn->prob_roll[1][v];
				}
				for(int i=0;i<BSIZE;++i){
					b.sum_prob_rank[0][i] = 0.0;
					b.sum_prob_rank[1][i] = 0.0;
					for(int j=0;j<BSIZE;++j){
						b.sum_prob_rank[0][i] += b.prob[0][rtoe[xytor[j][i]]];
						b.sum_prob_rank[1][i] += b.prob[1][rtoe[xytor[j][i]]];
					}
				}
			}
			// c. �v���C�A�E�g���A���ʂ�[-1.0, 1.0]�ɋK�i��
			//    Roll out and normalize the result to [-1.0, 1.0].
			rollout_result = -2.0 * ((double)PlayoutLGR(b, lgr_, komi) + win_bias);
		}
	}
	else{
		// a-2. ����node�ɐi��
		//      ��Ԃ��ς���Ă���̂ŁA���ʂ��������]������
		//      Proceed to the next node and reverse the results.
		rollout_result = -SearchBranch(b, (int)pc->next_idx, value_result, serch_route, lgr_, stat_);
		value_result *= -1.0;
	}

	// 10. virtual loss������&�����X�V
	//     Subtract virtual loss and update results.
	if(use_rollout){
		FetchAdd(&pc->rollout_win, (double)vloss_cnt + rollout_result);
		pc->rollout_cnt += 1 - vloss_cnt;
		pn->total_game_cnt += 1 - vloss_cnt;
		FetchAdd(&pn->rollout_win, rollout_result);
		pn->rollout_cnt += 1;
	}
	else{
		FetchAdd(&pc->value_win, (double)vloss_cnt);
		pc->value_cnt += -vloss_cnt;
		pn->total_game_cnt += 1 - vloss_cnt;
	}

	if(value_result != 0){
		FetchAdd(&pc->value_win, value_result);
		pc->value_cnt += 1;
		FetchAdd(&pn->value_win, value_result);
		pn->value_cnt += 1;
	}

	return rollout_result;

}


void PrintLog(std::ofstream* log_file, const char* output_text, ...){

	char buff[1024];

	va_list args;
	va_start(args, output_text);
	vsnprintf(buff, sizeof(buff), output_text, args);
	va_end(args);

	fprintf(stderr, buff);

	if(log_file != NULL){
		*log_file << buff;
	}

}

void SortChildren(Node* pn, std::vector<Child*>& child_list){

	std::vector<std::pair<int, int>> game_cnt_list;
	for (int i=0;i<pn->child_cnt;++i) {
		int game_cnt = std::max((int)pn->children[i].rollout_cnt, (int)pn->children[i].value_cnt);
		game_cnt_list.push_back(std::make_pair(game_cnt, i));
	}
	std::sort(game_cnt_list.begin(), game_cnt_list.end(), std::greater<std::pair<int, int>>());

	child_list.clear();
	for(int i=0;i<pn->child_cnt;++i){
		Child* pc = &pn->children[game_cnt_list[i].second];
		child_list.push_back(pc);
	}

}

double Tree::BranchRate(Child* pc){

	double winning_rate;
	if(pc->value_cnt == 0){
		winning_rate = pc->rollout_win/std::max(1.0, (double)pc->rollout_cnt);
	}
	else if(pc->rollout_cnt == 0){
		winning_rate = pc->value_win/std::max(1.0, (double)pc->value_cnt);
	}
	else{
		winning_rate = (1-lambda)*pc->rollout_win/std::max(1.0, (double)pc->rollout_cnt)
				+ lambda*pc->value_win/std::max(1.0, (double)pc->value_cnt);
	}
	winning_rate =	(winning_rate + 1) / 2;	// [0.0, 1.0]

	return winning_rate;

}

std::string CoordinateString(int v){

	string str_v;

	if(v == PASS) str_v = "PASS";
	else if(v < 0 || v > PASS || DistEdge(v) == 0) str_v = "VNULL";
	else{
		string str_x = "ABCDEFGHJKLMNOPQRST";
		str_v = str_x[etox[v] - 1];
		str_v += std::to_string(etoy[v]);
	}

	return str_v;

}

/**
 *  �T�����J��Ԃ��őP������߂�
 *  Repeat searching for the best move.
 */
int Tree::SearchTree(	Board& b, double time_limit, double& win_rate,
						bool is_errout, bool is_ponder)
{

	// 1. root node���X�V. Update root node.
	if(b.move_cnt == 0) Tree::InitBoard();
	int node_idx = CreateNode(b);
	bool is_root_changed = (root_node_idx != node_idx);
	root_node_idx = node_idx;
	move_cnt = b.move_cnt;
	eval_policy_cnt = 0;
	eval_value_cnt = 0;

	// 2. ���@�肪�Ȃ��Ƃ��̓p�X��Ԃ�
	//    Return pass if there is no legal move.
	Node *pn = &node[root_node_idx];
	if (pn->child_cnt <= 1){
		win_rate = 0.5;
		return PASS;
	}

	// 3. lambda��i�s�x�ɍ��킹�Ē��� (0.7 -> 0.3)
	//    Adjust lambda to progress.
	lambda = 0.7 - std::min(0.4, std::max(0.0, ((double)b.move_cnt - 160) / 500));
	//lambda = 1.0;

	// 4. root node�����]���̂Ƃ��A�m�����z��]������
	//    If the root node is not evaluated, evaluate the probability.
	if(!pn->is_policy_eval){
		std::vector<std::array<double,EBVCNT>> prob_list;
		FeedTensor ft;
		ft.Set(b, PASS);
		std::vector<FeedTensor> ft_list;
		ft_list.push_back(ft);

		PolicyNet(sess_policy[0], ft_list, prob_list, policy_temp, sym_idx);
		UpdateNodeProb(root_node_idx, prob_list[0]);
	}

	// 5. �q�m�[�h��T���񐔂��������Ƀ\�[�g
	//    Sort child nodes in descending order of search count.
	std::vector<Child*> rc;
	SortChildren(pn, rc);

	// 6. �T���񐔂��ő�̎q�m�[�h�̏��������߂�
	//    Calculate the winning percentage of pc0.
	win_rate = BranchRate(rc[0]);
	int rc0_game_cnt = std::max((int)rc[0]->value_cnt, (int)rc[0]->rollout_cnt);
	int rc1_game_cnt = std::max((int)rc[1]->value_cnt, (int)rc[1]->rollout_cnt);


	// 7-1. �������Ԃ��c�菭�Ȃ��Ƃ��͒T�����Ȃ�
	//      Return best move without searching when time is running out.
	if(!is_ponder &&
		time_limit == 0.0 &&
		byoyomi == 0.0 &&
		left_time < 15.0){

		// a. ���{���[���̂Ƃ��A�������O�̎肪pass�Ȃ�pass
		//    Return pass if the previous move is pass in Japanese rule.
		if(japanese_rule && b.prev_move[b.her] == PASS) return PASS;

		// b. �ő����s�̎q�m�[�h��1000�����̂Ƃ��Apolicy net�̍ŏ�ʂ�Ԃ�
		//    Return the move with highest probability if total game count is less than 1000.
		if(rc0_game_cnt < 1000){
			int v = pn->children[pn->prob_order[0]].move;
			if(is_errout){
				PrintLog(log_file, "move cnt=%d lambda=%.2f\n", b.move_cnt, lambda);
				PrintLog(log_file, "emagency mode: remaining time=%.1f[sec], move=%s, prob=.1%f[%%]\n",
						(double)left_time, CoordinateString(v).c_str(), pn->children[pn->prob_order[0]].prob * 100);
			}
			win_rate = 0.5;
			return v;
		}

	}
	// 7-2. ����T�����s��. Parallel search.
	else
	{
		// a. root�ȉ��ɑ��݂���m�[�h�𒲂ׁA����ȊO������
		if (is_root_changed) DeleteNodeIndex(root_node_idx);

		bool stand_out =
				!is_ponder &&
				(rc0_game_cnt > 10000 && rc0_game_cnt > 100 * rc1_game_cnt) &&
				((double)left_time > byoyomi || byoyomi == 0);
		bool enough_game = pn->total_game_cnt > 500000;
		bool almost_win =
				pn->total_game_cnt > 1000 &&
				(win_rate < 0.08 || win_rate > 0.92);

		if(stand_out || enough_game || almost_win)
		{
			// Skip search.
			if(is_errout){
				PrintLog(log_file, "%d[node] remaining time=%.1f[sec]\n", node_cnt, (double)left_time);
			}
		}
		else
		{

			const auto t1 = std::chrono::system_clock::now();
			int prev_game_cnt = pn->total_game_cnt;
			Statistics prev_stat = stat;

			double thinking_time = time_limit;
			bool can_extend = false;

			// b. �ő�v�l���Ԃ��v�Z����
			if(!is_ponder){
				if(time_limit == 0.0){
					if(main_time == 0.0){
						// �������Ԃ��b�ǂ݂����̂Ƃ�
						// Set byoyomi if the main time is 0.
						thinking_time = std::max(byoyomi, 0.1);
						can_extend = (extension_cnt > 0);
					}
					else
					{
						if(left_time < byoyomi * 2.0){
							thinking_time = std::max(byoyomi - 1.0, 1.0); // Take 1sec margin.
							can_extend = (extension_cnt > 0);
						}
						else{
							// �T�h���f�X�̂Ƃ��A�c�莞�Ԃ���Z�o
							// �b�ǂ݂�����Ƃ��A�b�ǂ݂�1-1.5�{
							// Calculate from remaining time if sudden death,
							// otherwise set that of 1-1.5 times of byoyomi.
							thinking_time = std::max(
										left_time/(55.0 + std::max(50.0 - b.move_cnt, 0.0)),
										byoyomi * (1.5 - (double)std::max(50.0 - b.move_cnt, 0.0) / 100)
									);
							// �T�h���f�X�ł́A�c�莞�Ԃ�15���ȉ��̂Ƃ��͎v�l�������Ȃ�
							// Do not extend thinking time if the remaining time is 10% or less.
							can_extend = (left_time > main_time * 0.15) || (byoyomi >= 10);
						}
					}

				}

				// �ǂ��炩�̏�����90%���̂Ƃ��A1�b�����v�l����
				// Think only for 1sec when either winning percentage is over 90%.
				if(win_rate < 0.1 || win_rate > 0.9) thinking_time = std::min(thinking_time, 1.0);
				can_extend &= (thinking_time > 1 && b.move_cnt > 3);
			}

			// c. thread_cnt�̃X���b�h�ŕ���T�����s��
			//    Search in parallel with thread_cnt threads.
			ParallelSearch(thinking_time, b, is_ponder);
			SortChildren(pn, rc);

			// d. 1�ʂ̎��2�ʂ̎�̎��s�񐔂�1.5�{�ȓ��̂Ƃ��A�v�l���Ԃ���������
			//    Extend thinking time when the trial number of first move
			//    and second move is close.
			if(!stop_think && can_extend){

				rc0_game_cnt = std::max((int)rc[0]->rollout_cnt, (int)rc[0]->value_cnt);
				rc1_game_cnt = std::max((int)rc[1]->rollout_cnt, (int)rc[1]->value_cnt);

				if(rc0_game_cnt < rc1_game_cnt * 1.5){

					if(byoyomi > 0 && left_time <= byoyomi){
						--extension_cnt;
					}
					else thinking_time *= 0.8;

					ParallelSearch(thinking_time, b, is_ponder);
					SortChildren(pn, rc);
				}

			}

			stop_think = false;

			// e. �Ֆʂ̐�L�����X�V. Update statistics of the board.
			if(pn->total_game_cnt - prev_game_cnt > 5000) stat -= prev_stat;

			// f. �T�������o�͂���
			//    Output search information.
			auto t2 = std::chrono::system_clock::now();
			auto elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
			if(is_errout){
				PrintLog(log_file, "%d[node] %.1f[sec] %d[playouts] %.1f[pps/thread]\nremaining time=%.1f[sec]\n",
						node_cnt,
						elapsed_time,
						(pn->total_game_cnt - prev_game_cnt),
						(pn->total_game_cnt - prev_game_cnt) / elapsed_time / (thread_cnt - gpu_cnt),
						std::max(0.0, (double)left_time - elapsed_time));
			}

		}
	}

	// 8. ���O�̎肪�p�X�̂Ƃ��������p�X�����邩���ׂ�
	//    Check whether pass should be returned. (Japanese rule)
	if(japanese_rule && b.prev_move[b.her] == PASS){
		Board b_cpy;
		int win_cnt = 0;
		int playout_cnt = 1000;
		int pl = b.my;
		LGR lgr_th = lgr;

		for(int i=0;i<playout_cnt;++i){
			b_cpy = b;
			b_cpy.PlayLegal(PASS);
			int result = PlayoutLGR(b_cpy, lgr_th, komi);
			if(pl == 0){
				if(result == 0) ++win_cnt;
			}
			else{
				if(result != 0) ++win_cnt;
			}
		}

		// ����7���ȏ�̂Ƃ��A�p�X��Ԃ�
		// Return pass if the winning rate > 70%.
		if((double)win_cnt / playout_cnt > 0.7){
			win_rate = (double)win_cnt / playout_cnt;
			return PASS;
		}
	}
	// 9. �őP�肪�p�X�Ō��ʂ��卷�Ȃ��Ƃ��A�p�X��2�Ԗڂ̌���
	//    When the best move is pass and the result is not much different,
	//    return the second move. (Chinese rule)
	else if (!japanese_rule && rc[0]->move == PASS) {
		double  win_sign = rc[0]->rollout_win * rc[1]->rollout_win;
		if(lambda == 1.0) win_sign = rc[0]->value_win * rc[1]->value_win;

		if (win_sign > 0) std::swap(rc[0], rc[1]);
	}

	// 10. �������X�V. Update winning_rate.
	win_rate = BranchRate(rc[0]);

	// 11. ��ʂ̎q�m�[�h�̒T�����ʂ��o�͂���.
	//     Output information of upper child nodes.
	if (is_errout) {
		PrintLog(log_file, "move cnt=%d lambda=%.2f\n", b.move_cnt + 1, lambda);
		PrintLog(log_file, "total games=%d, evaluated policy=%d(%d), evaluated value=%d(%d)\n",
				  (int)pn->total_game_cnt, eval_policy_cnt, (int)policy_que_cnt, eval_value_cnt, (int)value_que_cnt);

		for(int i=0;i<std::min((int)pn->child_cnt, 9);++i) {

			Child* pc = rc[i];
			int game_cnt = std::max((int)pc->rollout_cnt, (int)pc->value_cnt);

			if (game_cnt == 0) break;

			double rollout_rate = (pc->rollout_win / std::max(1, (int)pc->rollout_cnt) + 1) / 2;
			double value_rate = (pc->value_win / std::max(1, (int)pc->value_cnt) + 1) / 2;
			if(pc->value_win == 0.0) value_rate = 0;
			if(pc->rollout_win == 0.0) rollout_rate = 0;

			double rate = BranchRate(pc);

			int depth = 1;
			std::unordered_set<int> node_list;
			if(pc->is_next){
				depth = CollectNodeIndex((int)pc->next_idx, depth, node_list);
			}

			PrintLog(log_file, "%d: %s,\t", i + 1, CoordinateString((int)pc->move).c_str());
			PrintLog(log_file, "game=%d, \t", game_cnt);
			PrintLog(log_file, "%3.1f[%%](%3.2f/%3.2f),\t", rate * 100, rollout_rate, value_rate);
			PrintLog(log_file, "value=%.2f,\t", ((double)pc->value + 1) / 2);
			PrintLog(log_file, "prob=%3.1f[%%],\t", (double)pc->prob * 100);
			PrintLog(log_file, "depth=%d\n", depth);
		}
	}

	return rc[0]->move;

}

/**
 *  �X���b�h�ŒT�����J��Ԃ�
 *  Repeat searching with a single thread.
 */
void Tree::ThreadSearchBranch(Board& b, double time_limit, bool is_ponder) {

	Node* pn = &node[root_node_idx];
	if(pn->child_cnt <= 1){
		stop_think = true;
		return;
	}

	int loop_cnt = 0;
	int initial_game_cnt = pn->total_game_cnt;
	const auto t1 = std::chrono::system_clock::now();

	mtx_lgr.lock();

	LGR lgr_th = lgr;
	Statistics stat_th = stat;
	Statistics initial_stat = stat;

	mtx_lgr.unlock();


	for (;;){
		if(value_que_cnt > 192 || policy_que_cnt > 96){
			// Wait for 1msec if the queue is full.
			std::this_thread::sleep_for(std::chrono::microseconds(1000)); //1 msec
		}
		else{
			Board b_ = b;
			int node_idx = root_node_idx;
			double value_result = 0.0;
			std::vector<std::pair<int,int>> serch_route;
			SearchBranch(b_, node_idx, value_result, serch_route, lgr_th, stat_th);
		}
		++loop_cnt;

		// 64�񂲂ƂɒT����ł��؂邩���`�F�b�N
		// Check whether to terminate the search every 64 times.
		if (loop_cnt % 64 == 0) {
			auto t2 = std::chrono::system_clock::now();
			auto elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000;

			// �������Ԃ��o�߂������Astop_think�t���O���������Ƃ��T���I��
			// Terminate the search when the time limit has elapsed or stop_think flag is set.
			if(elapsed_time > time_limit || stop_think){
				stat_th -= initial_stat;
				{
					std::lock_guard<std::mutex> lock(mtx_lgr);
					lgr = lgr_th;
					stat += stat_th;
				}
				break;
			}
			else if(!is_ponder && (byoyomi == 0 || (double)left_time > byoyomi))
			{
				std::vector<Child*> rc;
				SortChildren(pn, rc);

				int th_rollout_cnt = (int)pn->total_game_cnt - initial_game_cnt;
				int rc0_cnt = std::max((int)rc[0]->rollout_cnt, (int)rc[0]->value_cnt);
				int rc1_cnt = std::max((int)rc[1]->rollout_cnt, (int)rc[1]->value_cnt);
				double max_rc1_cnt = rc1_cnt + th_rollout_cnt * (time_limit - elapsed_time) / elapsed_time;

				bool stand_out = 	rc0_cnt > 10000 &&
									rc0_cnt > 100 * rc1_cnt;
				bool cannot_catchup =	th_rollout_cnt > 15000 &&
										rc0_cnt > 1.5 * max_rc1_cnt;

				if(stand_out || cannot_catchup)
				{
					stop_think = true;
					break;
				}
			}
		}
	}

}

/**
 *  �X���b�h��policy/value�̕]�����s��
 *  Evaluate policy and value of boards in a single thread.
 */
void Tree::ThreadEvaluate(double time_limit, int gpu_idx, bool is_ponder) {

	const auto t1 = std::chrono::system_clock::now();
	std::deque<ValueEntry> vque_th;
	std::deque<PolicyEntry> pque_th;
	std::vector<FeedTensor> ft_list;
	std::vector<std::array<double,EBVCNT>> prob_list;

	for (;;){

		// 1. value_que������. Process value_que.
		if(value_que_cnt > 0){
			int eval_cnt = 0;
			{
				std::lock_guard<std::mutex> lock(mtx_vque);
				if(value_que_cnt > 0){
					eval_cnt = std::min(48, (int)value_que_cnt);

					// a. vque_th�ɃR�s�[. Copy partially to vque_th.
					vque_th.resize(eval_cnt);
					copy(value_que.begin(), value_que.begin() + eval_cnt, vque_th.begin());

					// b. value_que��擪����폜.
					//    Remove value_que from the beginning.
					for(int i=0;i<eval_cnt;++i) value_que.pop_front();
					value_que_cnt -= eval_cnt;
				}
			}

			if(eval_cnt > 0){
				Child* pc0;

				if(eval_cnt > 0){

					// c. Evaluate value.
					ft_list.clear();
					for(int i=0;i<eval_cnt;++i){
						ft_list.push_back(vque_th[i].ft);
					}
					std::vector<float> eval_list;
					ValueNet(sess_value[gpu_idx], ft_list, eval_list, sym_idx);

					// d. �㗬�m�[�h��value_win��S�čX�V����
					//    Update all value information of the upstream nodes.
					for(int i=0;i<eval_cnt;++i){

						int leaf_pl = ft_list[i].color;
						pc0 = &node[vque_th[i].node_idx[0]].children[vque_th[i].child_idx[0]];
						pc0->value = -(double)eval_list[i];
						pc0->is_value_eval = true;

						for(int j=0, n=vque_th[i].depth;j<n;++j){
							if(	vque_th[i].node_idx[j] < 0 ||
								vque_th[i].node_idx[j] >= node_limit) continue;
							if(	vque_th[i].child_idx[j] >= node[vque_th[i].node_idx[j]].child_cnt) continue;

							Node* pn = &node[vque_th[i].node_idx[j]];
							Child* pc = &pn->children[vque_th[i].child_idx[j]];
							int add_cnt = vque_th[i].request_cnt;
							double add_win = double(add_cnt * eval_list[i]);
							if(int(pn->pl) != leaf_pl) add_win *= -1;

							FetchAdd(&pc->value_win, add_win);
							FetchAdd(&pn->value_win, add_win);
							pc->value_cnt += add_cnt;
							pn->value_cnt += add_cnt;

							if(vque_th[i].node_idx[j] == root_node_idx) break;
						}

					}
					eval_value_cnt += eval_cnt;
				}
			}
		}

		// 2. policy_que������. Process policy_que.
		if(policy_que_cnt > 0){
			int eval_cnt = 0;
			{
				std::lock_guard<std::mutex> lock(mtx_pque);
				if(policy_que_cnt > 0){
					eval_cnt = std::min(16, (int)policy_que_cnt);
					eval_policy_cnt += eval_cnt;

					// a. pque_th�ɃR�s�[. Copy partially to pque_th.
					pque_th.resize(eval_cnt);
					copy(policy_que.begin(), policy_que.begin()+eval_cnt, pque_th.begin());

					// b. policy_que��擪����폜.
					//    Remove policy_que from the beginning.
					for(int i=0;i<eval_cnt;++i) policy_que.pop_front();
					policy_que_cnt -= eval_cnt;
				}
			}

			if(eval_cnt > 0){
				ft_list.clear();
				for(int i=0;i<eval_cnt;++i){
					ft_list.push_back(pque_th[i].ft);
				}

				// c. Evaluate policy.
				PolicyNet(sess_policy[gpu_idx], ft_list, prob_list, policy_temp, sym_idx);

				// d. �m�[�h�̊m�����z���X�V����. Update probability of nodes.
				for(int i=0;i<eval_cnt;++i){
					UpdateNodeProb(pque_th[i].node_idx, prob_list[i]);
				}
			}
		}

		// 3. �������Ԃ��o�߂������Astop_think�t���O���������Ƃ��]���I��
		//    Terminate evaluation when the time limit has elapsed or stop_think flag is set.
		auto t2 = std::chrono::system_clock::now();
		auto elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
		if(elapsed_time > time_limit || stop_think){
			stop_think = true;
			break;
		}

	}

}

/**
 *  thread_cnt�̃X���b�h�ŕ���T�����s��
 *  Search in parallel with thread_cnt threads.
 */
void Tree::ParallelSearch(double time_limit, Board& b, bool is_ponder){
	std::vector<std::thread> ths(thread_cnt);
	std::vector<Board> b_;
	for(int i=0;i<thread_cnt-gpu_cnt;++i){
		Board b_tmp = b;
		b_.push_back(b_tmp);
	}

	for(int i=0;i<thread_cnt;++i){
		if(i < gpu_cnt){
			ths[i] = std::thread(&Tree::ThreadEvaluate, this, time_limit, i, is_ponder);
		}
		else{
			ths[i] = std::thread(&Tree::ThreadSearchBranch, this, std::ref(b_[i - gpu_cnt]), time_limit, is_ponder);
		}
	}

	for(std::thread& th : ths) th.join();
}


/**
 *  1000��v���C�A�E�g���s���A�ŏI���ʂ��o�͂���
 *  Roll out 1000 times and output the final result.
 */
void Tree::PrintResult(Board& b){

	stat.Clear();

	int win_cnt = 0;
	int rollout_cnt = 1000;
	Board b_ = b;
	for(int i=0;i<rollout_cnt;++i){
		b_ = b;
		int result = PlayoutLGR(b_, lgr, stat, komi);
		if(result != 0) ++win_cnt;
	}
	int win_pl = ((double)win_cnt/rollout_cnt >= 0.5)? 1 : 0;

	PrintFinalScore(b, stat.game, stat.owner, win_pl, komi, log_file);

}

