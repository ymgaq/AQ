#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>
#include <ostream>

#include "gtp.h"

using std::string;
using std::cerr;
using std::endl;

bool save_log = true;
bool need_time_controll = false;


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

	// 1. master�̏ꍇ�A�q�v���Z�X���N������.
	//    Launch workers if master of the cluster.
	if(is_master) cluster.LaunchWorkers();

	// 2. ���O�t�@�C���̃p�X
	//    Set file path of log.
	int file_cnt = 0;
	if(save_log){
		std::stringstream ss;
		ss << "./log/log_" << file_cnt << ".txt";
		tree.log_file = new std::ofstream(ss.str(), std::ofstream::out);
	}

	// 3. ���͊���������ꍇ�͓ǂ݂���
	//    Resume game if resume_sgf_path is set.
	SgfData sgf;
	if(resume_sgf_path != ""){
		SgfData sgf_read;
		sgf_read.ImportData(resume_sgf_path);
		sgf_read.GenerateBoard(b, sgf_read.move_cnt);
		sgf = sgf_read;
	}

	// 4. GTP�v���g�R���ɂ��ʐM���J�n����
	//    Start communication with the GTP protocol.
	for (;;) {
		gtp_str = "";

		// �|���_�[���ɃR�}���h���Ď�����X���b�h
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

		// GTP�R�}���h�������Ă���܂Ń|���_�[
		// Ponder until GTP commands are sent.
		if(	is_playing && b.prev_move[b.her] != PASS &&
			(tree.left_time > 25 || tree.byoyomi != 0))
		{
			double time_limit = 300.0;
			tree.SearchTree(b, time_limit, win_rate, false, true);
		}

		read_th.join();
		tree.stop_think = false;

		// GTP�R�}���h�̏���
		// Process GTP command.
		if (gtp_str == "" || gtp_str == "\n"){
			continue;
		}
		else if (FindStr(gtp_str, "name")) SendGTP("= AQ\n\n");
		else if (FindStr(gtp_str, "version")) SendGTP("= 2.0.2\n\n");
		else if (FindStr(gtp_str, "boardsize")) {
			// Board size setting. (only corresponding to 19 size)
			// "=boardsize 19", "=boardsize 13", ...
			SendGTP("= \n\n");
			cerr << "AQ corresponds only to the board with 19 size." << endl;
		}
		else if (FindStr(gtp_str, "list_commands"))
		{
			// �Ή����Ă���R�}���h�ꗗ�𑗂�
			// Send the corresponding command list.
			SendGTP("= boardsize\n");
			SendGTP("= list_commands\n");
			SendGTP("= clear_board\n");
			SendGTP("= genmove\n");
			SendGTP("= play\n");
			SendGTP("= quit\n");
			SendGTP("= time_left\n");
			SendGTP("= time_settings\n");
			SendGTP("= name\n");
			SendGTP("= version\n");
			SendGTP("= \n\n");
		}
		else if (FindStr(gtp_str, "clear_board"))
		{
			// �Ֆʂ�����������.
			// Initialize the board.
			b.Clear();
			tree.InitBoard();
			sgf.Clear();

			if(is_master) cluster.SendCommand("clear_board");

			if(save_log){
				++file_cnt;
				std::stringstream ss;
				ss << "./log/log_" << file_cnt << ".txt";
		                tree.log_file = new std::ofstream(ss.str(), std::ofstream::out);
			}

			is_playing = is_worker? true : false;
			play_mimic = cfg_mimic;

			SendGTP("= \n\n");
			cerr << "clear board." << endl;
		}
		else if (FindStr(gtp_str, "time_left"))
		{
			// �c�莞�Ԃ�ݒ肷��
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

			SendGTP("= \n\n");
		}
		else if (FindStr(gtp_str, "genmove")) {
			// ���̎���l���đ��M����.
			// Think and send the next move.
			// "=genmove b", "=genmove white", ...

			auto t1 = std::chrono::system_clock::now();
			cerr << "thinking...\n";

			pl = FindStr(gtp_str, "B", "b")? 1 : 0;
			is_playing = true;
			int next_move;
			tree.stop_think = false;
			bool think_full = true;


			if(play_mimic && b.my == 0){
				// a. �}�l��t���O�������Ă���Ƃ��}�l�������.
				//    Play mimic move if play_mimic is true.
				int v = b.prev_move[b.her];

				if(DistBetween(v, EBVCNT/2) < 8 || v == PASS){
					// �����t�߂ɑł��ꂽ�玩�͂ōl����.
					// Think by itself if the previous move is near the center.
					play_mimic = false;
				}
				else{
					next_move = tree.SearchTree(b, 1.0, win_rate, true, false);
					if(win_rate >= 0.65){
						// ������65%�𒴂���Ȃ�}�l����I������
						// Think by itself if the winning rate is over 65%.
						play_mimic = false;
					}
					else{
						// �}�l�邷�������߂�. Set mimic move.
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
				// b. �őP������߂�.
				//    Search for the best move.
				next_move = tree.SearchTree(b, 0.0, win_rate, true, false);

				if(	is_master && next_move != PASS 	&&
					b.prev_move[b.her] != PASS		&&
					(tree.left_time > 25 || tree.byoyomi != 0))
				{
					// ���c�̌��ʂ𔽉f����
					// Reflect the result of consultation.
					next_move = cluster.Consult(tree, tree.log_file);
				}
			}

			if(b.IsMimicGo()){ next_move = EBVCNT/2; }
			else if(win_rate < 0.1){
				// 1000��v���C�A�E�g���Ė{���ɕ����Ă��邩�m�F����
				// Roll out 1000 times to check if really losing.
				Board b_;
				int win_cnt = 0;
				for(int i=0;i<1000;++i){
					b_ = b;
					int result = PlayoutLGR(b_, tree.lgr, tree.komi);
					if(b.my == std::abs(result)) ++win_cnt;
				}
				if((double)win_cnt / 1000 < 0.25) next_move = PASS;
			}

			// c. �ǖʂ�i�߂�. Play the move.
			b.PlayLegal(next_move);
			tree.UpdateRootNode(b);

			// d. �����GTP�R�}���h�𑗂�. Send response of the next move.
			string str_nv = CoordinateString(next_move);
			if(next_move == PASS){
				if(!never_resign && win_rate < 0.1) SendGTP("= resign\n\n");
				else SendGTP("= pass\n\n");
			}
			else{
				SendGTP("= %s\n\n", str_nv.c_str());
			}


			// e. �q�v���Z�X�Ɏ�𑗐M����.
			//    Send play command to the remote processes.
			if(is_master){
				string cmd_str = (b.my == 0)? "play b " : "play w ";
				cmd_str += str_nv;
				cluster.SendCommand(cmd_str);
			}

			// f. ���O�t�@�C�����X�V����. Update logs.
			sgf.AddMove(next_move);
			if(!is_worker){

				PrintBoard(b, next_move);
				if(tree.log_file != NULL){
					PrintBoard(b, next_move, tree.log_file);
				}

				if(save_log){
					std::stringstream ss;
					ss << "./log/log_" << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}

				if(b.prev_move[b.her] == PASS && b.prev_move[b.my] == PASS){
					tree.PrintResult(b);
				}
			}

			// g. �c�莝�����Ԃ��X�V����. Update remaining time.
			if(need_time_controll){
				auto t2 = std::chrono::system_clock::now();
				double elapsed_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()/1000;
				tree.left_time = std::max(0.0, (double)tree.left_time - elapsed_time);
			}

		}
		else if (FindStr(gtp_str, "play "))
		{
			// �����M���A�Ֆʂɔ��f����. "play w D4" �̂悤�ɗ���
			// Receive the opponent's move and reflect on the board.
			// "=play w D4", "play b pass", ...

			int next_move;

			// a. ���������͂���.
			//    Analyze received string.
			if(FindStr(gtp_str, "pass", "Pass", "PASS")){
				next_move = PASS;
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

			// b. �u�΂�z�u����O�ɔ��̃p�X��}������
			//    Insert white pass before placing a stone
			if(	b.my == 0 && FindStr(gtp_str, "play b", "play B"))
			{
				b.PlayLegal(PASS);
				sgf.AddMove(PASS);
				--b.pass_cnt[0];

				if(tree.komi != 0){
					tree.komi = 0.0;
					cerr << "set Komi: 0\n";
				}
			}

			// c. �ǖʂ�i�߂�. Play the move.
			b.PlayLegal(next_move);
			tree.UpdateRootNode(b);

			// d. GTP�R�}���h�𑗐M. Send GTP response.
			SendGTP("= \n\n");

			// e. �}�l������Ă���Ƃ��A���肪�c�P�Ă������߂�
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

			// f. ���O�t�@�C�����X�V����. Update logs.
			sgf.AddMove(next_move);
			if(!is_worker){
				if(tree.log_file != NULL) PrintBoard(b, next_move, tree.log_file);
				PrintBoard(b, next_move);

				if(save_log){
					std::stringstream ss;
					ss << "./log/log_" << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}

				if(b.prev_move[b.her] == PASS && b.prev_move[b.my] == PASS){
					tree.PrintResult(b);
				}
			}
		}
		else if (FindStr(gtp_str, "undo", "Undo"))
		{
			// �ǖʂ��P��߂�. Undo the previous move.

			std::vector<int> move_history;
			for(auto v_hist: b.move_history) move_history.push_back(v_hist);
			if(!move_history.empty()) move_history.pop_back();

			// a. �ǖʂ�������. Initialize board.
			b.Clear();
			tree.Clear();
			sgf.Clear();
			if(is_master) cluster.SendCommand("clear_board");

			// b. �ǖʂ����O�܂Ői�߂�.
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

			// c. ���O�t�@�C�����X�V����. Update logs.
			if(!is_worker){
				if(save_log){
					std::stringstream ss;
					ss << "./log/log_" << file_cnt << ".sgf";
					sgf.ExportData(ss.str());
				}
			}

			// d. GTP�R�}���h�𑗐M. Send GTP response.
			SendGTP("= \n\n");
		}
		else if(FindStr(gtp_str, "final_score")) {
			// 1000��v���C�A�E�g���s���A�ŏI�I�ȃX�R�A�����߂�
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
			// �őP��𑗐M����
			// Send the best move.

			Node *pn = &tree.node[tree.root_node_idx];
			std::vector<Child*> rc;
			SortChildren(pn, rc);
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
		else if(FindStr(gtp_str, "quit")){
			if(is_master) cluster.SendCommand("quit");
			if(!is_worker) tree.PrintResult(b);
			break;
		}
		else{
			SendGTP("= \n\n");
			cerr << "unknown command.\n";
		}
	}
	return 0;

}
