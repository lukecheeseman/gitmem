#pragma once


#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <cstdint>

namespace gitmem {

namespace linear {

// -----------------------------
// Timestamp
// -----------------------------

class LargeCounter {
  uint64_t _epoch{0};
  uint64_t _counter{0};

public:
  auto operator<=>(const LargeCounter&) const = default;

  LargeCounter& operator++() {
    if (_counter == UINT64_MAX) {
      _counter = 0;
      assert(_epoch != UINT64_MAX && "timestamp overflow");
      ++_epoch;
    } else {
      ++_counter;
    }
    return *this;
  }

  LargeCounter operator++(int) {
    LargeCounter old = *this;
    ++(*this);
    return old;
  }
};

using Timestamp = LargeCounter;
using Value = size_t;
using ObjectNumber = uint64_t;

// -----------------------------
// Version
// -----------------------------

class Version {
  Timestamp _timestamp;
  Value _value;

public:
  Version(Timestamp ts, Value value)
    : _timestamp(ts), _value(value) {}

  Timestamp timestamp() const { return _timestamp; }
  Value value() const { return _value; }
};

using VersionHistory = std::vector<Version>;

// -----------------------------
// Conflict
// -----------------------------

struct Conflict {
  ObjectNumber object;
  Timestamp local_base;
  Timestamp global_head;
};

// -----------------------------
// LocalVersionStore
// -----------------------------

class LocalVersionStore {
  Timestamp _base_timestamp{};
  std::unordered_map<ObjectNumber, Value> _staging;

public:
  Timestamp base_timestamp() const { return _base_timestamp; }
  const auto& staged_changes() const { return _staging; }

  void stage(ObjectNumber obj, Value value);
  void clear_staging();
  void advance_base(Timestamp ts);
};

// -----------------------------
// GlobalVersionStore
// -----------------------------

class GlobalVersionStore {
  Timestamp _timestamp{};
  ObjectNumber _next_object{0};
  std::unordered_map<ObjectNumber, VersionHistory> _history;

public:
  Timestamp current_timestamp() const { return _timestamp; }

  ObjectNumber allocate_object();

  std::optional<Conflict> check_conflicts(
    Timestamp base,
    const std::unordered_map<ObjectNumber, Value>& changes
  ) const;

  Timestamp apply_changes(
    Timestamp base,
    const std::unordered_map<ObjectNumber, Value>& changes
  );
};

// -----------------------------
// Synchronisation Protocol
// -----------------------------

class GlobalVersionHistory {
  GlobalVersionStore _global;

public:
  std::optional<Conflict> push(LocalVersionStore& local);
  std::optional<Conflict> pull(LocalVersionStore& local);
};

} // namespace linear

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