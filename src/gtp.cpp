#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>

#include "gtp.h"

using std::string;
using std::cerr;
using std::endl;

bool save_log = true;
bool need_time_control = true;
bool use_pondering = true;


bool FindStr(string str, string s1){
	return 	str.find(s1) != string::npos;
}
bool FindStr(string str, string s1, string s2){
	return 	str.find(s1) != string::npos ||
			str.find(s2) != string::npos;
}
bool FindStr(string str, string s1, string s2, string s3){
	return 	str.find(s1) != string::npos ||
			str.find(s2) != string::npos ||
			str.find(s3) != string::npos;
}
bool FindStr(string str, string s1, string s2, string s3, string s4){
	return 	str.find(s1) != string::npos ||
			str.find(s2) != string::npos ||
			str.find(s3) != string::npos ||
			str.find(s4) != string::npos;
}


void SendGTP(const char* output_str, ...){

	va_list args;
	va_start(args, output_str);
	vfprintf(stdout, output_str, args);
	va_end(args);

}

void SendCommandList(){
	SendGTP("= boardsize\n");
	SendGTP("list_commands\n");
	SendGTP("clear_board\n");
	SendGTP("genmove\n");
	SendGTP("play\n");
	SendGTP("quit\n");
	SendGTP("time_left\n");
	SendGTP("time_settings\n");
	SendGTP("name\n");
	SendGTP("protocol_version\n");
	SendGTP("version\n");
	SendGTP("komi\n");
	SendGTP("final_score\n");
	SendGTP("kgs-time_settings\n");
	SendGTP("kgs-game_over\n");
	SendGTP("place_free_handicap\n");
	SendGTP("fixed_handicap\n");
	SendGTP("set_free_handicap\n");
	SendGTP("gogui-play_sequence\n");
	SendGTP("= \n\n");
}

void ReadGTP(std::string& input_str){ getline(std::cin, input_str); }


int CallGTP(){

	string gtp_str;
	std::vector<string> split_list;
	int pl = 0;
	bool is_playing = false;
	double win_rate;
	bool play_mimic = cfg_mimic;
	Board b;
	Tree tree;
	Cluster cluster;
#ifdef _WIN32
	// Cluster cannot be used on Windows.
	is_master = false;
	is_worker = false;
#endif

	// 1. masterの場合、子プロセスを起動する.
	//    Launch workers if master of the cluster.
	if(is_master) cluster.LaunchWorkers();

	// 2. ログファイルのパス
	//    Set file path of log.
	int file_cnt = 0;
	if(save_log){
		std::stringstream ss;
		ss << working_dir << "log" << spl_str << file_cnt << ".txt";
		tree.log_file = new std::ofstream(ss.str(), std::ofstream::out);
	}

	// 3. 入力棋譜がある場合は読みだす
	//    Resume game if resume_sgf_path is set.
	SgfData sgf;
	if(resume_sgf_path != ""){
		SgfData sgf_read;
		sgf_read.ImportData(working_dir + resume_sgf_path);
		sgf_read.GenerateBoard(b, sgf_read.move_cnt);
		sgf = sgf_read;
	}

#ifdef OnlineMatch
	SendCommandList();
#endif

	// 4. GTPプロトコルによる通信を開始する
	//    Start communication with the GTP protocol.
	for (;;) {
		gtp_str = "";

		// ポンダー中にコマンドを監視するスレッド
		// Thread that monitors GTP commands during pondering.
		std::thread read_th([&gtp_str, &is_playing, &b, &tree]{
			while(gtp_str == ""){
				ReadGTP(gtp_str);
				if(	gtp_str != "" && is_playing &&
					b.prev_move[b.her] < PASS)
				{
					tree.stop_think = true;
					break;
				}
			}
		});

		// GTPコマンドが送られてくるまでポンダー
		// Ponder until GTP commands are sent.
		if(	is_playing && b.prev_move[b.her] != PASS &&
			(tree.left_time > 25 || tree.byoyomi != 0) &&
			use_pondering)
		{
			double time_limit = 100.0;
			tree.SearchTree(b, time_limit, win_rate, false, true);
		}

		read_th.join();
		tree.stop_think = false;

		// GTPコマンドの処理
		// Process GTP command.
		if (gtp_str == "" || gtp_str == "\n"){
			continue;
		}
		else if (FindStr(gtp_str, "name")) SendGTP("= AQ\n\n");
		else if (FindStr(gtp_str, "protocol_version")) SendGTP("= 2.0\n\n");
		else if (FindStr(gtp_str, "version")) SendGTP("= 2.1.1\n\n");
		else if (FindStr(gtp_str, "boardsize")) {
			// Board size setting. (only corresponding to 19 size)
			// "=boardsize 19", "=boardsize 13", ...
			SendGTP("= \n\n");
			cerr << "AQ corresponds only to the board with 19 size." << endl;
		}
		else if (FindStr(gtp_str, "list_commands"))
		{
			// 対応しているコマンド一覧を送る
			// Send the corresponding command list.
			SendCommandList();
		}
		else if (FindStr(gtp_str, "clear_board"))
		{
			// 盤面を初期化する.
			// Initialize the board.
			b.Clear();
			tree.InitBoard();
			sgf.Clear();

			if(is_master) cluster.SendCommand("clear_board");

			// 棋譜から再開 Resume from SGF file.
			if(resume_sgf_path != ""){
				SgfData sgf_read;
				sgf_read.ImportData(working_dir + resume_sgf_path);
				sgf_read.GenerateBoard(b, sgf_read.move_cnt);
				sgf = sgf_read;
				tree.UpdateRootNode(b);

				if(is_master){
					cerr << "Cluster cannot resume from SGF file.\n";
					break;
				}
			}

			if(save_log){
				++file_cnt;
				std::stringstream ss;
				ss << working_dir << "log" << spl_str << file_cnt << ".txt";
				if(tree.log_file != NULL) tree.log_file->close();
				tree.log_file = new std::ofstream(ss.str(), std::ofstream::out);
			}

			is_playing = is_worker? true : false;
			play_mimic = cfg_mimic;

			SendGTP("= \n\n");
			cerr << "clear board." << endl;
		}
		else if (FindStr(gtp_str, "komi"))
		{
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			double komi_ = stod(split_list[1]);
			cfg_komi = (komi_ == 0)? 0.5 : komi_;
			tree.komi = cfg_komi;

			if(is_master) cluster.SendCommand(gtp_str);

			SendGTP("= \n\n");
			fprintf(stderr, "set komi=%.1f.\n", tree.komi);
		}
		else if (FindStr(gtp_str, "time_left"))
		{
			// 残り時間を設定する
			// Set remaining time.
			// "=time_left B 944", "=time_left white 300", ...
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			int left_time = stoi(split_list[2]);
			if(	(pl == 0 && FindStr(split_list[1], "W", "w")) ||
				(pl == 1 && FindStr(split_list[1], "B", "b")) )
			{
				tree.left_time = (double)left_time;
				std::fprintf(stderr, "left time: %d[sec]\n", left_time);
			}

			need_time_control = false;

			SendGTP("= \n\n");
		}
		else if (FindStr(gtp_str, "genmove")) {
			// 次の手を考えて送信する.
			// Think and send the next move.
			// "=genmove b", "=genmove white", ...

			auto t1 = std::chrono::system_clock::now();
			//cerr << "thinking...\n";

			pl = FindStr(gtp_str, "B", "b")? 1 : 0;
			if(pl != b.my){
				// Insert pass if the turn is different.
				b.PlayLegal(PASS);
				sgf.AddMove(PASS);
				tree.UpdateRootNode(b);
				--b.pass_cnt[b.her];
			}

			is_playing = true;
			int next_move;
			tree.stop_think = false;
			bool think_full = true;


			if(play_mimic && b.my == 0){
				// a. マネ碁フラグが立っているときマネ碁をする.
				//    Play mimic move if play_mimic is true.
				int v = b.prev_move[b.her];

				if(DistBetween(v, EBVCNT/2) < 5 || v == PASS){
					// 中央付近に打たれたら自力で考える.
					// Think by itself if the previous move is near the center.
					play_mimic = false;
				}
				else{
					next_move = tree.SearchTree(b, 1.0, win_rate, true, false);
					if(win_rate >= 0.65){
						// 勝率が65%を超えるならマネ碁を終了する
						// Think by itself if the winning rate is over 65%.
						play_mimic = false;
					}
					else{
						// マネ碁する手を求める. Set mimic move.
						int x = EBSIZE - 1 - etox[v];
						int y = EBSIZE - 1 - etoy[v];
						if(b.IsLegal(b.my, xytoe[x][y])){
							next_move = xytoe[x][y];
							think_full = false;
						}
					}
				}
			}

			if(think_full){
				// b. 最善手を求める.
				//    Search for the best move.
				next_move = tree.SearchTree(b, 0.0, win_rate, true, false);

				if(	is_master && next_move != PASS 	&&
					b.prev_move[b.her] != PASS		&&
					(tree.left_time > 25 || tree.byoyomi != 0))
				{
					// 合議の結果を反映する
					// Reflect the result of consultation.
					next_move = cluster.Consult(tree, tree.log_file);
				}
			}
#ifdef OnlineMatch
			double resign_value = 0.2;
#else
			double resign_value = 0.1;
#endif
			if(b.IsMimicGo()){ next_move = EBVCNT/2; }
			else if(win_rate < resign_value){

				// 1000回プレイアウトして本当に負けているか確認する
				// Roll out 1000 times to check if really losing.
				Board b_;
				int win_cnt = 0;
				for(int i=0;i<1000;++i){
					b_ = b;
					int result = PlayoutLGR(b_, tree.lgr, tree.komi);
					if(b.my == std::abs(result)) ++win_cnt;
				}
				if((double)win_cnt / 1000 < 0.3) next_move = PASS;
			}

			// c. 局面を進める. Play the move.
			b.PlayLegal(next_move);
			tree.UpdateRootNode(b);

			// d. ログファイルを更新する. Update logs.
			sgf.AddMove(next_move);
			if(!is_worker){

				PrintBoard(b, next_move);
				if(tree.log_file != NULL){
					PrintBoard(b, next_move, tree.log_file);
					//PrintOccupancy(tree.stat.game, tree.stat.owner, tree.log_file);
				}

				if(save_log){
					std::stringstream ss;
					ss << working_dir << "log" << spl_str << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}

				if(b.prev_move[b.her] == PASS && b.prev_move[b.my] == PASS){
					if(tree.left_time > 5 || tree.byoyomi != 0){
						tree.PrintResult(b);
					}
					else{
						SendGTP("= pass\n\n");
						tree.PrintResult(b);
						continue;
					}
				}
			}

			// e. 着手のGTPコマンドを送る. Send response of the next move.
			string str_nv = CoordinateString(next_move);
			if(next_move == PASS){
				if(!never_resign && win_rate < resign_value) SendGTP("= resign\n\n");
				else SendGTP("= pass\n\n");
			}
			else{
				SendGTP("= %s\n\n", str_nv.c_str());
			}

			// f. 子プロセスに手を送信する.
			//    Send play command to the remote processes.
			if(is_master){
				string cmd_str = (b.my == 0)? "play b " : "play w ";
				cmd_str += str_nv;
				cluster.SendCommand(cmd_str);
			}

			// g. 残り持ち時間を更新する. Update remaining time.
			if(need_time_control){
				auto t2 = std::chrono::system_clock::now();
				double elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
				tree.left_time = std::max(0.0, (double)tree.left_time - elapsed_time);
			}

		}
		else if (FindStr(gtp_str, "play "))
		{
			// 手を受信し、盤面に反映する. "play w D4" のように来る
			// Receive the opponent's move and reflect on the board.
			// "=play w D4", "play b pass", ...

			int next_move;

			// a. 文字列を解析する.
			//    Analyze received string.
			if(FindStr(gtp_str, "pass", "Pass", "PASS")){
				next_move = PASS;
			}
			else if(FindStr(gtp_str, "resign")){
				next_move = PASS;
				is_playing = false;
			}
			else{
				SplitString(gtp_str, " ", split_list);
				if(split_list[0] == "=") split_list.erase(split_list.begin());
				string str_x = split_list[2].substr(0, 1);
				string str_y = split_list[2].substr(1);

				string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

				int x = int(x_list.find(str_x)) % 19 + 1;
				int y = stoi(str_y);

				next_move = xytoe[x][y];
			}

			// b. 異なる手番の石を配置する前にパスを挿入する
			//    Insert pass before placing a opponent's stone.
			if(	(b.my == 0 && FindStr(gtp_str, "play b", "play B")) ||
				(b.my == 1 && FindStr(gtp_str, "play w", "play W"))	)
			{
				b.PlayLegal(PASS);
				sgf.AddMove(PASS);
				--b.pass_cnt[b.her];
			}

			if(!is_worker && save_log){
				std::stringstream ss;
				tree.PrintChildInfo(tree.root_node_idx, next_move, ss, true);
				std::ofstream& ofs = *tree.log_file;
				ofs << ss.str();
				std::cerr << ss.str();
			}

			// c. 局面を進める. Play the move.
			b.PlayLegal(next_move);
			tree.UpdateRootNode(b);

			// d. ログファイルを更新する. Update logs.
			sgf.AddMove(next_move);
			if(!is_worker){
				if(tree.log_file != NULL) PrintBoard(b, next_move, tree.log_file);
				PrintBoard(b, next_move);

				if(save_log){
					std::stringstream ss;
					ss << working_dir << "log" << spl_str << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}

				if(b.prev_move[b.her] == PASS && b.prev_move[b.my] == PASS){
					if(tree.left_time > 5 || tree.byoyomi != 0){
						tree.PrintResult(b);
					}
					else{
						SendGTP("= \n\n");
						tree.PrintResult(b);
						continue;
					}

				}
			}

			// e. GTPコマンドを送信. Send GTP response.
			SendGTP("= \n\n");

			// f. マネ碁をしているとき、相手がツケてきたらやめる
			//    Stop mimic go if the opponent's move is Tsuke.
			if(play_mimic && b.move_cnt < 11 && next_move < PASS){
				int my_color = b.my + 2;
				for(auto dv: VSHIFT){
					if(b.color[next_move + dv] == my_color){
						play_mimic = false;
					}
				}
			}

			if(is_master) cluster.SendCommand(gtp_str);

		}
		else if (FindStr(gtp_str, "undo", "Undo"))
		{
			// 局面を１手戻す. Undo the previous move.

			std::vector<int> move_history;
			for(auto v_hist: b.move_history) move_history.push_back(v_hist);
			if(!move_history.empty()) move_history.pop_back();

			// a. 局面を初期化. Initialize board.
			b.Clear();
			tree.Clear();
			sgf.Clear();
			if(is_master) cluster.SendCommand("clear_board");

			// b. 局面を一手前まで進める.
			//    Advance the board to the previous state.
			for(auto v_hist: move_history){
				b.PlayLegal(v_hist);
				sgf.AddMove(v_hist);

				if(is_master){
					int x = v_hist % EBSIZE - 1;
					int y = v_hist / EBSIZE - 1;
					std::stringstream ss;
					string pl_str = "w";
					if(b.her != 0) pl_str = "b";
					string xlist = "ABCDEFGHJKLMNOPQRST";
					ss << "play " << pl_str << " " << xlist[x] << y+1;
					cluster.SendCommand(ss.str());
				}
			}
			tree.UpdateRootNode(b);

			// c. ログファイルを更新する. Update logs.
			if(!is_worker){
				if(save_log){
					std::stringstream ss;
					ss << working_dir << "log" << spl_str << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}
			}

			// d. GTPコマンドを送信. Send GTP response.
			SendGTP("= \n\n");
		}
		else if(FindStr(gtp_str, "final_score")) {
			// 1000回プレイアウトを行い、最終的なスコアを求める
			// Roll out 1000 times and return the final score.

			// a. Roll out 1000 times.
			tree.stat.Clear();
			int win_cnt = 0;
			int rollout_cnt = 1000;
			for(int i=0;i<rollout_cnt;++i){
				Board b_cpy = b;
				int result = PlayoutLGR(b_cpy, tree.lgr, tree.stat, tree.komi);
				if(result != 0) ++win_cnt;
			}
			bool is_black_win = ((double)win_cnt/rollout_cnt >= 0.5);

			// b. Calculate scores in Chinese rule.
			double score[2] = {0.0, 0.0};
			for(int i=0;i<BVCNT;++i){
				int v = rtoe[i];
				if((double)tree.stat.owner[0][v]/tree.stat.game[2] > 0.5){
					++score[0];
				}
				else ++score[1];
			}
			double final_score = std::abs(score[1] - score[0] - tree.komi);

			// c. Send GTP response.
			string win_pl = is_black_win? "B+" : "W+";
			std::stringstream ss;
			ss << std::fixed << std::setprecision(1) << final_score;
			win_pl += ss.str();
			if(final_score == 0) win_pl = "0";

			SendGTP("= %s\n\n", win_pl.c_str());
		}
		else if(FindStr(gtp_str, "isready")) {
			SendGTP("= readyok\n");
		}
		else if(FindStr(gtp_str, "ponder")) {
			is_playing = true;
			SendGTP("= ponder started.\n");
		}
		else if(FindStr(gtp_str, "bestbranch")) {
			// 最善手を送信する
			// Send the best move.

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

			std::stringstream ss;
			ss 	<< "bestbranch " << tree.move_cnt
				<< " " << (int)rc0->move
				<< " " << (int)rc0->rollout_cnt
				<< " " << win_rate
				<< " " << ratio
				<< endl;

			SendGTP(ss.str().c_str());
		}
		else if (FindStr(gtp_str, "chid_info")) {

			tree.SearchTree(b, 0.0, win_rate, false, false);
			std::stringstream ss;
			tree.PrintChildInfo(tree.root_node_idx, ss);
			SendGTP("= %s\n\n", ss.str().c_str());

		}
		else if (FindStr(gtp_str, "analyze")) {
			// N秒思考して読み筋を表示する
			// Think in N seconds and display top 10 moves.
			// "= analyze 60" -> think 60 sec.
			// "= analyze" -> think with default time setting.
			// "= analyze -1" -> returns moves without thinking.

			SplitString(gtp_str, " ", split_list);
			if (split_list[0] == "=") split_list.erase(split_list.begin());

			double think_time = 0.0;
			if (split_list.size() >= 2) think_time = stod(split_list[1]);

			if(think_time >= 0) tree.SearchTree(b, think_time, win_rate, false, true);
			tree.PrintChildInfo(tree.root_node_idx, std::cerr);
			SendGTP("= \n\n");

		}
		else if (FindStr(gtp_str, "kgs-time_settings"))
		{
			// 時間を設定する
			// Set main and byoyomi time.
			// "=kgs-time_settings byoyomi 30 60 3", ...
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			if(FindStr(gtp_str, "byoyomi") && split_list.size() >= 4){
				tree.main_time = (double)stoi(split_list[2]);
				tree.left_time = tree.main_time;
				tree.byoyomi = (double)stoi(split_list[3]);
			}
			else{
				tree.main_time = (double)stoi(split_list[2]);
				tree.left_time = tree.main_time;
			}

			SendGTP("= \n\n");
		}
		else if (FindStr(gtp_str, "time_settings"))
		{
			// 時間を設定する
			// Set main and byoyomi time.
			// "=time_settings 30 60 3", ...
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			tree.main_time = (double)stoi(split_list[1]);
			tree.left_time = tree.main_time;
			tree.byoyomi = (double)stoi(split_list[2]);

			SendGTP("= \n\n");
		}
		else if (FindStr(gtp_str, "set_free_handicap"))
		{
			// 置き石を配置する
			// "=set_free_handicap D4 ..."
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			if(split_list.size() >= 2){
				int i_max = (int)split_list.size();
				for(int i=1;i<i_max;++i){
					string str_x = split_list[i].substr(0, 1);
					string str_y = split_list[i].substr(1);

					string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

					int x = int(x_list.find(str_x)) % 19 + 1;
					int y = stoi(str_y);

					int next_move = xytoe[x][y];
					b.PlayLegal(next_move);
					sgf.AddMove(next_move);

					b.PlayLegal(PASS);
					sgf.AddMove(PASS);
					--b.pass_cnt[0];
				}

				if(tree.komi != 0.5){
					tree.komi = 0.5;
					cerr << "set komi=0.5.\n";
				}
			}

			if(is_master) cluster.SendCommand(gtp_str);
			SendGTP("= \n\n");
			cerr << "set free handicap.\n";
		}
		else if (FindStr(gtp_str, "fixed_handicap") || FindStr(gtp_str, "place_free_handicap"))
		{
			// 置き石を配置する. Place fixed handicap stones.
			// "=fixed_handicap 2"
			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			if(split_list.size() >= 2){
				int x_[9] = {4,16,4,16,4,16,10,10,10};//{16,4,16,4,16,4,10,10,10};
				int y_[9] = {4,16,16,4,10,10,4,16,10};//{16,4,4,16,10,10,16,4,10};
				int stones[8][9] = {{0,1},{0,1,2},{0,1,2,3},{0,1,2,3,8},
									{0,1,2,3,4,5}, {0,1,2,3,4,5,8},{0,1,2,3,4,5,6,7},
									{0,1,2,3,4,5,6,7,8}};
				int hc_cnt = stoi(split_list[1]);
				int hc_idx = hc_cnt = 1;
				for(int i=0;i<hc_cnt;++i){
					int stone_idx = stones[hc_idx][i];
					int v = xytoe[x_[stone_idx]][y_[stone_idx]];
					b.PlayLegal(v);
					sgf.AddMove(v);

					b.PlayLegal(PASS);
					sgf.AddMove(PASS);
					--b.pass_cnt[0];
				}

				if(tree.komi != 0.5){
					tree.komi = 0.5;
					cerr << "set komi=0.5.\n";
				}
			}

			if(is_master) cluster.SendCommand(gtp_str);
			SendGTP("= \n\n");
			cerr << "placed handicap stones.\n";
		}
		else if (FindStr(gtp_str, "gogui-play_sequence"))
		{
			// 引き継ぎ対局で、すべての手を受信する
			// "=gogui-play_sequence B R16 W D16 B Q3 W D3 ..."

			SplitString(gtp_str, " ", split_list);
			if(split_list[0] == "=") split_list.erase(split_list.begin());

			int i_max = (int)split_list.size();
			for(int i=2;i<i_max;i=i+2){
				int pl = (FindStr(split_list[i-1], "B", "b"))? 1 : 0;
				int next_move = PASS;
				if(b.my != pl){
					b.PlayLegal(PASS);
					sgf.AddMove(PASS);
					--b.pass_cnt[pl];
				}
				if(!FindStr(split_list[i], "PASS", "Pass", "pass")){
					string str_x = split_list[i].substr(0, 1);
					string str_y = split_list[i].substr(1);

					string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

					int x = int(x_list.find(str_x)) % 19 + 1;
					int y = stoi(str_y);

					next_move = xytoe[x][y];
				}

				// 局面を進める. Play the move.
				b.PlayLegal(next_move);
				// ログファイルを更新する. Update logs.
				sgf.AddMove(next_move);
				if(!is_worker){
					if(tree.log_file != NULL) PrintBoard(b, next_move, tree.log_file);
				}
			}

			tree.UpdateRootNode(b);
			if(is_master) cluster.SendCommand(gtp_str);
			if(!is_worker && save_log){
				std::stringstream ss;
				ss << working_dir << "log" << spl_str << file_cnt << ".sgf";
				sgf.ExportData(ss.str());
			}

			SendGTP("= \n\n");
			cerr << "sequence loaded." << endl;
		}
		else if(FindStr(gtp_str, "kgs-game_over")){
			is_playing = false;
			SendGTP("= \n\n");
		}
		else if(FindStr(gtp_str, "quit")){
			if(is_master) cluster.SendCommand("quit");
			if(!is_worker) tree.PrintResult(b);
			SendGTP("= \n\n");
			break;
		}
		else{
			SendGTP("= \n\n");
			cerr << "unknown command.\n";
		}
	}
	return 0;

}
