#pragma once


#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <cstdint>

namespace gitmem {

namespace branching {

  /* A 'Global' is a structure to capture the current synchronising objects
  * representation of a global variable. The structure is the current value,
  * the current commit id for the variable, and the history of commited ids.
  */

  using Commit = size_t;
  using CommitHistory = std::vector<Commit>;

  struct Global
  {
      size_t val;
      std::optional<Commit> commit;
      CommitHistory history;
  };

  using Globals = std::unordered_map<std::string, Global>;

  using Locals = std::unordered_map<std::string, size_t>;


  struct Conflict
  {
      std::string var;
      std::pair<Commit, Commit> commits;
  };

} // namespace branching

} // namespace gitmem