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

#ifndef GTP_H_
#define GTP_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "./board.h"
#include "./search.h"
#include "./sgf.h"

extern const char kVersion[];

/**
 * List of supported commands.
 * (Just what match servers or GUI requires)
 */
extern const std::vector<std::string> kListCommands;

/**
 * @class GTPConnector
 * Manages GTP communication and loops until the quit command is sent.
 *
 * See the following link for detail of GTP (Go Text Protocol) communication.
 * https://www.lysator.liu.se/~gunnar/gtp/gtp2-spec-draft2/gtp2-spec.html
 *
 * It is implemented so that it receives a query from the server and returns
 * information such as a move. Since standard input/output is used, stdout
 * should not be used except for GTP. (Error in GTP communication.)
 */
class GTPConnector {
 public:
  // Constructor.
  GTPConnector()
      : c_engine_(kEmpty),
        go_ponder_(false),
        success_handle_(true),
        running_analysis_(false) {
    // Log settings.
    save_log_ = Options["save_log"].get_bool();
    time_t t = time(NULL);
    char date[64];
    std::strftime(date, sizeof(date), "%Y%m%d_%H%M%S", localtime(&t));
    std::string date_str = date;
    log_path_ = JoinPath(Options["working_dir"], "log", date_str + ".txt");
    sgf_path_ = JoinPath(Options["working_dir"], "log", date_str + ".sgf");
    // Sets file path of log.
    if (save_log_) tree_.SetLogFile(log_path_);

    // Sends command list for a kind of matching server.
    if (Options["send_list"]) {
      std::string response("");
      for (auto cmd : kListCommands) response += cmd + "\n";
      response += "= ";
      SendGTPCommand("= %s\n\n", response);
    }

    // Allocates gpu in advance.
    if (Options["allocate_gpu"]) AllocateGPU();
  }

  void Start() {
    // Starts communication with the GTP protocol.
    bool running = true;
    while (running) {
      std::string command("");
      bool start_pondering = Options["use_ponder"].get_bool() && go_ponder_ &&
                             b_.move_before() != kPass &&
                             (tree_.left_time() > 10.0 || tree_.byoyomi() != 0);
      if (start_pondering) AllocateGPU();

      // Thread that monitors GTP commands during pondering.
      std::thread read_th([this, &command, start_pondering]() {
        while (command == "") {
          ReceiveGTPCommand(&command);
          if (command != "" && start_pondering) {
            tree_.StopToThink();
            break;
          }
          // Interval of checking command strings.
          std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 1 msec
        }
        // Waits until SearchTree class stops thinking.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 10 msec
      });

      // Goes pondering until the next command is received.
      if (start_pondering) {
        double winning_rate = 0.5;
        double time_limit = 100.0;
        if (Options["lizzie"])
          time_limit = 86400.0;
        else if (tree_.byoyomi() > 0 && tree_.main_time() > 0 &&
                 tree_.left_time() < tree_.byoyomi() * 2)
          time_limit = tree_.byoyomi() * 2;

        tree_.Search(b_, time_limit, &winning_rate, false, true);
      }

      read_th.join();
      tree_.PrepareToThink();

      // Processes GTP command.
      if (command == "" || command == "\n") continue;
      // Executes each command.
      // Stops when 'quit' command is send.
      running = ExecuteCommand(command);
    }
  }

  bool ExecuteCommand(std::string command) {
    // 1. Print command into the log file.
    if (tree_.log_file()) *tree_.log_file() << command << std::endl;

    // 2. Parses command.
    int command_id = -1;
    std::string type = ParseCommand(command, &command_id, &args_);
    std::string response = "";
    success_handle_ = true;

    if (FindString(command, "protocol_version")) {
      response = "2";
    } else if (type == "name") {
      StopLizzieAnalysis();
      response = "AQ";
    } else if (type == "version") {
      response = Options["lizzie"] ? "0.16" : std::string(kVersion);
    } else if (type == "known_command") {
      if (args_.size() >= 1 &&
          std::find(kListCommands.begin(), kListCommands.end(), args_[0]) !=
              kListCommands.end())
        response = "true";
      else
        response = "false";
    } else if (type == "list_commands") {
      for (auto cmd : kListCommands) response += cmd + "\n";
      response += "= ";
    } else if (type == "boardsize") {
      // Board size setting. (only corresponding to 19 size)
      //  e.g. "=boardsize 19", "=boardsize 13", ...
      if (stoi(args_[0]) != kBSize) {
        success_handle_ = false;
        response = "This build is allowed to play in only " +
                   std::to_string(int{kBSize}) + " board.";
        fprintf(stderr, "? %s\n", response.c_str());
      }
    } else if (type == "clear_board") {
      response = OnClearBoardCommand();
    } else if (type == "komi") {
      double komi = stod(args_[0]);
      Options["komi"] = komi;
      tree_.set_komi(komi);
      fprintf(stderr, "set komi=%.1f.\n", komi);
    } else if (type == "time_left") {
      // Sets remaining time.
      //  e.g. "=time_left B 944", "=time_left white 300", ...
      Color c = FindString(args_[0], "B", "b")
                    ? kBlack
                    : FindString(args_[0], "W", "w") ? kWhite : kEmpty;
      double left_time = stod(args_[1]);
      if (c_engine_ == kEmpty || c_engine_ == c) tree_.set_left_time(left_time);
      Options["need_time_control"] = "false";
    } else if (type == "genmove") {
      response = OnGenmoveCommand();
    } else if (type == "play") {
      response = OnPlayCommand();
    } else if (type == "undo") {
      response = OnUndoCommand();
    } else if (type == "final_score") {
      response = PrintFinalResult(b_);
    } else if (type == "lz-analyze") {
      response = OnLzAnalyzeCommand();
    } else if (type == "kgs-time_settings") {
      response = OnKgsTimeSettingsCommand();
    } else if (type == "time_settings") {
      response = OnTimeSettingsCommand();
    } else if (type == "set_free_handicap") {
      response = OnSetFreeHandicapCommand();
    } else if (type == "fixed_handicap" || type == "place_free_handicap") {
      response = OnFixedHandicapCommand();
    } else if (type == "gogui-play_sequence") {
      response = OnGoguiPlaySequenceCommand();
    } else if (type == "kgs-game_over") {
      go_ponder_ = false;
    } else if (type == "quit") {
      StopLizzieAnalysis();
      PrintFinalResult(b_);
    } else {
      success_handle_ = false;
      response = "unknown command.";
      fprintf(stderr, "? %s\n", response.c_str());
    }

    std::string head_str = success_handle_ ? "=" : "?";
    if (command_id >= 0) head_str += std::to_string(command_id);
    SendGTPCommand("%s %s\n\n", head_str.c_str(), response.c_str());

    return (type != "quit");
  }

  /**
   * Checks if an arbitrary string is included.
   */
  bool FindString(std::string str, std::string s1, std::string s2 = "",
                  std::string s3 = "") {
    bool found = false;
    found |= (s1 != "" && str.find(s1) != std::string::npos);
    found |= (s2 != "" && str.find(s2) != std::string::npos);
    found |= (s3 != "" && str.find(s3) != std::string::npos);
    return found;
  }

  /**
   * Parses GTP command to type and arguments.
   */
  std::string ParseCommand(const std::string& command, int* command_id,
                           std::vector<std::string>* args) {
    *command_id = -1;
    args->clear();
    std::string type = "";

    std::istringstream iss(command);
    std::string s;

    while (iss >> s) {
      if (type == "") {
        if (s.substr(0, 1) == "=") s = s.substr(1);
        if (s == "") continue;

        if (std::all_of(s.cbegin(), s.cend(), isdigit)) {
          *command_id = std::stoi(s);
        } else {
          type = s;
        }
      } else {
        args->push_back(s);
      }
    }

    return type;
  }

  /**
   * Returns a GTP response using standard output.
   */
  void SendGTPCommand(const char* output_str, ...) {
    va_list args;
    va_start(args, output_str);
    vfprintf(stdout, output_str, args);
    va_end(args);
  }

  /**
   * Reads a line of the input GTP command.
   */
  void ReceiveGTPCommand(std::string* input_str) {
    std::getline(std::cin, *input_str);
  }

  /**
   * Allocates GPUs.
   * Allocating GPU memory in the constructor of the SearchTree class may take
   * several tens of seconds, so it is allocated at clear_board to avoid
   * timeouts in game servers and GUI.
   */
  void AllocateGPU() {
    if (!tree_.has_eval_worker()) {
      std::cerr << "allocating memory...\n";
      // Waits 5s when rating measurement.
      if (!save_log_ && !Options["use_ponder"])
        std::this_thread::sleep_for(std::chrono::seconds(5));  // 5s
      tree_.SetGPUAndMemory();
    }
  }

  /**
   * Return the final score.
   * If the log file is specified, the dead stone information is output.
   */
  std::string PrintFinalResult(const Board& b_) {
    std::vector<std::ostream*> os_list;
    os_list.push_back(&std::cerr);
    if (tree_.log_file()) os_list.push_back(tree_.log_file());

    Board::OwnerMap owner = {0};
    double s = tree_.FinalScore(b_, kVtNull, -1, 1024, &owner);
    b_.PrintOwnerMap(s, 1024, owner, os_list);

    if (s == 0) return "0";
    std::stringstream ss;
    ss << (s > 0 ? "B+" : "W+");
    ss << std::fixed << std::setprecision(1) << std::abs(s);

    return ss.str();
  }

  /**
   * Stops analysis for Lizzie.
   */
  void StopLizzieAnalysis() {
    std::lock_guard<std::mutex> lk(mx_);
    running_analysis_ = false;
    cv_.notify_one();
  }

  std::string OnClearBoardCommand();
  std::string OnGenmoveCommand();
  std::string OnPlayCommand();
  std::string OnUndoCommand();
  std::string OnLzAnalyzeCommand();
  std::string OnKgsTimeSettingsCommand();
  std::string OnTimeSettingsCommand();
  std::string OnSetFreeHandicapCommand();
  std::string OnFixedHandicapCommand();
  std::string OnGoguiPlaySequenceCommand();

 private:
  Board b_;
  SearchTree tree_;
  Color c_engine_;
  bool go_ponder_;
  bool save_log_;
  SgfData sgf_;
  std::string log_path_;
  std::string sgf_path_;
  std::vector<std::string> args_;
  bool success_handle_;
  std::mutex mx_;
  std::condition_variable cv_;
  bool running_analysis_;
};

#endif  // GTP_H_
