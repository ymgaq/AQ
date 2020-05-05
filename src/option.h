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

#ifndef OPTION_H_
#define OPTION_H_

#include <string>
#include <unordered_map>

#include "./config.h"

// --------------------
//       Option
// --------------------

/**
 * @enum OptionType
 * Data type to be stored in the Option class.
 */
enum OptionType {
  kOptionNone,
  kOptionString,
  kOptionBool,
  kOptionInt,
  kOptionDouble,
};

/**
 * @class Option
 * Option class holds the options specified by the command line argument, which
 * are bool, int, double, and string for each option type. The data type is
 * determined by the constructor.
 */
class Option {
 public:
  typedef std::unordered_map<std::string, Option> OptionsMap;

  // Constructor
  Option() : type_(kOptionNone), min_(0), max_(0) {}

  explicit Option(const char* v)
      : type_(kOptionString), val_(v), min_(0), max_(0) {}

  explicit Option(bool v)
      : type_(kOptionBool), val_(v ? "true" : "false"), min_(0), max_(0) {}

  Option(int v, int min_v, int max_v)
      : type_(kOptionInt), val_(std::to_string(v)), min_(min_v), max_(max_v) {}

  explicit Option(double v)
      : type_(kOptionDouble), val_(std::to_string(v)), min_(0), max_(0) {}

  // Copy
  Option& operator=(std::string v) {
    ASSERT_LV1(type_ != kOptionNone);

    // Out of range or invalid argument.
    if (type_ != kOptionString && v.empty() ||
        (type_ == kOptionBool && v != "true" && v != "false") ||
        (type_ == kOptionInt && (stoll(v) < min_ || stoll(v) > max_)))
      return *this;

    val_ = v;
    return *this;
  }

  Option& operator=(const char* ptr) { return *this = std::string(ptr); }

  Option& operator=(int v) {
    ASSERT_LV1(type_ == kOptionInt || type_ == kOptionDouble);
    return *this = std::to_string(v);
  }

  Option& operator=(bool v) {
    ASSERT_LV1(type_ == kOptionBool);
    return *this = (v ? "true" : "false");
  }

  Option& operator=(double v) {
    ASSERT_LV1(type_ == kOptionDouble);
    return *this = std::to_string(v);
  }

  void operator<<(const Option& o) { *this = o; }

  // Accessor for each data type.
  int get_int() const {
    ASSERT_LV1(type_ == kOptionInt);
    return std::stoi(val_);
  }

  bool get_bool() const {
    ASSERT_LV1(type_ == kOptionBool);
    return (val_ == "true");
  }

  double get_double() const {
    ASSERT_LV1(type_ == kOptionDouble);
    return std::stod(val_);
  }

  std::string get_string() const {
    ASSERT_LV1(type_ != kOptionNone);
    return val_;
  }

  // Implicit type conversion.
  // Used for variable initialization and conditional branching by options of
  // kOptionBool.
  operator int() const {
    ASSERT_LV1(type_ == kOptionInt);
    return std::stoi(val_);
  }

  operator bool() const {
    ASSERT_LV1(type_ == kOptionBool);
    return (val_ == "true");
  }

  operator double() const {
    ASSERT_LV1(type_ == kOptionDouble);
    return std::stod(val_);
  }

  operator std::string() const {
    ASSERT_LV1(type_ != kOptionNone);
    return val_;
  }

 private:
  OptionType type_;
  std::string val_;
  int min_;
  int max_;
};

/**
 * Map to store options with command line arguments.
 */
extern Option::OptionsMap Options;

/**
 * Combine directory and file paths.
 */
std::string JoinPath(const std::string s1, const std::string s2,
                     const std::string s3 = "");

/**
 * Parse config.txt and command line arguments and reflect them in Options.
 */
std::string ReadConfiguration(int argc, char** argv);

#endif  // OPTION_H_
