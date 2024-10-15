#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "pin.H"

template <std::size_t N>
void pretty_print_char_buf(std::ostream &os, const unsigned char (&arr)[N],
                           std::size_t size) {
  bool first = true;

  os << std::right << std::noshowbase << std::hex << std::setfill('0');

  for (std::size_t i = 0; i < std::min(N, size); ++i) {
    os << (first ? "" : " ") << std::setw(2)
       << static_cast<unsigned int>(arr[i]);
    first = false;
  }

  os << std::left << std::showbase << std::dec << std::setfill(' ');
}

void pretty_print_char_buf(std::ostream &os,
                           const std::vector<unsigned char> &vec) {
  bool first = true;
  os << std::right << std::noshowbase << std::hex << std::setfill('0');

  for (const unsigned char val : vec) {
    os << (first ? "" : " ") << std::setw(2) << static_cast<unsigned int>(val);
    first = false;
  }

  os << std::left << std::showbase << std::dec << std::setfill(' ');
}

void pretty_print_values(
    std::ostream &os, const std::set<std::vector<unsigned char>> &container) {
  bool first = true;

  os << '[';

  for (const auto &val : container) {
    os << (first ? "" : ", ");
    pretty_print_char_buf(os, val);
    first = false;
  }

  os << ']';
}

// Get the filename from an absolute path. For example, /path/to/test.so becomes
// test.so.
std::string get_filename(const std::string &path) {
  // Look for last path separator in path string.
  auto last_path_sep = path.rfind('/');

  if (last_path_sep != std::string::npos) {
    // Return string after last path separator.
    return path.substr(last_path_sep + 1);
  } else {
    // No path separator found, so just return entire string.
    return path;
  }
}

#endif
