#pragma once

#include <atomic>
#include <array>
#include <deque>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "board.h"
#include "playout.h"
#include "feed_tensor.h"
#include "nueral_net.h"
#include "node.h"


/**************************************************************
 *
 *  �T���؂̃N���X.
 *
 *  �u���\�Ɋ�Â��A�ǖʂ̕]����T���؂̊g�����s��.
 *  �j���[�����l�b�g�ɂ��ǖʕ]���ƃv���C�A�E�g�͔񓯊��Ɏ��s�����.
 *
 *  Class of search tree.
 *
 *  Based on the transposition table, evaluate boards and expand nodes.
 *  Board evaluation and roll out by the the neural network are performed
 *  asynchronously.
 *
 ***************************************************************/
class Tree {
public:

	// �f���̃m�[�h����\�������u���\.
	// Transposition table consisting of prime number of Node.
	std::vector<Node> node;

	// �u���\�̃T�C�Y
	// Size of the transposition table.
	const int node_limit = 65537;		// ~2.6GB
	// const int node_limit = 32771;	// ~1.3GB
	// const int node_limit = 16411;	// ~650MB

	std::unordered_map<int64, int> node_hash_list;

	int node_cnt;		// Number of registered nodes.
	int node_depth;		// Maximum depth of the tree.
	int root_node_idx;	// Index of the root node.

	// ValueNet�]����҂��̓e���\���̑o�����L���[
	// Double-ended queue for the input tensor of the value network.
	std::deque<ValueEntry> value_que;

	// PolicyNet�]����҂��̓e���\���̑o�����L���[
	// Double-ended queue for the input tensor of the policy network.
	std::deque<PolicyEntry> policy_que;

	std::atomic<int> value_que_cnt;		// Size of value_que.
	std::atomic<int> policy_que_cnt;	// Size of policy_que.
	int eval_policy_cnt;	// Number of FeedTensor evaluated by PolicyNet() while thinking.
	int eval_value_cnt;		// Number of FeedTensor evaluated by ValueNet() while thinking.

	std::mutex mtx_node, mtx_lgr, mtx_pque, mtx_4, mtx_vque;

	int vloss_cnt;	// Number of virtual loss.
	double lambda;	// Mixing parameter of value and rollout result.
	double cp;		// Gain parameter that determine contribution of probability values.

	int thread_cnt;
	int gpu_cnt;
	double policy_temp;		// Softmax temperature for the policy network.
	int sym_idx;			// Symmetrical index for the policy and value network.
	std::vector<tensorflow::Session*> sess_policy;
	std::vector<tensorflow::Session*> sess_value;

	double komi;
	std::atomic<int> move_cnt;
	int expand_cnt;
	LGR lgr;
	Statistics stat;

	double main_time;
	double byoyomi;
	std::atomic<double> left_time;
	int extension_cnt;
	std::atomic<bool> stop_think;

	std::ofstream* log_file;

	Tree();
	Tree(std::vector<std::string>& sl_path, std::vector<std::string>& vl_path);
	void SetGPU(std::vector<std::string>& sl_path, std::vector<std::string>& vl_path);
	void InitBoard();
	void Clear();

	void AddPolicyQue(int node_idx, Board& b);
	void AddValueQue(std::vector<std::pair<int,int>>& upper_list, Board& b);

	int CreateNode(Board& b);
	void UpdateNodeProb(int node_idx, std::array<double, EBVCNT>& prob_list);
	int CollectNodeIndex(int node_idx, int depth, std::unordered_set<int>& node_list);
	void DeleteNodeIndex(int node_idx);
	int UpdateRootNode(Board& b);
	double BranchRate(Child* pc);

	double SearchBranch(Board& b, int node_idx, double& value_result,
		std::vector<std::pair<int, int>>& search_route, LGR& lgr_, Statistics& stat_);

	int SearchTree(Board& b, double time_limit, double& win_rate,
					bool is_errout, bool is_ponder=false);

	void ThreadSearchBranch(Board& b, double time_limit, bool is_ponder=false);
	void ThreadEvaluate(double time_limit, int gpu_idx, bool is_ponder=false);
	void ParallelSearch(double time_limit, Board& b, bool is_ponder);

	void PrintResult(Board& b);

};

void PrintLog(std::ofstream* log_path, const char* output_text, ...);
void SortChildren(Node* pn, std::vector<Child*>& child_list);
std::string CoordinateString(int v);

extern double cfg_main_time;
extern double cfg_byoyomi;
extern int cfg_thread_cnt;
extern int cfg_gpu_cnt;
extern double cfg_komi;
extern bool self_match;
extern bool is_master;
extern bool is_worker;
extern int cfg_worker_cnt;
extern bool cfg_mimic;
extern bool never_resign;
extern std::string resume_sgf_path;
extern std::string pb_dir;
