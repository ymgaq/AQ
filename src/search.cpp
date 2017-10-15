#include <algorithm>
#include <thread>
#include <stdarg.h>
#include <iomanip>
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

#ifdef _WIN32
	std::string spl_str = "\\";
	#undef max
	#undef min
#else
	std::string spl_str = "/";
#endif

/**
 *  弱いCASを繰り返して加算する double用
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
	std::string sl_path;
	std::string vl_path;
	std::stringstream ss;

	if(pb_dir != "") ss << pb_dir << spl_str;
	ss << "sl.pb";
	sl_path = ss.str();

	ss.str("");
	if(pb_dir != "") ss << pb_dir << spl_str;
	ss << "vl.pb";
	vl_path = ss.str();

	std::vector<int> gpu_list;
	for(int i=0;i<gpu_cnt;++i) gpu_list.push_back(i);

	SetGPU(sl_path, vl_path, gpu_list);

}


Tree::Tree(std::string sl_path, std::string vl_path, std::vector<int>& gpu_list){

	Tree::Clear();
	SetGPU(sl_path, vl_path, gpu_list);

}


void Tree::SetGPU(std::string sl_path, std::string vl_path, std::vector<int>& gpu_list){

	// Suppress tensorflow information and warnings.
	if(!self_match) putenv("TF_CPP_MIN_LOG_LEVEL=2");
	
	{
		using namespace tensorflow;

		sess_policy.clear();
		sess_value.clear();

		if(gpu_list.empty())
			for(int i=0;i<gpu_cnt;++i) gpu_list.push_back(i);

		for(int i=0, n=(int)gpu_list.size();i<n;++i){

#ifdef CPU_ONLY
			const std::string device_name = "/cpu:" + std::to_string(gpu_list[i]);
#else
			const std::string device_name = "/gpu:" + std::to_string(gpu_list[i]);
#endif //CPU_ONLY

			Session* sess_p(NewSession(SessionOptions()));
			GraphDef graph_p;

			ReadBinaryProto(Env::Default(), sl_path, &graph_p);
			graph::SetDefaultDevice(device_name, &graph_p);
			sess_p->Create(graph_p);

			Session* sess_v(NewSession(SessionOptions()));
			GraphDef graph_v;

			ReadBinaryProto(Env::Default(), vl_path, &graph_v);
			graph::SetDefaultDevice(device_name, &graph_v);
			sess_v->Create(graph_v);

			sess_policy.push_back(sess_p);
			sess_value.push_back(sess_v);

		}
	}

}


void Tree::Clear(){

#ifdef CPU_ONLY
	thread_cnt = 2;
	gpu_cnt = 1;
	expand_cnt = 64;
#else
	thread_cnt = (cfg_thread_cnt > cfg_gpu_cnt) ? cfg_thread_cnt : cfg_gpu_cnt + 1;
	gpu_cnt = cfg_gpu_cnt;
	expand_cnt = 12;
#endif // CPU_ONLY

	main_time = cfg_main_time;
	byoyomi = cfg_byoyomi;
	komi = cfg_komi;
	sym_idx = cfg_sym_idx;

	vloss_cnt = 3;
	lambda = 0.7;
	cp = 2.0;
	policy_temp = 0.7;

	Tree::InitBoard();

	node.clear();
	node.resize(node_limit);

	log_path = "";
	live_best_sequence = false;
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
	node_move_cnt = 0;

}


/**
 *  policy_queに局面を追加する
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
 *  value_queに局面を追加する
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
 *  ノードを新規作成する
 *  既に登録されているときはそのindexを返す
 *
 *  Create a new Node and returns the index.
 */
int Tree::CreateNode(Board& b) {

	// 入力盤面のハッシュを求める. Calculate board hash.
	int64 hash_b = BoardHash(b);
	int node_idx;

	{
		Node *pn;
		{
			std::lock_guard<std::mutex> lock(mtx_node);

			if(node_hash_list.find(hash_b) != node_hash_list.end()){

				// 別のスレッドでこの局面ノードを生成中
				// Return -1 if another thread is creating this node.
				if(node[node_hash_list[hash_b]].is_creating) return -1;

				// 既に登録済のとき、そのindexを返す
				// Return the index if the key is already registered.
				else{
					// 盤面ハッシュが同一か確認
					// Confirm whether the board hashes are the same.
					if(node[node_hash_list[hash_b]].hash == hash_b){
						return node_hash_list[hash_b];
					}
				}
			}

			node_idx = int(hash_b % (int64)node_limit);
			node_idx = std::max(0, std::min(node_idx, node_limit - 1));
			pn = &node[node_idx];

			// 別の登録がされているor作成中のとき、node_idxを変更
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
		pn->move_cnt = b.move_cnt;
		pn->prev_move[0] = b.prev_move[b.her];
		pn->prev_move[1] = b.prev_move[b.my];

		pn->prev_ptn[0] = b.prev_ptn[0].bf;
		pn->prev_ptn[1] = b.prev_ptn[1].bf;

		int my = b.my;
		int her = b.her;
		double sum_prob = 0.0;

		int prev_move_[3] = {b.prev_move[her], b.prev_move[my], PASS};
		if(b.move_cnt >= 3) prev_move_[2] = b.move_history[b.move_cnt - 3];

		for(int i=0;i<EBVCNT;++i){
			pn->prob_roll[my][i] = 0;
			pn->prob_roll[her][i] = 0;
		}

		for(int i=0, n=b.empty_cnt;i<n;++i){
			int v = b.empty[i];
			double my_dp = 1.0;
			double her_dp = 1.0;
			my_dp *= prob_dist[0][DistBetween(v, prev_move_[0])][0];
			my_dp *= prob_dist[1][DistBetween(v, prev_move_[1])][0];
			her_dp *= prob_dist[0][DistBetween(v, prev_move_[2])][0];
			her_dp *= prob_dist[1][DistBetween(v, prev_move_[1])][0];
			Pattern3x3 ptn12 = b.ptn[v];
			ptn12.SetColor(8, b.color[std::min(EBVCNT - 1, (v + EBSIZE * 2))]);
			ptn12.SetColor(9, b.color[v + 2]);
			ptn12.SetColor(10, b.color[std::max(0, (v - EBSIZE * 2))]);
			ptn12.SetColor(11, b.color[v - 2]);
			if(prob_ptn12.find(ptn12.bf) != prob_ptn12.end()){
				my_dp *= prob_ptn12[ptn12.bf][my];
				her_dp *= prob_ptn12[ptn12.bf][her];
			}

			pn->prob_roll[my][v] = b.prob[my][v] * my_dp;
			pn->prob_roll[her][v] = b.prob[her][v] * her_dp;

			sum_prob += b.prob[my][v] * my_dp;
		}

		double inv_sum = 1.0;
		if(sum_prob != 0) inv_sum = 1.0 / sum_prob;
		for(int i=0;i<EBVCNT;++i) pn->prob[i] = (double)pn->prob_roll[my][i] * inv_sum;

		std::vector<std::pair<double, int>> prob_list;
		for(int i=0;i<b.empty_cnt;++i) {
			int v = b.empty[i];
			if(	!b.IsLegal(b.my,v)	||
				b.IsEyeShape(b.my,v)||
				b.IsSeki(v)) 	continue;

			prob_list.push_back(std::make_pair(pn->prob[v].load() * inv_sum, v));
		}
		std::sort(prob_list.begin(), prob_list.end(), std::greater<std::pair<double,int>>());

		for (int i=0, n=(int)prob_list.size(); i<n; ++i) {
			Child new_child;
			new_child.move = prob_list[i].second;
			new_child.prob = prob_list[i].first;

			// 子局面を登録. Register the child.
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

	// 作成したノードのindexを返す
	// Return the Node index.
	return node_idx;

}


/**
 *  ノードの確率分布をpolicy netに置き換える
 *  Update probability of node with that evaluated
 *  by the policy network.
 */
void Tree::UpdateNodeProb(int node_idx, std::array<double, EBVCNT>& prob_list) {

	// 1. node[node_idx]の確率分布をprob_listに更新
	//    Replace probability of node[node_idx] with prob_list.
	Node* pn = &node[node_idx];
	for(int i=0;i<BVCNT;++i){
		int v = rtoe[i];
		pn->prob[v] = prob_list[v];
	}

	// 2. (prob,idx)のペアを降順にソートし、prob_orderを更新
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

	// 3. LGRの着手をlgr.policyに登録
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
 *  node[node_idx]以下に連なるノードのindexを収集する
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
				// 次のノードが存在するときは再帰呼び出し.
				// Call recursively if next node exits.
				int tmp_depth = CollectNodeIndex((int)pc->next_idx, depth, node_list);
				if(tmp_depth > max_depth) max_depth = tmp_depth;
			}
		}
	}

	return max_depth;

}


/**
 *  ノード以下のインデックスを調べ、ノード使用率を減らす
 *  Delete indexes to reduce node usage rate. (30%-60%)
 */
void Tree::DeleteNodeIndex(int node_idx){

	// 1. ノード使用率が60%未満なら削除しない
	//    Do not delete nodes if node utilization is less than 60%.
	if(node_cnt < 0.6 * node_limit) return;

	// 2. node_idxに繋がるインデックスを調べる
	//    Find indexes connecting to the root node.
	std::unordered_set<int> under_root;
	CollectNodeIndex(node_idx, 0, under_root);

	// 3. ノード使用率が30%以下になるまで最古のノード手数を更新する
	//    Update the oldest move count of the nodes until the node
	//    usage becomes 30% or less.
	std::unordered_set<int> node_list(under_root);

	if((int)node_list.size() < 0.3 * node_limit){
		for(int i=0, n=move_cnt-node_move_cnt;i<n;++i){
			++node_move_cnt;

			for(int j=0;j<node_limit;++j){
				if(node[j].move_cnt >= node_move_cnt){
					node_list.insert(j);
				}
			}

			if((int)node_list.size() < 0.3 * node_limit){
				break;
			}
			else{
				node_list.clear();
				node_list = under_root;
			}
		}
	}

	// 4. node_listにない古いノードを削除
	//    Delete old node not in node_list.
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

	// 5. policy_queから削除
	//    Remove entries from policy_que.
	std::deque<PolicyEntry> remain_pque;
	for(auto i:policy_que){
		if(node_list.find(i.node_idx) != node_list.end()){
			remain_pque.push_back(i);
		}
	}
	policy_que.swap(remain_pque);
	policy_que_cnt = (int)policy_que.size();

	// 6. value_queから削除
	//    Remove entries from value_que.
	std::deque<ValueEntry> remain_vque;
	for(auto i:value_que){
		if(node_list.find(i.node_idx[0]) != node_list.end()){

			bool is_remain = true;

			for(int j=1, j_max=i.depth-1;j<j_max;++j){
				if(	node_list.find(i.node_idx[j]) == node_list.end())
				{
					// 経路に削除されたノードが含まれるときは削除
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
 *  ルートノードを入力盤面のものに変更する
 *  Update the root node with the input board.
 */
int Tree::UpdateRootNode(Board&b){

	move_cnt = b.move_cnt;
	int node_idx = CreateNode(b);

	if(root_node_idx != node_idx) DeleteNodeIndex(node_idx);
	root_node_idx = node_idx;

	return node_idx;

}


/**
 *  探索木の中で、親ノード → 子ノードの移動を1回行う
 *  子ノードが存在しないとき、新規に作成するかを判断する
 *  末端ではプレイアウト・ValueNet評価を行い、その結果を返す
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

	// 1. action valueが一番高い手を選ぶ
	//    Choose the move with the highest action value.
	int max_idx = 0;
	double max_avalue = -128;

	double pn_rollout_rate = (double)pn->rollout_win/((double)pn->rollout_cnt + 0.01);
	double pn_value_rate = (double)pn->value_win/((double)pn->value_cnt + 0.01);

	double rollout_cnt, rollout_win, value_cnt, value_win;
	double rollout_rate, value_rate, game_cnt, rate, action_value;

	for (int i=0, n=(int)pn->child_cnt;i<n;++i) {

		// a. 確率が高い順に調べる
		//    Search in descending order of probability.
		int child_idx = pn->prob_order[i];
		pc = &pn->children[child_idx];
		int next_idx = (int)pc->next_idx;

		// b. 子ノードが展開済の場合はその情報を利用する
		//    なければ現在のノードのブランチの情報を使う
		//    Use the child node information if already expanded,
		//    otherwise use that of the child branch in the current
		//    node.
		if(pc->is_next &&
			next_idx >= 0 && next_idx < node_limit &&
			pc->next_hash == node[next_idx].hash)
		{
			// 子ノードの探索情報.
			// Search information in the child node.
			rollout_cnt = (double)node[next_idx].rollout_cnt;
			value_cnt = (double)node[next_idx].value_cnt;
			// 手番が変わるので評価値を反転
			// Reverse value as the turn changes.
			rollout_win = -(double)node[next_idx].rollout_win;
			value_win = -(double)node[next_idx].value_win;
		}
		else{
			// ブランチの探索情報.
			// Search information in the child branch.
			rollout_cnt = (double)pc->rollout_cnt;
			value_cnt = (double)pc->value_cnt;
			rollout_win = (double)pc->rollout_win;
			value_win = (double)pc->value_win;
		}

		// c. この手の勝率を計算する
		//    Calculate winning rate of this move.
		if(rollout_cnt == 0) 	rollout_rate = pn_rollout_rate;
		else					rollout_rate = rollout_win / rollout_cnt;
		if(value_cnt == 0)		value_rate = pn_value_rate;
		else					value_rate = value_win / value_cnt;

		rate = (1-lambda) * rollout_rate + lambda * value_rate;

		// d. action valueを求める
		//    Calculate action value.
		game_cnt = use_rollout? (double)pc->rollout_cnt : (double)pc->value_cnt;
		action_value = rate + cp * pc->prob * sqrt((double)pn->total_game_cnt) / (1 + game_cnt);

		// e. max_idxを更新. Update max_idx.
		if (action_value > max_avalue) {
			max_avalue = action_value;
			max_idx = child_idx;
		}

	}

	// 2. action valueが最大の手を探索する
	//    Search for the move with the maximum action value.
	pc = &pn->children[max_idx];
	int next_idx = std::max(0, std::min(node_limit - 1, (int)pc->next_idx));
	Node* npn = &node[next_idx]; // Next pointer of the node.

	serch_route.push_back(std::make_pair(node_idx, max_idx));
	int next_move = pc->move;
	int prev_move = b.prev_move[b.her];
	// 勝敗結果の(0,±1)を(-0.5,+0.5)に補正するバイアス
	// Bias of winning rate that corrects result of (0, +/-1) to (-0.5, +0.5).
	double win_bias = (b.my == 0)? -0.5 : 0.5;

	// 3. LGRを更新. Update LGR of policy.
	if(use_rollout && !pn->is_visit && pn->is_policy_eval)
	{
		// 現在のroot nodeになってから最初に探索するときに更新
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

	// 4. プレイアウトをするかを判断
	//    Check if rollout is necessary.
	bool need_rollout = false;
	if(	(!pc->is_next && (std::max((int)pc->rollout_cnt, (int)pc->value_cnt) < expand_cnt))	||
		(next_move==PASS && prev_move==PASS) 			||
		(b.move_cnt > 720)								||
		(pn->child_cnt <= 1 && pc->is_next && npn->child_cnt <= 1))
	{
		need_rollout = true;
	}

	// 5. ノード展開するかを判断
	//    Check whether the next node can be expanded.
	bool expand_node = false;
	if(	!need_rollout &&
		(pc->is_next == false || (int64)pc->next_hash != (int64)npn->hash) )
	{
		// 置換表が8割埋まっているときは新規作成しない
		// New node is not chreated when the transposition table is filled by 80%.
		if(node_cnt < 0.8 * node_limit) expand_node = true;
		else need_rollout = true;
	}

	// 6. 局面を進める. Play next_mvoe.
	b.PlayLegal(next_move);

	// 7. ノードを展開する
	//    Expand the next node.
	if(expand_node){
		int next_idx_exp = CreateNode(b);
		// たまに置換表が壊れて不正なindexを返すのでその対策
		if(next_idx_exp < 0 || next_idx_exp >= node_limit) need_rollout = true;
		else{
			npn = &node[next_idx_exp];
			pc->next_idx = next_idx_exp;
			pc->next_hash = (int64)npn->hash;
			pc->is_next = true;

			// pc -> npnへ対局情報を反映. Reflect game information.
			npn->total_game_cnt += (int)pc->rollout_cnt;
			npn->rollout_cnt += (int)pc->rollout_cnt;
			npn->value_cnt += (double)pc->value_cnt;
			// 手番が変わるので評価値を反転
			// Reverse evaluation value since turn changes.
			FetchAdd(&npn->rollout_win, -(double)pc->rollout_win);
			FetchAdd(&npn->value_win, -(double)pc->value_win);
		}
	}

	// 8. virtual lossを加える. Add virtual loss.
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


	// 9. 末端であればプレイアウトを行い、次のノードが存在すれば探索を進める
	//    Roll out if it is the leaf node, otherwise proceed to the next node.
	double rollout_result = 0.0;
	if (need_rollout)
	{
		// a-1. 局面が未評価であればキューに追加する
		//      Add into the queue if the board is not evaluated.
		value_result = 0;
		if(pc->is_value_eval){
			value_result = (double)pc->value;
		}
		else{
			AddValueQue(serch_route, b);
		}

		if(use_rollout){
			// b. 確率分布をコピー. Copy probability.
			if(pc->is_next && (int64)pc->next_hash == (int64)npn->hash){
				for(int i=0, n=b.empty_cnt;i<n;++i){
					int v = b.empty[i];
					b.ReplaceProb(0, v, (double)npn->prob_roll[0][v]);
					b.ReplaceProb(1, v, (double)npn->prob_roll[1][v]);
				}
			}
			// c. プレイアウトし、結果を[-1.0, 1.0]に規格化
			//    Roll out and normalize the result to [-1.0, 1.0].
			rollout_result = -2.0 * ((double)PlayoutLGR(b, lgr_, stat_, komi) + win_bias);
			//rollout_result = -2.0 * ((double)PlayoutLGR(b, lgr_, komi) + win_bias);
		}
	}
	else{
		// a-2. 次のnodeに進む
		//      手番が変わっているので、結果も符号反転させる
		//      Proceed to the next node and reverse the results.
		rollout_result = -SearchBranch(b, (int)pc->next_idx, value_result, serch_route, lgr_, stat_);
		value_result *= -1.0;
	}

	// 10. virtual lossを解消&勝率更新
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


void PrintLog(std::string log_path, const char* output_text, ...){

	va_list args;
	char buf[1024];

	va_start(args, output_text);
	vsprintf(buf, output_text, args);
	va_end(args);	

	string str(buf);
	std::cerr << str;
	std::ofstream ofs(log_path, std::ios::app);
	ofs << str;

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
 *  探索を繰り返し最善手を求める
 *  Repeat searching for the best move.
 */
int Tree::SearchTree(	Board& b, double time_limit, double& win_rate,
						bool is_errout, bool is_ponder)
{

	// 1. root nodeを更新. Update root node.
	if(b.move_cnt == 0) Tree::InitBoard();
	int node_idx = CreateNode(b);
	bool is_root_changed = (root_node_idx != node_idx);
	root_node_idx = node_idx;
	move_cnt = b.move_cnt;
	eval_policy_cnt = 0;
	eval_value_cnt = 0;

	// 2. 合法手がないときはパスを返す
	//    Return pass if there is no legal move.
	Node *pn = &node[root_node_idx];
	if (pn->child_cnt <= 1){
		win_rate = 0.5;
		return PASS;
	}

	// 3. lambdaを進行度に合わせて調整 (0.8 -> 0.5)
	//    Adjust lambda to progress.
	lambda = 0.8 - std::min(0.3, std::max(0.0, ((double)b.move_cnt - 160) / 600));
	//lambda = 0.7 - std::min(0.4, std::max(0.0, ((double)b.move_cnt - 160) / 500));
	//cp = 3.0;
	//expand_cnt = 18;

	// 4. root nodeが未評価のとき、確率分布を評価する
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

	// 5. 子ノードを探索回数が多い順にソート
	//    Sort child nodes in descending order of search count.
	std::vector<Child*> rc;
	SortChildren(pn, rc);

	// 6. 探索回数が最大の子ノードの勝率を求める
	//    Calculate the winning percentage of pc0.
	win_rate = BranchRate(rc[0]);
	int rc0_game_cnt = std::max((int)rc[0]->value_cnt, (int)rc[0]->rollout_cnt);
	int rc1_game_cnt = std::max((int)rc[1]->value_cnt, (int)rc[1]->rollout_cnt);

	// 7-1. 持ち時間が残り少ないときは探索しない
	//      Return best move without searching when time is running out.
	if(!is_ponder &&
		time_limit == 0.0 &&
		byoyomi == 0.0 &&
		left_time < 15.0){

		// a. 日本ルールのとき、もし直前の手がpassならpass
		//    Return pass if the previous move is pass in Japanese rule.
		if(japanese_rule && b.prev_move[b.her] == PASS) return PASS;

		// b. 最多試行の子ノードが1000未満のとき、policy netの最上位を返す
		//    Return the move with highest probability if total game count is less than 1000.
		if(rc0_game_cnt < 1000){
			int v = pn->children[pn->prob_order[0]].move;
			if(is_errout){
				PrintLog(log_path, "move cnt=%d: emagency mode: left time=%.1f[sec], move=%s, prob=.1%f[%%]\n",
						b.move_cnt + 1, (double)left_time, CoordinateString(v).c_str(), pn->children[pn->prob_order[0]].prob * 100);
			}
			win_rate = 0.5;
			return v;
		}

	}
	// 7-2. 並列探索を行う. Parallel search.
	else
	{
		// a. root以下に存在するノードを調べ、それ以外を消去
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
				PrintLog(log_path, "move cnt=%d: left time=%.1f[sec]\n%d[nodes]\n", 
					b.move_cnt + 1, (double)left_time, node_cnt);
			}
		}
		else
		{

			const auto t1 = std::chrono::system_clock::now();
			int prev_game_cnt = pn->total_game_cnt;
			Statistics prev_stat = stat;

			double thinking_time = time_limit;
			bool can_extend = false;

			// b. 最大思考時間を計算する
			if(!is_ponder){
				if(time_limit == 0.0){
					if(main_time == 0.0){
						// 持ち時間が秒読みだけのとき
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
							// サドンデスのとき、残り時間から算出
							// 秒読みがあるとき、秒読みの1-1.5倍
							// Calculate from remaining time if sudden death,
							// otherwise set that of 1-1.5 times of byoyomi.
							thinking_time = std::max(
										left_time/(55.0 + std::max(50.0 - b.move_cnt, 0.0)),
										byoyomi * (1.5 - (double)std::max(50.0 - b.move_cnt, 0.0) / 100)
									);
							// サドンデスでは、残り時間が15％以下のときは思考延長しない
							// Do not extend thinking time if the remaining time is 10% or less.
							can_extend = (left_time > main_time * 0.15) || (byoyomi >= 10);
						}
					}

				}

				// どちらかの勝率が90%超のとき、1秒だけ思考する
				// Think only for 1sec when either winning percentage is over 90%.
				if(win_rate < 0.1 || win_rate > 0.9) thinking_time = std::min(thinking_time, 1.0);
				can_extend &= (thinking_time > 1 && b.move_cnt > 3);
			}

			// c. thread_cnt個のスレッドで並列探索を行う
			//    Search in parallel with thread_cnt threads.
			ParallelSearch(thinking_time, b, is_ponder);
			SortChildren(pn, rc);

			// d. 1位の手と2位の手の試行回数が1.5倍以内のとき、思考時間を延長する
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

			// e. 盤面の占有率を更新. Update statistics of the board.
			if(pn->total_game_cnt - prev_game_cnt > 5000) stat -= prev_stat;

			// f. 探索情報を出力する
			//    Output search information.
			auto t2 = std::chrono::system_clock::now();
			auto elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
			if(is_errout){
				PrintLog(log_path, "move cnt=%d: left time=%.1f[sec]\n%d[nodes] %.1f[sec] %d[playouts] %.1f[pps/thread]\n",
						b.move_cnt + 1,
						std::max(0.0, (double)left_time - elapsed_time),
						node_cnt,
						elapsed_time,
						(pn->total_game_cnt - prev_game_cnt),
						(pn->total_game_cnt - prev_game_cnt) / elapsed_time / (thread_cnt - gpu_cnt)
						);
			}

		}
	}

	// 8. 直前の手がパスのとき自分もパスをするか調べる
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

		// 勝率7割以上のとき、パスを返す
		// Return pass if the winning rate > 70%.
		if((double)win_cnt / playout_cnt > 0.7){
			win_rate = (double)win_cnt / playout_cnt;
			return PASS;
		}
	}
	// 9. 最善手がパスで結果が大差ないとき、パスを2番目の候補に
	//    When the best move is pass and the result is not much different,
	//    return the second move. (Chinese rule)
	else if (!japanese_rule && rc[0]->move == PASS) {
		double  win_sign = rc[0]->rollout_win * rc[1]->rollout_win;
		if(lambda == 1.0) win_sign = rc[0]->value_win * rc[1]->value_win;

		if (win_sign > 0) std::swap(rc[0], rc[1]);
	}

	// 10. 勝率を更新. Update winning_rate.
	win_rate = BranchRate(rc[0]);

	// 11. 上位の子ノードの探索結果を出力する.
	//     Output information of upper child nodes.
	if (is_errout) {
		PrintLog(log_path, "total games=%d, evaluated policy=%d(%d), value=%d(%d)\n",
				  (int)pn->total_game_cnt, eval_policy_cnt, (int)policy_que_cnt, eval_value_cnt, (int)value_que_cnt);

		std::stringstream ss;
		PrintChildInfo(root_node_idx, ss);
		PrintLog(log_path, "%s", ss.str().c_str());
	}

	return rc[0]->move;

}

/**
 *  スレッドで探索を繰り返す
 *  Repeat searching with a single thread.
 */
void Tree::ThreadSearchBranch(Board& b, double time_limit, int cpu_idx, bool is_ponder) {

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

#ifdef CPU_ONLY
	const int max_value_cnt = 24;
	const int max_policy_cnt = 12;
#else
	const int max_value_cnt = 192;
	const int max_policy_cnt = 96;
#endif //CPU_ONLY


	for (;;){
		if(value_que_cnt > max_value_cnt || policy_que_cnt > max_policy_cnt){
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

		// 64回ごとに探索を打ち切るかをチェック
		// Check whether to terminate the search every 64 times.
		if (loop_cnt % 64 == 0) {
			auto t2 = std::chrono::system_clock::now();
			auto elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() / 1000;

			// 制限時間が経過したか、stop_thinkフラグが立ったとき探索終了
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
					stat_th -= initial_stat;
					{
						std::lock_guard<std::mutex> lock(mtx_lgr);
						lgr = lgr_th;
						stat += stat_th;
					}

					stop_think = true;
					break;
				}
			}
		}
	}

}

/**
 *  スレッドでpolicy/valueの評価を行う
 *  Evaluate policy and value of boards in a single thread.
 */
void Tree::ThreadEvaluate(double time_limit, int gpu_idx, bool is_ponder) {

	const auto t1 = std::chrono::system_clock::now();
	std::deque<ValueEntry> vque_th;
	std::deque<PolicyEntry> pque_th;
	std::vector<FeedTensor> ft_list;
	std::vector<std::array<double,EBVCNT>> prob_list;

#ifdef CPU_ONLY
	const int max_eval_value = 1;
	const int max_eval_policy = 1;
#else
	const int max_eval_value = 48;
	const int max_eval_policy = 16;
#endif //CPU_ONLY

	for (;;){

		// 1. value_queを処理. Process value_que.
		if(value_que_cnt > 0){
			int eval_cnt = 0;
			{
				std::lock_guard<std::mutex> lock(mtx_vque);
				if(value_que_cnt > 0){
					eval_cnt = std::min(max_eval_value, (int)value_que_cnt);

					// a. vque_thにコピー. Copy partially to vque_th.
					vque_th.resize(eval_cnt);
					copy(value_que.begin(), value_que.begin() + eval_cnt, vque_th.begin());

					// b. value_queを先頭から削除.
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

					// d. 上流ノードのvalue_winを全て更新する
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

		// 2. policy_queを処理. Process policy_que.
#ifdef CPU_ONLY
		if(policy_que_cnt > 0 && mt_double(mt_32) < 0.25){
#else
		if (policy_que_cnt > 0) {
#endif //CPU_ONLY
			int eval_cnt = 0;
			{
				std::lock_guard<std::mutex> lock(mtx_pque);
				if(policy_que_cnt > 0){
					eval_cnt = std::min(max_eval_policy, (int)policy_que_cnt);
					eval_policy_cnt += eval_cnt;

					// a. pque_thにコピー. Copy partially to pque_th.
					pque_th.resize(eval_cnt);
					copy(policy_que.begin(), policy_que.begin()+eval_cnt, pque_th.begin());

					// b. policy_queを先頭から削除.
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

				// d. ノードの確率分布を更新する. Update probability of nodes.
				for(int i=0;i<eval_cnt;++i){
					UpdateNodeProb(pque_th[i].node_idx, prob_list[i]);
				}
			}
		}

		// 3. 制限時間が経過したか、stop_thinkフラグが立ったとき評価終了
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
 *  thread_cnt個のスレッドで並列探索を行う
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
			ths[i] = std::thread(&Tree::ThreadSearchBranch, this, std::ref(b_[i - gpu_cnt]), time_limit, i - gpu_cnt, is_ponder);
		}
	}

	for(std::thread& th : ths) th.join();
}


/**
 *  1000回プレイアウトを行い、最終結果を出力する
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

	PrintFinalScore(b, stat.game, stat.owner, win_pl, komi, log_path);

}


std::string Tree::BestSequence(int node_idx, int head_move, int max_move){

	string seq = "";
	Node* pn = &node[node_idx];
	if(head_move == VNULL && pn->prev_move[0] == VNULL) return seq;


	int head_move_ = (head_move == VNULL)? (int)pn->prev_move[0] : head_move;
	string head_str = CoordinateString(head_move_);
	if(head_str.length() == 2) head_str += " ";

	seq += head_str;

	std::vector<Child*> child_list;
	for(int i=0;i<max_move;++i){
		if(pn->child_cnt <= 1) break;
		SortChildren(pn, child_list);

		string move_str = CoordinateString(child_list[0]->move);
		if(move_str.length() == 2) move_str += " ";

		seq += "->";
		seq += move_str;

		if(	child_list[0]->is_next &&
			node[child_list[0]->next_idx].hash == child_list[0]->next_hash)
		{
			pn = &node[child_list[0]->next_idx];
		}
		else break;
	}

	// D4 ->D16->Q16->Q4 ->...
	return seq;

}


void Tree::PrintGFX(std::ostream& ost){

	double occupancy[BVCNT] = {0};
	double game_cnt = std::max(1.0, (double)stat.game[2]);
	for(int i=0;i<BVCNT;++i)
		occupancy[i] = (stat.owner[1][rtoe[i]]/game_cnt - 0.5) * 2;

	ost << "gogui-gfx:\n";
	ost << "VAR ";

	Node* pn = &node[root_node_idx];
	std::vector<Child*> child_list;
	for(int i=0;i<9;++i){
		if(pn->child_cnt <= 1) break;
		SortChildren(pn, child_list);

		int move = child_list[0]->move;

		string pl_str = (pn->pl == 1)? "b " : "w ";
		ost << pl_str << CoordinateString(move) << " ";

		if(move != PASS) occupancy[etor[move]] = 0;

		if(	child_list[0]->is_next &&
			node[child_list[0]->next_idx].hash == child_list[0]->next_hash)
		{
			pn = &node[child_list[0]->next_idx];
		}
		else break;
	}
	ost << endl;

	if(stat.game[2] == 0) return;

	ost << "INFLUENCE ";
	for(int i=0;i<BVCNT;++i){
		if(-0.2 < occupancy[i] && occupancy[i] < 0.2) continue;
		ost << CoordinateString(rtoe[i]) << " " << occupancy[i] << " ";
	}
	ost << endl;

}


void Tree::PrintChildInfo(int node_idx, std::ostream& ost){

	Node* pn = &node[node_idx];
	std::vector<Child*> rc;
	SortChildren(pn, rc);

	ost << "|move|count  |value|roll |prob |depth| best sequence" << endl;

	for(int i=0;i<std::min((int)pn->child_cnt, 10);++i) {

		Child* pc = rc[i];
		int game_cnt = std::max((int)pc->rollout_cnt, (int)pc->value_cnt);

		if (game_cnt == 0) break;

		double rollout_rate = (pc->rollout_win / std::max(1, (int)pc->rollout_cnt) + 1) / 2;
		double value_rate = (pc->value_win / std::max(1, (int)pc->value_cnt) + 1) / 2;
		if(pc->value_win == 0.0) value_rate = 0;
		if(pc->rollout_win == 0.0) rollout_rate = 0;

		int depth = 1;
		string seq;
		if(pc->is_next){
			std::unordered_set<int> node_list;
			depth = CollectNodeIndex((int)pc->next_idx, depth, node_list);
			seq = BestSequence((int)pc->next_idx, (int)pc->move);
		}

		ost << "|" << std::left << std::setw(4) << CoordinateString((int)pc->move);
		ost << "|" << std::right << std::setw(7) << std::min(9999999, game_cnt);
		auto prc = ost.precision();
		ost.precision(1);
		if(pc->value_cnt == 0) ost << "|" << std::setw(5) << "N/A";
		else ost << "|" << std::setw(5) << std::fixed << value_rate * 100;
		if(pc->rollout_cnt == 0) ost << "|" << std::setw(5) << "N/A";
		else ost << "|" << std::setw(5) << std::fixed << rollout_rate * 100;
		ost << "|" << std::setw(5) << std::fixed << (double)pc->prob * 100;
		ost.precision(prc);
		ost << "|" << std::setw(5) << depth;
		ost << "| " << seq;
		ost << endl;

	}

}
