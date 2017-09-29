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

std::string trim(const std::string& string, const char* trimCharacterList = " \t\v\r\n") {
	std::string result;

	auto left = string.find_first_not_of(trimCharacterList);

	if (left != std::string::npos)
	{
		auto right = string.find_last_not_of(trimCharacterList);

		result = string.substr(left, right - left + 1);
	}

	return result;
}

int parseBool(const std::string& str) {
	return str == "true" || str == "True" || str == "TRUE";
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
			line_cnt++;
			auto cmt = str.find("#");
			if (cmt != std::string::npos) {
				str = str.substr(0, cmt);
			}
			str = trim(str, " \t\v\r\n-");
			if (str.length() == 0) continue;

			auto eql = str.find("=") + 1;
			if (eql == std::string::npos) {
				std::cerr << "Failed to parse config:" << config_path << ":" << line_cnt << " " << str << ". '=' not found" << std::endl;
				continue;
			}
			auto key = trim(str.substr(0, eql - 1));
			auto val = trim(str.substr(eql));
			if (key == "gpu count")			cfg_gpu_cnt = stoi(val);
			else if (key == "thread count")		cfg_thread_cnt = stoi(val);
			else if (key == "main time[sec]")	cfg_main_time = stod(val);
			else if (key == "byoyomi[sec]")		cfg_byoyomi = stod(val);
			else if (key == "time controll")	need_time_controll = parseBool(val);
			else if (key == "japanese rule")	japanese_rule = parseBool(val);
			else if (key == "komi")			cfg_komi = stod(val);
			else if (key == "symmetrical index")	cfg_sym_idx = stoi(val);
			else if (key == "mimic go")		cfg_mimic = parseBool(val);
			else if (key == "never resign")		never_resign = parseBool(val);
			else if (key == "self match")		self_match = parseBool(val);
			else if (key == "save log")		save_log = parseBool(val);
			else if (key == "master")		is_master = parseBool(val);
			else if (key == "worker")		is_worker = parseBool(val);
			else if (key == "pb path")		pb_dir = val;
			else if (key == "resume sgf path")	resume_sgf_path = val;
			else if (key == "worker count")		cfg_worker_cnt = stoi(val);
			else {
				std::cerr << "Unknown key: [" << key << "]" << std::endl;
			}
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
