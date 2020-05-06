/*
 * AQ, a Go playing engine.
 * Copyright (C) 2017-2020 Yu Yamaguchi
 * except where otherwise indicated.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "./option.h"
#include "./pattern.h"

Option::OptionsMap Options;

std::string JoinPath(const std::string s1, const std::string s2,
                     const std::string s3) {
  std::string s = s1;
#ifdef _WIN32
  std::string split_str = "\\";
#else
  std::string split_str = "/";
#endif
  size_t n = split_str.size();
  if (s.size() >= n && s.substr(s.size() - n) != split_str) s += split_str;
  s += s2;
  if (s3 != "") {
    if (s.substr(s.size() - n) != split_str) s += split_str;
    s += s3;
  }

  return s;
}

namespace {
/**
 * Initialize an OptionMap to the default values.
 */
void InitOptions(Option::OptionsMap* o) {
  (*o)["num_threads"] << Option(16, 1, 512);
  (*o)["num_gpus"] << Option(1, 1, 32);

#if BOARD_SIZE == 19
  (*o)["komi"] << Option(7.5);
#else
  (*o)["komi"] << Option(7.0);
#endif
  (*o)["rule"] << Option(0, 0, 2);
  (*o)["repetition_rule"] << Option(0, 0, 2);
  (*o)["resign_value"] << Option(0.1);
  (*o)["use_ponder"] << Option(true);
  (*o)["allocate_gpu"] << Option(false);

  (*o)["main_time"] << Option(0.0);
  (*o)["byoyomi"] << Option(3.0);
  (*o)["byoyomi_margin"] << Option(0.0);
  (*o)["num_extensions"] << Option(0, 0, 100);
  (*o)["emergency_time"] << Option(15.0);
  (*o)["need_time_control"] << Option(true);

  (*o)["working_dir"] << Option("");
  (*o)["sgf_dir"] << Option("");
  (*o)["result_dir"] << Option("");
  (*o)["stop_flag_dir"] << Option("");
  (*o)["use_rating_model"] << Option(false);
  (*o)["model_path"] << Option("default");
  (*o)["validate_model_path"] << Option("default");
  (*o)["node_size"] << Option(65536, 4096, 67108864);

  (*o)["save_log"] << Option(true);
  (*o)["resume_file_name"] << Option("");
  (*o)["send_list"] << Option(false);

  (*o)["lizzie"] << Option(false);

  // Seach parameters.
  (*o)["batch_size"] << Option(8, 1, 256);
  (*o)["lambda_init"] << Option(0.95);
  (*o)["lambda_delta"] << Option(0.2);

#if BOARD_SIZE == 19
  (*o)["lambda_move_start"] << Option(240, 0, 400);
  (*o)["lambda_move_end"] << Option(360, 1, 401);
#elif BOARD_SIZE == 13
  (*o)["lambda_move_start"] << Option(120, 0, 400);
  (*o)["lambda_move_end"] << Option(180, 1, 401);
#else   // BOARD_SIZE == 9
  (*o)["lambda_move_start"] << Option(60, 0, 400);
  (*o)["lambda_move_end"] << Option(90, 1, 401);
#endif  // BOARD_SIZE == 19

  (*o)["cp_init"] << Option(0.75);
  (*o)["cp_base"] << Option(20000.0);
  (*o)["use_dirichlet_noise"] << Option(false);
  (*o)["dirichlet_noise"] << Option(0.03);
  (*o)["search_limit"] << Option(-1, -1, 100000);
  (*o)["virtual_loss"] << Option(1, 0, 64);
  (*o)["ladder_reduction"] << Option(0.1);

  (*o)["num_games"] << Option(1, 1, 1000000);
  (*o)["use_full_features"] << Option(true);
  (*o)["value_from_black"] << Option(false);

#if defined(LEARN)
  (*o)["opening_model_path"] << Option("");
  (*o)["run_id"] << Option(-1, -1, 1000000);
  (*o)["param_id"] << Option(-1, -1, 1000000);
  (*o)["update_each"] << Option(false);
  (*o)["model_interval"] << Option(10, 1, 10000);
  (*o)["num_agents"] << Option(-1, 2, 40);

  (*o)["db_host"] << Option("127.0.0.1");
  (*o)["db_user"] << Option("user");
  (*o)["db_pwd"] << Option("pwd");
  (*o)["db_name"] << Option("learn");
  (*o)["db_port"] << Option(3306, 0, 65535);
#endif
}
}  // namespace

/**
 * Parse config.txt and command line arguments and reflect them in Options.
 */
std::string ReadConfiguration(int argc, char** argv) {
  InitOptions(&Options);

  // 1. Gets working directory path.
  char buf[1024] = {};
#ifdef _WIN32
  // GetModuleFileName needs a char* argument in MinGW-x64.
  GetModuleFileName(NULL, buf, sizeof(buf));
  std::string split_str = "\\";
#else
  auto sz = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  std::string split_str = "/";
#endif
  std::string path_(buf);
  // Deletes file name.
  auto pos_filename = path_.rfind(split_str);
  if (pos_filename != std::string::npos) {
    path_ = path_.substr(0, pos_filename + 1);
    // Uses current directory if there is no configure file.
    std::ifstream ifs(path_ + "config.txt");
    if (ifs.is_open()) Options["working_dir"] = path_;
  }

  // 2. Import prob table.
  std::string prob_dir = JoinPath(Options["working_dir"], "prob");
  Pattern::Init(prob_dir);

  std::string config_path = JoinPath(Options["working_dir"], "config.txt");

  // 3. Sets configuration file path.
  for (int i = 0; i < argc; ++i) {
    std::string arg_i = argv[i];
    if (arg_i.find("--config=") != std::string::npos) {
      config_path = arg_i.substr(9);
      std::cerr << "Set configuration file: " << config_path << std::endl;
    }
  }

  // 4. Sets alies for old options.
  std::unordered_map<std::string, std::string> alies_options;
  {
    alies_options["gpu_cnt"] = "num_gpus";
    alies_options["thread_cnt"] = "num_threads";
    alies_options["extension_cnt"] = "num_extensions";
    alies_options["batch_cnt"] = "batch_size";
    alies_options["game_cnt"] = "num_games";
    alies_options["vloss_cnt"] = "virtual_loss";
    alies_options["agent_cnt"] = "num_agents";
  }

  std::unordered_set<std::string> executable_modes{
      "--benchmark",   "--test",  "--self",
      "--policy_self", "--learn", "--rating"};

  auto trim_str = [](const std::string& str,
                     const char* trim_chars = " \t\v\r\n") {
    std::string trimmed_str;
    auto left = str.find_first_not_of(trim_chars);

    if (left != std::string::npos) {
      auto right = str.find_last_not_of(trim_chars);
      trimmed_str = str.substr(left, right - left + 1);
    }
    return trimmed_str;
  };

  auto flag_str = [](const std::string& str) {
    return (str == "on" || str == "On" || str == "ON") ? "true" : "false";
  };

  // 5. Open the configuration file.
  std::ifstream ifs(config_path);
  std::string str;

  // Read line by line.
  int num_lines = 0;
  while (ifs && getline(ifs, str)) {
    ++num_lines;
    // Exclude comments.
    auto cmt_pos = str.find("#");
    if (cmt_pos != std::string::npos) {
      str = str.substr(0, cmt_pos);
    }
    str = trim_str(str, " \t\v\r\n");
    if (str.length() == 0) continue;

    // Finds position after '='.
    auto eql_pos = str.find("=");
    if (eql_pos == std::string::npos) {
      std::cerr << "Failed to parse config:" << config_path << ":" << num_lines
                << " " << str << ". '=' not found.\n";
      continue;
    }

    std::string key = trim_str(str.substr(0, eql_pos));
    std::string val = trim_str(str.substr(eql_pos + 1));

    if (key.find("--") == std::string::npos) {
      std::cerr << "Set '--' before option: [" << key << "]" << std::endl;
    } else {
      key = key.substr(2);  // Removes '--'
      if (alies_options.count(key) > 0) {
        // Convert from options of old version.
        key = alies_options[key];
      }

      if (Options.find(key) == Options.end()) {
        std::cerr << "Unknown option: [--" << key << "]" << std::endl;
        exit(1);
      }

      if (val == "on" || val == "off")
        Options[key] = flag_str(val);
      else
        Options[key] = val;
    }
  }
  ifs.close();

  // 6. Reads command line options.
  // Overwrites options from configure file.
  std::string mode = "";
  bool set_batch_size = Options["batch_size"].get_int() != 8;
  for (int i = 0; i < argc; ++i) {
    std::string arg_i = argv[i];
    if (executable_modes.count(arg_i) > 0) {
      mode = arg_i;
    } else if (arg_i == "--lizzie") {
      Options["lizzie"] = "true";
    } else if (arg_i.find("--config=") != std::string::npos) {
      continue;
    } else if (arg_i.find("--") != std::string::npos) {
      std::string str = arg_i.substr(2);
      auto eql_pos = str.find("=");
      if (eql_pos == std::string::npos) {
        std::cerr << "Failed to parse command line option: [--" << str
                  << "]. '=' not found.\n";
        exit(1);
      }
      std::string key = str.substr(0, eql_pos);
      std::string val = str.substr(eql_pos + 1);
      if (alies_options.count(key) > 0) {
        // Convert from options of old version.
        key = alies_options[key];
      }

      if (Options.find(key) == Options.end()) {
        std::cerr << "Unknown option: [--" << key << "]" << std::endl;
        exit(1);
      }

      if (val == "on" || val == "off")
        Options[key] = flag_str(val);
      else
        Options[key] = val;

      if (key == "batch_size") set_batch_size = true;
    }
  }
  // Set 5 batch size when using search_limit option.
  if (!set_batch_size && Options["search_limit"].get_int() > 0)
    Options["batch_size"] = 5;

  std::cerr << "Configuration is loaded.\n";

  return mode;
}
