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

#include "./board.h"
#include "./gtp.h"
#include "./option.h"
#include "./test.h"

int main(int argc, char **argv) {
  std::string mode = ReadConfiguration(argc, argv);

  if (mode == "--benchmark") {
    BenchMark();
    NetworkBench();
  } else if (mode == "--test") {
    TestBoard();
  } else if (mode == "--self") {
    SelfMatch();
  } else if (mode == "--policy_self") {
    PolicySelf();
  } else {
    GTPConnector gtp_connector;
    gtp_connector.Start();
  }

  return 0;
}
