#include <iostream>
#include "cluster.h"

using std::string;
using std::cerr;
using std::endl;


int Process::Start(const char* const file, char* const argv[]) {

#ifdef _WIN32
	cerr << "AQ cannot run culuster on Windows." << endl;
	return -1;
#else
	int pipe_from[2];
	int pipe_to[2];

	// 1. パイプの作成（親プロセス->子プロセス）
	//    Create a pipe (parent process -> child process)
	if (pipe(pipe_from) < 0) {
		std::perror("process error on creating pipe_from.\n");
		return -1;
	}

	// 2. パイプの作成（子プロセス->親プロセス）
	//    Create a pipe (child process -> parent process)
	if (pipe(pipe_to) < 0) {
		std::perror("process error on creating pipe_to.\n");
		close(pipe_from[0]);
		close(pipe_from[1]);
		return -1;
	}

	// 3. 子プロセスの作成
	//    Create a child process.
	pid_t process_id = fork();
	if (process_id < 0) {
		std::perror("fork() failed.\n");
		close(pipe_from[0]);
		close(pipe_from[1]);
		close(pipe_to[0]);
		close(pipe_to[1]);
		return -1;
	}

	if (process_id == 0) {
		// 4. 使用しないパイプを終了
		//    Close unused pipes.
		close(pipe_to[1]);
		close(pipe_from[0]);

		// 5. 標準入力に割り当てる
		//    Assign to standard input.
		dup2(pipe_to[0], STDIN_FILENO);
		dup2(pipe_from[1], STDOUT_FILENO);

		// 6. 割り当て済のパイプを閉じる
		//    Close allocated pipes.
		close(pipe_to[0]);
		close(pipe_from[1]);

		// 7. 子プロセスでプログラムを起動する
		//    Start program in the child process
		if (execvp(file, argv) < 0) {
			std::perror("execvp() failed.\n");
			return -1;
		}
	}

	// 8. パイプをファイルストリームとして開く
	//    Open pipe as file stream.
	stream_to = fdopen(pipe_to[1], "w");
	stream_from = fdopen(pipe_from[0], "r");

	// 9. バッファリングをオフにする
	//    Turn off buffering.
	std::setvbuf(stream_to, NULL, _IONBF, 0);
	std::setvbuf(stream_from, NULL, _IONBF, 0);

	return process_id;
#endif

}


struct BranchInfo{
	int move;
	int rollout_cnt;
	double win_rate;
	double ratio;
	BranchInfo(){
		move = PASS;
		rollout_cnt = 0;
		win_rate = 0.0;
		ratio = 0.0;
	};
};


void Cluster::LaunchWorkers(){

	process_list.clear();
	std::vector<bool> is_ready;

	// 1. ワーカーを起動. Launch workers.
	for(int i=0;i<worker_cnt;++i){
		Process process_;
		// Use list in ~/.ssh/config
		std::string worker_name = "remote-" + std::to_string(i);

		string command_ = "export LD_LIBRARY_PATH=/usr/local/cuda/lib64; ";
		command_ += "cd AQ; ";
		command_ += "./AQ";

		char* const args[] = {
			const_cast<char*>("ssh"),
			const_cast<char*>(worker_name.c_str()),
			const_cast<char*>(command_.c_str()),
			NULL
		};
		if (process_.Start(args[0], args) < 0) {
			fprintf(stderr, "%s could not start.\n", worker_name.c_str());
		}

		process_list.push_back(process_);
		is_ready.push_back(false);
	}

	// 2. ワーカーがすべて待機状態になるまで待つ
	//    Wait until all workers are in standby state.
	auto t1 = std::chrono::system_clock::now();
	for (;;) {
		bool are_all_ready = true;
		for(int i=0;i<worker_cnt;++i){
			if(is_ready[i] == false){
				process_list[i].SendLine("isready");
				std::string line;
				process_list[i].GetLine(&line);
				if (line == "= readyok"){
					is_ready[i] = true;
				}
				else are_all_ready = false;
			}
		}
		if(are_all_ready){
			cerr << "all workers are ready." << endl;
			break;
		}

#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		auto t2 = std::chrono::system_clock::now();
		double elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
		// 待っても応答がなければ次に進む
		// If not receive a response after 60 sec.
		if(elapsed_time > 60){
			cerr << "worker launch time up." << endl;
			break;
		}
	}

	// 3. ワーカーにponderを開始させる
	//    Make worker start ponder.
	for(int i=0;i<worker_cnt;++i) is_ready[i] = false;
	t1 = std::chrono::system_clock::now();
	for (;;) {
		bool are_all_ready = true;
		for(int i=0;i<worker_cnt;++i){
			if(is_ready[i] == false){
				process_list[i].SendLine("ponder");
				std::string line;
				process_list[i].GetLine(&line);
				if (line == "= ponder started."){
					is_ready[i] = true;
				}
				else are_all_ready = false;
			}
		}
		if(are_all_ready){
			cerr << "all workers start ponder." << endl;
			break;
		}

#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		auto t2 = std::chrono::system_clock::now();
		double elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
		// 待っても応答がなければ次に進む
		// If not receive a response after 30 sec.
		if(elapsed_time > 30){
			cerr << "worker ponder time up." << endl;
			break;
		}
	}

}

int Cluster::Consult(Tree& tree, std::ofstream* log_file){

	SendCommand("bestbranch");
	std::vector<bool> is_ready;
	for(int i=0;i<worker_cnt;++i){
		is_ready.push_back(false);
	}

	std::vector<BranchInfo> binfo_list;
	auto t1 = std::chrono::system_clock::now();

	for(;;){
		bool are_all_ready = true;
		for(int i=0;i<worker_cnt;++i){
			if(is_ready[i] == false){
				std::string line;
				process_list[i].GetLine(&line);

				if (line.find("bestbranch") != string::npos){
					std::vector<string> str_list;
					SplitString(line, " ", str_list);
					BranchInfo binfo;
					if(str_list.size() == 6){
						int move_cnt = stoi(str_list[1]);
						if(move_cnt != tree.move_cnt){
							are_all_ready = false;
							continue;
						}
						binfo.move = stoi(str_list[2]);
						binfo.rollout_cnt = stoi(str_list[3]);
						binfo.win_rate = stod(str_list[4]);
						binfo.ratio = stod(str_list[5]);
						binfo_list.push_back(binfo);
						is_ready[i] = true;
					}
					else{
						process_list[i].SendLine("bestbranch");
						are_all_ready = false;
					}

				}
				else{
					process_list[i].SendLine("bestbranch");
					are_all_ready = false;
				}
			}
		}
		auto t2 = std::chrono::system_clock::now();
		double dt = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
		if(are_all_ready || dt > 2.0) break;
	}

	Node *pn = &tree.node[tree.root_node_idx];
	std::vector<Child*> rc;
	tree.SortChildren(pn, rc);
	Child *rc0 = rc[0];

	double win_rate = tree.BranchRate(rc0);
	double ratio = 10.0;
	if(pn->child_cnt > 1){
		auto rc1 = rc[1];
		int rc0_cnt = std::max((int)rc0->rollout_cnt, (int)rc0->value_cnt);
		int rc1_cnt = std::max((int)rc1->rollout_cnt, (int)rc1->value_cnt);
		ratio = (double)rc0_cnt/std::max(1, rc1_cnt);
	}

	BranchInfo binfo;
	binfo.move = (int)rc0->move;
	binfo.rollout_cnt = (int)rc0->rollout_cnt;
	binfo.win_rate = win_rate;
	binfo.ratio = ratio;
	binfo_list.push_back(binfo);

	for(int i=0, n=(int)binfo_list.size();i<n;++i){
		if(i == n - 1){
			PrintLog(log_file, "master: ");
		}
		else{
			PrintLog(log_file, "worker-%d: ", i);
		}
		PrintLog(log_file, "%s %d[cnt] %.1f[%%] %.2f\n",
				CoordinateString(binfo_list[i].move).c_str(),
				binfo_list[i].rollout_cnt,
				binfo_list[i].win_rate * 100,
				binfo_list[i].ratio);
	}

	int best_move = PASS;
	int max_cnt = 0;
	double max_rate = 0.;

	for(int i=0, n=(int)binfo_list.size();i<n;++i){
		int conc_cnt = 0;
		double conc_rate = 0.;
		for(int j=0;j<n;++j){
			if(binfo_list[i].move == binfo_list[j].move){
				++conc_cnt;
				conc_rate = (conc_rate * (conc_cnt - 1) + binfo_list[j].win_rate) / conc_cnt;
			}
		}
		if(conc_cnt > max_cnt ||
			(conc_cnt == max_cnt && conc_rate > max_rate)){
			best_move = binfo_list[i].move;
			max_cnt = conc_cnt;
			max_rate = conc_rate;
		}
	}

	return best_move;

}
