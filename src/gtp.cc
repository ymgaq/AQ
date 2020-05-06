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

#include "./gtp.h"

const char kVersion[] = "4.0.0";

const std::vector<std::string> kListCommands = {"board_size",
                                                "list_commands",
                                                "clear_board",
                                                "genmove",
                                                "play",
                                                "quit",
                                                "time_left",
                                                "time_settings",
                                                "name",
                                                "protocol_version",
                                                "version",
                                                "komi",
                                                "final_score",
                                                "kgs-time_settings",
                                                "kgs-game_over",
                                                "place_free_handicap",
                                                "set_free_handicap",
                                                "gogui-play_sequence",
                                                "lz-analyze"};

std::string GTPConnector::OnClearBoardCommand() {
  StopLizzieAnalysis();
  b_.Init();      // Initializes the board.
  AllocateGPU();  // Allocates memory.
  tree_.InitRoot();
  tree_.UpdateRoot(b_);
  sgf_.Init();
  c_engine_ = kEmpty;
  go_ponder_ = false;

  // Resumes from SGF file.
  if ((std::string)Options["resume_file_name"] != "") {
    sgf_.Read((std::string)Options["working_dir"] +
              (std::string)Options["resume_file_name"]);
    sgf_.ReconstructBoard(&b_, sgf_.game_ply());
    tree_.UpdateRoot(b_);

    Options["resume_file_name"] = "";
  }

  if (save_log_) {
    std::ifstream ifs(sgf_path_);
    if (ifs.is_open()) {
      time_t t = time(NULL);
      char date[64];
      strftime(date, sizeof(date), "%Y%m%d_%H%M%S", localtime(&t));
      std::string date_str = date;

      log_path_ = JoinPath(Options["working_dir"], "log", date_str + ".txt");
      sgf_path_ = JoinPath(Options["working_dir"], "log", date_str + ".sgf");

      if (tree_.log_file()) tree_.log_file()->close();
      tree_.SetLogFile(log_path_);
      ifs.close();
    }
  }

  fprintf(stderr, "cleared board.\n");
  return "";
}

/**
 * Searches and send the next move.
 *  e.g. "=genmove b", "=genmove white", ...
 */
std::string GTPConnector::OnGenmoveCommand() {
  auto t0 = std::chrono::system_clock::now();
  StopLizzieAnalysis();
  std::string response("");

  // a. Allocates memory.
  AllocateGPU();

  // b. Returns error if side to move is not consistent.
  Color c_arg = FindString(args_[0], "B", "b") ? kBlack : kWhite;
  if (c_arg != b_.side_to_move()) {
    success_handle_ = false;
    response = "genmove command passed wrong color.";
    fprintf(stderr, "? %s\n", response.c_str());
    if (tree_.log_file()) *tree_.log_file() << "? " << response << std::endl;

    return response;
  }

  c_engine_ = b_.side_to_move();
  go_ponder_ = true;
  tree_.PrepareToThink();

  // c. Searches for the best move.
  double winning_rate = 0.5;
  Vertex next_move = tree_.Search(b_, 0.0, &winning_rate, true, false);

  bool resign = false;
  if (next_move != kPass &&
      winning_rate < Options["resign_value"].get_double()) {
    resign = true;
    next_move = kPass;
  }

  // d. Plays the move.
  b_.MakeMove<kOneWay>(next_move);
  tree_.UpdateRoot(b_);

  // e. Updates logs.
  sgf_.Add(next_move);
  if (save_log_) sgf_.Write(sgf_path_);
  tree_.PrintBoardLog(b_);
  if (b_.double_pass()) PrintFinalResult(b_);

  // f. Sends response of the next move.
  if (resign)
    response = "resign";
  else if (next_move == kPass)
    response = "pass";
  else
    response = tree_.v2str(next_move);

  // g. Updates remaining time.
  if (Options["need_time_control"]) {
    double elapsed_time = tree_.ElapsedTime(t0);
    tree_.set_left_time(std::max(0.0, tree_.left_time() - elapsed_time));
  }

  return response;
}

/**
 * Receives the opponent's move and reflect on the board.
 *  e.g. "=play w D4", "play b pass", ...
 */
std::string GTPConnector::OnPlayCommand() {
  StopLizzieAnalysis();
  std::string response("");

  go_ponder_ = false;  // Because 'genmove' command comes soon.

  // Returns error if side to move is not consistent.
  Color c_arg = FindString(args_[0], "B", "b") ? kBlack : kWhite;
  if (c_arg != b_.side_to_move()) {
    success_handle_ = false;
    response = "play command passed wrong color.";
    fprintf(stderr, "? %s\n", response.c_str());
    if (tree_.log_file()) *tree_.log_file() << "? " << response << std::endl;

    return response;
  }

  // a. Analyzes received string.
  Vertex next_move;
  if (FindString(args_[1], "pass", "Pass", "PASS")) {
    next_move = kPass;
  } else if (FindString(args_[1], "resign", "Resign", "RESIGN")) {
    next_move = kPass;
  } else {
    std::string str_x = args_[1].substr(0, 1);
    std::string str_y = args_[1].substr(1);
    std::string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

    int x = static_cast<int>(x_list.find(str_x)) % 19 + 1;
    int y = stoi(str_y);
    next_move = xy2v(x, y);
  }

  if (save_log_) {
    if (b_.game_ply() == 0) tree_.UpdateRoot(b_);
    std::stringstream ss;
    tree_.PrintCandidates(tree_.root_node(), next_move, ss, true);
    if (tree_.log_file()) *(tree_.log_file()) << ss.str();
    std::cerr << ss.str();
  }

  // c. Plays the move.
  b_.MakeMove<kOneWay>(next_move);
  tree_.UpdateRoot(b_);

  // d. Updates logs.
  sgf_.Add(next_move);
  if (save_log_) sgf_.Write(sgf_path_);
  tree_.PrintBoardLog(b_);
  if (b_.double_pass()) PrintFinalResult(b_);

  return response;
}

/**
 * Undoes the previous move.
 */
std::string GTPConnector::OnUndoCommand() {
  StopLizzieAnalysis();
  std::vector<Vertex> move_history = b_.move_history();
  if (!move_history.empty()) move_history.pop_back();
  double left_time = tree_.left_time();

  int num_passes[2] = {b_.num_passes(kWhite), b_.num_passes(kBlack)};
  if (b_.move_before() == kPass) --num_passes[~b_.side_to_move()];

  // a. Initializes board.
  b_.Init();
  tree_.InitRoot();
  sgf_.Init();

  // b. Advances the board to the previous state.
  for (auto v_hist : move_history) {
    b_.MakeMove<kOneWay>(v_hist);
    sgf_.Add(v_hist);
  }
  tree_.UpdateRoot(b_);
  tree_.set_left_time(left_time);

  b_.set_num_passes(kWhite, num_passes[kWhite]);
  b_.set_num_passes(kBlack, num_passes[kBlack]);

  // c. Updates logs.
  if (save_log_) sgf_.Write(sgf_path_);
  tree_.PrintBoardLog(b_);

  return "";
}

/**
 * Returns Lizzie information.
 */
std::string GTPConnector::OnLzAnalyzeCommand() {
  lizzie_interval_ = (args_.size() >= 1 ? stoi(args_[0]) * 10 : 100);  // millisec
  if (!tree_.has_eval_worker()) {
    AllocateGPU();  // Allocates memory.
    b_.Init();
    tree_.InitRoot();
    tree_.UpdateRoot(b_);
    sgf_.Init();
    c_engine_ = kEmpty;
  }
  go_ponder_ = true;
  tree_.PrepareToThink();

  return "";
}

/**
 * Sets main and byoyomi time.
 *  e.g. "=kgs-time_settings byoyomi 30 60 3", ...
 */
std::string GTPConnector::OnKgsTimeSettingsCommand() {
  if (FindString(args_[0], "byoyomi") && args_.size() >= 3) {
    Options["main_time"] = args_[1];
    tree_.set_main_time(stod(args_[1]));
    tree_.set_left_time(tree_.main_time());
    Options["byoyomi"] = args_[2];
    tree_.set_byoyomi(stod(args_[2]));
  } else {
    Options["main_time"] = args_[1];
    tree_.set_main_time(stod(args_[1]));
    tree_.set_left_time(tree_.main_time());
  }

  std::fprintf(stderr, "main time=%.1f[sec], byoyomi=%.1f[sec], extension=%d\n",
               Options["main_time"].get_double(),
               Options["byoyomi"].get_double(),
               Options["num_extensions"].get_int());

  return "";
}

/**
 * Sets main and byoyomi time.
 *  e.g. "=time_settings 30 60 3", ...
 */
std::string GTPConnector::OnTimeSettingsCommand() {
  Options["main_time"] = args_[0];
  tree_.set_main_time(stod(args_[0]));
  tree_.set_left_time(tree_.main_time());
  Options["byoyomi"] = args_[1];
  tree_.set_byoyomi(stod(args_[1]));
  // if (args_.size() >= 3) {
  //   Options["num_extensions"] =
  //       std::to_string(std::max(0, stoi(args_[2]) - 1));
  //   tree_.set_num_extensions(std::max(0, stoi(args_[2]) - 1));
  // }

  std::fprintf(stderr, "main time=%.1f[sec], byoyomi=%.1f[sec], extension=%d\n",
               Options["main_time"].get_double(),
               Options["byoyomi"].get_double(),
               Options["num_extensions"].get_int());

  return "";
}

/**
 * Places handicap stones.
 *  e.g. "=set_free_handicap D4 ..."
 */
std::string GTPConnector::OnSetFreeHandicapCommand() {
  if (args_.size() >= 1) {
    int i_max = args_.size();
    for (int i = 0; i < i_max; ++i) {
      std::string str_x = args_[i].substr(0, 1);
      std::string str_y = args_[i].substr(1);

      std::string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

      int x = static_cast<int>(x_list.find(str_x)) % 19 + 1;
      int y = stoi(str_y);

      Vertex next_move = xy2v(x, y);
      b_.MakeMove<kOneWay>(next_move);
      sgf_.Add(next_move);

      // Add a white pass except at the end to adjust the turn.
      // In the case of handicapped games, start with white.
      if (i != i_max - 1) {
        b_.MakeMove<kOneWay>(kPass);
        sgf_.Add(kPass);
        b_.decrement_passes(kWhite);
      }
    }
  }

  std::fprintf(stderr, "set free handicap.\n");
  return "";
}

/**
 * Places fixed handicap stones.
 *  e.g. "=fixed_handicap 2"
 */
std::string GTPConnector::OnFixedHandicapCommand() {
  if (args_.size() >= 1) {
    int x_[9] = {4, 16, 4, 16, 4, 16, 10, 10, 10};
    int y_[9] = {4, 16, 16, 4, 10, 10, 4, 16, 10};
    int stones[8][9] = {{0, 1},
                        {0, 1, 2},
                        {0, 1, 2, 3},
                        {0, 1, 2, 3, 8},
                        {0, 1, 2, 3, 4, 5},
                        {0, 1, 2, 3, 4, 5, 8},
                        {0, 1, 2, 3, 4, 5, 6, 7},
                        {0, 1, 2, 3, 4, 5, 6, 7, 8}};
    int num_handicaps = stoi(args_[0]);
    for (int i = 0; i < num_handicaps; ++i) {
      int stone_idx = stones[num_handicaps - 2][i];
      Vertex v = xy2v(x_[stone_idx], y_[stone_idx]);
      b_.MakeMove<kOneWay>(v);
      sgf_.Add(v);

      // Add a white pass except at the end to adjust the turn.
      // In the case of handicapped games, start with white.
      if (i != num_handicaps - 1) {
        b_.MakeMove<kOneWay>(kPass);
        sgf_.Add(kPass);
        b_.decrement_passes(kWhite);
      }
    }
  }

  std::fprintf(stderr, "placed handicap stones.\n");
  return "";
}

/**
 * Receives all moves from start and reconstructs board.
 *  e.g. "=gogui-play_sequence B R16 W D16 B Q3 W D3 ..."
 */
std::string GTPConnector::OnGoguiPlaySequenceCommand() {
  int i_max = args_.size();
  for (int i = 1; i < i_max; i = i + 2) {
    Color c = (FindString(args_[i - 1], "B", "b")) ? kBlack : kWhite;
    Vertex next_move = kPass;
    if (b_.side_to_move() != c) {
      b_.MakeMove<kOneWay>(kPass);
      sgf_.Add(kPass);
      b_.decrement_passes(c);
    }
    if (!FindString(args_[i], "PASS", "Pass", "pass")) {
      std::string str_x = args_[i].substr(0, 1);
      std::string str_y = args_[i].substr(1);

      std::string x_list = "ABCDEFGHJKLMNOPQRSTabcdefghjklmnopqrst";

      int x = static_cast<int>(x_list.find(str_x)) % 19 + 1;
      int y = stoi(str_y);

      next_move = xy2v(x, y);
    }

    // Plays the move.
    b_.MakeMove<kOneWay>(next_move);
    // Updates logs.
    sgf_.Add(next_move);
    tree_.PrintBoardLog(b_);
  }

  tree_.UpdateRoot(b_);
  if (save_log_) sgf_.Write(sgf_path_);

  std::fprintf(stderr, "sequence loaded.\n");
  return "";
}
