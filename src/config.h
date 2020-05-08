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

#ifndef CONFIG_H_
#define CONFIG_H_

// --------------------
//      include
// --------------------

#ifdef _WIN32
#define COMPILER_MSVC
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <conio.h>
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif
#else
#include <unistd.h>
#endif

// --------------------
//      board size
// --------------------

#ifndef BOARD_SIZE
#define BOARD_SIZE 19
#endif

// --------------------
//      assertion
// --------------------

// #define USE_DEBUG_ASSERT

/**
 * ASSERT, which will not be disabled even if it is not a DEBUG build (since
 * normal asserts will be disabled).
 *
 * Deliberately causing a memory access violation.
 * When USE_DEBUG_ASSERT is enabled, wait 3 seconds after outputting the
 * contents of ASSERT before executing the code that causes an access violation.
 *
 * This code has been based on the following link code as a reference;
 * https://github.com/yaneurao/YaneuraOu/blob/master/source/config.h
 * Revision date: 5/1/2020
 */
#if !defined(USE_DEBUG_ASSERT)
#define ASSERT(X)                             \
  {                                           \
    if (!(X)) *reinterpret_cast<int*>(1) = 0; \
  }
#else
#define ASSERT(X)                                                   \
  {                                                                 \
    if (!(X)) {                                                     \
      std::cout << "\nError : ASSERT(" << #X << ")" << std::endl;   \
      std::this_thread::sleep_for(std::chrono::microseconds(3000)); \
      *reinterpret_cast<int*>(1) = 0;                               \
    }                                                               \
  }
#endif

#ifndef ASSERT_LV
#define ASSERT_LV 0
#endif

#define ASSERT_LV_EX(L, X)         \
  {                                \
    if (L <= ASSERT_LV) ASSERT(X); \
  }
#define ASSERT_LV1(X) ASSERT_LV_EX(1, X)
#define ASSERT_LV2(X) ASSERT_LV_EX(2, X)
#define ASSERT_LV3(X) ASSERT_LV_EX(3, X)

#endif  // CONFIG_H_
