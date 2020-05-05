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

#include "./sgf.h"

void SgfData::Read(std::string file_path) {
  // Opens an sfg file.
  std::ifstream ifs(file_path);
  std::string buf;
  const auto npos = std::string::npos;
  auto is_numeric = [](const std::string str) {
    if (str.empty()) return false;

    std::string str_ = str;
    auto pos = str_.find(".");
    if (pos != std::string::npos) str_.replace(pos, 1, "");

    return std::find_if(str_.begin(), str_.end(),
                        [](char c) { return !std::isdigit(c); }) == str_.end();
  };

  // Reads lines until eof.
  while (ifs && std::getline(ifs, buf)) {
    // Moves to the next line when remaining letters are less than 4.
    while (buf.size() > 3) {
      // Header is typically written as '(;GM[1]FF[4]CA[UTF-8]...'.
      // Read two-character tags and contents in [] in order.
      std::string tag, in_br;

      // Goes to the next line if [] is not found.
      auto open_br = buf.find("[");
      auto close_br = buf.find("]");

      if (open_br == npos || close_br == npos) break;

      if (close_br == 0) {
        buf = buf.substr(close_br + 1);
        close_br = buf.find("]");
        open_br = buf.find("[");
      }

      tag = buf.substr(0, open_br);
      in_br = buf.substr(open_br + 1, close_br - open_br - 1);

      // Removes semicolon from the tag.
      auto semicolon = tag.find(";");
      if (semicolon != npos) tag = tag.substr(semicolon + 1);

      if (tag == "SZ") {  // board size
        if (std::stoi(in_br) != kBSize) {
          Init();
          return;
        }
      } else if (tag == "KM") {  // komi
        // Checks whether in_br is negative.
        // [-6.5], ...
        int sign_num = 1;
        if (in_br.substr(0, 1).find("-") != npos) {
          sign_num = -1;
          in_br = in_br.substr(1);
        }

        // Checks whether in_br is numeric.
        if (is_numeric(in_br)) {
          // Checks whether it becomes an integer when doubled.
          //   e.g. [3.75] in Chinese rule
          double tmp_floor = 2 * std::stod(in_br) - floor(2 * std::stod(in_br));

          // Doubles komi for Chinese rule.
          if (tmp_floor != 0) {  // Chinese rule.
            komi_ = 2 * sign_num * std::stod(in_br);
          } else {  // Japanese rule.
            komi_ = sign_num * std::stod(in_br);
          }
        }

      } else if (tag == "PW" || tag == "PB") {
        // Player name.
        Color c = tag == "PW" ? kWhite : kBlack;
        player_name_[c] = in_br;
      } else if (tag == "WR" || tag == "BR") {
        // Player rating.

        Color c = tag == "WR" ? kWhite : kBlack;

        // Excludes the trailing '?'.
        if (in_br.find("?") != npos)
          in_br = in_br.substr(0, in_br.length() - 1);

        // Inputs the rating if in_br is numeric.
        if (is_numeric(in_br)) {
          player_rating_[c] = std::stoi(in_br);
        } else if (in_br.length() == 2) {
          // Calculate player rating from the rank if the length of in_br
          // is 2.

          // 3000 if a professional player.
          if (in_br.find("p") != npos || in_br.find("P") != npos)
            player_rating_[c] = 3000;
          // 1d = 1580, 2d = 1760, ... 9d = 3020
          else if (in_br.find("d") != npos || in_br.find("D") != npos)
            player_rating_[c] = 1400 + std::stoi(in_br.substr(0, 1)) * 180;
          // 1k = 1450, 2k = 1350, ...
          else if (in_br.find("k") != npos || in_br.find("K") != npos)
            player_rating_[c] = 1550 - std::stoi(in_br.substr(0, 1)) * 100;
        }

      } else if (tag == "HA") {
        // Number of the handicap stones.
        //   e.g. 2, 3, 4, ...

        // Checks whether in_br is numeric.
        if (is_numeric(in_br)) {
          handicap_ = std::stoi(in_br);
        }
      } else if (tag == "RE") {
        // Result.
        //   e.g. W+R, B+Resign, W+6.5, B+Time, ...

        auto b = in_br.find("B+");
        auto w = in_br.find("W+");

        if (b == npos && w == npos) {
          score_ = 0.0;
        } else {
          if (is_numeric(in_br.substr(2))) {  // Won by score
            score_ = std::stod(in_br.substr(2));
            if (2 * score_ - floor(2 * score_) != 0) score_ *= 2;
            if (w != npos) score_ = -score_;
          } else if (in_br.find("R") != npos) {  // Won by resign
            score_ = w != npos ? -512 : 512;
          } else {  // Won by time or illegal
            score_ = w != npos ? -1024 : 1024;
          }
        }

      } else if (tag == "W" || tag == "B") {
        // Move

        // Checks whether the game starts from white (i.e. a handicap match)
        if (tag == "W" && game_ply() == 0) komi_ = 0;
        move_history_.push_back(sgf2v(in_br));
      } else if (tag == "AB" || tag == "AW") {
        // Handicap stones.
        //   e.g. AB[dd][qq], ...

        handicap_stones_[static_cast<int>(tag == "AB")].push_back(sgf2v(in_br));
        std::string::size_type next_open_br =
            buf.substr(close_br + 1).find("[");
        std::string::size_type next_close_br =
            buf.substr(close_br + 1).find("]");

        // Reads continuous []
        while (next_open_br == 0) {
          open_br = close_br + 1 + next_open_br;
          close_br = close_br + 1 + next_close_br;

          std::stringstream ss_in_br;
          in_br = "";
          ss_in_br << buf.substr(open_br + 1, close_br - open_br - 1);
          ss_in_br >> in_br;
          handicap_stones_[static_cast<int>(tag == "AB")].push_back(
              sgf2v(in_br));

          next_open_br = buf.substr(close_br + 1).find("[");
          next_close_br = buf.substr(close_br + 1).find("]");
        }
      }

      // Excludes tag[in_br] from buf.
      buf = buf.substr(close_br + 1);
    }
  }
}

void SgfData::Write(std::string file_path,
                    std::vector<std::string>* comments) const {
  // Opens file.
  std::stringstream ss;
  std::string rule_str =
      Options["rule"].get_int() == kJapanese ? "Japanese" : "Chinese";

  // Uses fixed header.
  ss << "(;GM[1]FF[4]CA[UTF-8]" << std::endl;
  ss << "RU[" << rule_str << "]SZ[" << kBSize << "]KM[" << komi_ << "]";
  if (player_name_[kWhite] != "" && player_name_[kBlack] != "") {
    ss << "PB[" << player_name_[kBlack] << "]"
       << "PW[" << player_name_[kWhite] << "]";
  }
  if (score_ != 0) {
    std::string winner = (score_ > 0) ? "B+" : "W+";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", std::abs(score_));
    std::string score_str(buf);
    if (std::abs(score_) == 512) score_str = "R";
    ss << "RE[" << winner << score_str << "]";
  } else {
    ss << "RE[0]";
  }
  ss << std::endl;

  std::string str = "abcdefghijklmnopqrs";
  for (int i = 0, n = move_history_.size(); i < n; ++i) {
    Vertex v = move_history_[i];
    int x = x_of(v) - 1;
    int y = kBSize - y_of(v);

    ss << (i % 2 == 0 ? ";B[" : ";W[");
    ss << (v < kPass ? str.substr(x, 1) + str.substr(y, 1) : "") << "]";
    if (comments != nullptr && static_cast<int>(comments->size()) > i)
      ss << "C[" << (*comments)[i] << "]";
    if ((i + 1) % 8 == 0) ss << std::endl;
  }
  ss << ")" << std::endl;

  std::ofstream ofs(file_path.c_str());
  ofs << ss.str();
  ofs.close();
}

bool SgfData::ReconstructBoard(Board* b, int move_idx) const {
  if (move_idx > game_ply()) return false;
  b->Init();

  // place handicap stones
  const int i_max = std::max(handicap_stones_[kWhite].size(),
                             handicap_stones_[kBlack].size());
  for (int i = 0; i < i_max; ++i) {
    for (Color c = kColorZero; c < kNumPlayers; ++c) {
      if (static_cast<int>(handicap_stones_[c].size()) > i) {
        if (!b->IsLegal(handicap_stones_[c][i])) return false;
        b->MakeMove<kOneWay>(handicap_stones_[c][i]);
      } else {
        b->MakeMove<kOneWay>(kPass);
      }
    }
  }

  if (i_max == 0 && handicap_ > 0) {
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
    int hc_idx = handicap_ - 2;
    for (int i = 0; i < handicap_; ++i) {
      int stone_idx = stones[hc_idx][i];
      Vertex v = xy2v(x_[stone_idx], y_[stone_idx]);
      b->MakeMove<kOneWay>(v);
      b->MakeMove<kOneWay>(kPass);
    }
  }

  if ((handicap_stones_[kWhite].size() == 0 &&
       handicap_stones_[kBlack].size() > 0) ||
      handicap_ > 0)
    b->MakeMove<kOneWay>(kPass);  // kBlack kPass

  // Resets game_ply.
  b->set_game_ply(0);
  b->set_num_passes(kWhite, 0);
  b->set_num_passes(kBlack, 0);

  // Initial board.
  if (move_idx == 0) return true;

  // Plays until move_idx.
  for (int i = 0; i < move_idx; ++i) {
    if (!b->IsLegal(move_history_[i])) return false;
    b->MakeMove<kOneWay>(move_history_[i]);
  }

  return true;
}

#ifdef _WIN32
int SgfData::GetSgfFiles(std::string dir_path,
                         std::vector<std::string>* files) {
  int num_sgf_files = 0;
  HANDLE h_find;
  WIN32_FIND_DATA fd;

  if (dir_path.size() < 2) {
    dir_path = ".\\";
  } else if (dir_path.substr(dir_path.size() - 2) != "\\") {
    dir_path += "\\";
  }
  std::string file_path = dir_path + "*.sgf";
  // FindFirstFile needs a char* argument in MinGW-x64.
  h_find = FindFirstFile(file_path.c_str(), &fd);

  // Fails to find.
  if (h_find == INVALID_HANDLE_VALUE) {
    return num_sgf_files;  // 0
  }

  do {
    // Excludes directory.
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
      // fd.cFileName is char* in MinGW-x64.
      std::string file_name = fd.cFileName;
      files->push_back(dir_path + file_name);
      ++num_sgf_files;
    }
  } while (FindNextFile(h_find, &fd));  // Next file.

  FindClose(h_find);
  return num_sgf_files;
}
#else  // Linux
#include <dirent.h>
int SgfData::GetSgfFiles(std::string dir_path,
                         std::vector<std::string>* files) {
  files->clear();
  int num_sgf_files = 0;
  DIR* dr = opendir(dir_path.c_str());
  if (dr == NULL) return num_sgf_files;
  dirent* entry;
  do {
    entry = readdir(dr);
    if (entry != NULL) {
      std::string file_name = entry->d_name;
      if (file_name.find(".sgf") == std::string::npos) continue;
      files->push_back(file_name);
      num_sgf_files++;
    }
  } while (entry != NULL);
  closedir(dr);
  return num_sgf_files;
}
#endif  // _WIN32
