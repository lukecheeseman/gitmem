#pragma once

#include <iostream>

namespace gitmem {

/* For debug printing */
inline struct Verbose {
  bool enabled = false;

  template <typename T> const Verbose &operator<<(const T &msg) const {
    if (enabled)
      std::cout << msg;
    return *this;
  }

  const Verbose &operator<<(std::ostream &(*manip)(std::ostream &)) const {
    if (enabled)
      std::cout << manip;
    return *this;
  }
} verbose;

} // namespace gitmem