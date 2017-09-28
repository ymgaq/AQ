#include "main.h"


int main(int argc, char **argv) {

	ReadConfiguration(argc, argv);
	std::cerr << "configuration loaded.\n";

	if(self_match) 	SelfMatch();
	else 			CallGTP();

	//DoSomething();

	std::cerr << "finished.\n";
	return 0;

}


void ReadConfiguration(int argc, char **argv){

	ImportProbDist();
	ImportProbPtn3x3();

	for(int i=0;i<argc;++i){

		std::string str_arg = argv[i];
		std::string config_path = "aq_config.txt";
		if(str_arg.find("--config=") != std::string::npos){
			config_path = str_arg.substr(9);
			std::cerr << "configuration file path = " << config_path << std::endl;
		}

		// Open the configuration file.
		std::ifstream ifs(config_path);
		std::string str;

		// Read line by line.
		int line_cnt = 0;
		while (ifs && getline(ifs, str)) {

			if(str.find("=") != std::string::npos){
				auto head = str.find("=") + 1;
				str = str.substr(head);
				if(str.substr(0,1) == " ") str = str.substr(1);
			}
			else continue;

			switch(line_cnt){
				case 0:  cfg_gpu_cnt 	= stoi(str); break;
				case 1:  cfg_thread_cnt 	= stoi(str); break;
				case 2:  cfg_main_time 	= stod(str); break;
				case 3:  cfg_byoyomi 	= stod(str); break;
				case 4:  need_time_controll = (str == "true" || str == "True"); break;
				case 5:  japanese_rule 	= (str == "true" || str == "True"); break;
				case 6:  cfg_komi 		= stod(str); break;
				case 7:  cfg_sym_idx 	= stoi(str); break;
				case 8:  cfg_mimic 		= (str == "true" || str == "True"); break;
				case 9:  never_resign 	= (str == "true" || str == "True"); break;
				case 10: self_match 	= (str == "true" || str == "True"); break;
				case 11: save_log 		= (str == "true" || str == "True"); break;
				case 12: is_master 		= (str == "true" || str == "True"); break;
				case 13: is_worker 		= (str == "true" || str == "True"); break;
				case 14: pb_dir 		= str; break;
				case 15: resume_sgf_path 	= str; break;
				case 16: cfg_worker_cnt 	= stoi(str); break;
				default: break;
			}
			++line_cnt;
		}
		ifs.close();
	}

}


void SelfMatch() {

	Tree tree;
	Board b;
	int next_move = PASS;
	int prev_move = VNULL;
	double win_rate = 0.5;
	std::string log_path = "./log/log_self.txt";
	tree.log_file = new std::ofstream(log_path, std::ofstream::out);

	for(int i=0;i<1;++i){
		b.Clear();
		prev_move = VNULL;
		win_rate = 0.5;

		while (b.move_cnt<720) {
			next_move = tree.SearchTree(b, 0.0, win_rate, true, false);
			b.PlayLegal(next_move);
			PrintBoard(b, next_move);
			PrintBoard(b, next_move, tree.log_file);
			if (next_move==PASS && prev_move==PASS) break;
			prev_move = next_move;
		}

		tree.PrintResult(b);
	}

}
