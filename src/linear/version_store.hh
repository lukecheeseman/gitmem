#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace gitmem {

namespace linear {

// -----------------------------
// Timestamp
// -----------------------------

class LargeCounter {
  uint64_t _epoch{0};
  uint64_t _counter{0};

public:
  auto operator<=>(const LargeCounter &) const = default;

  LargeCounter &operator++() {
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

  friend std::ostream &operator<<(std::ostream &os,
                                  const LargeCounter &counter) {
    os << counter._epoch << ":" << counter._counter;
    return os;
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
  Version(Timestamp ts, Value value) : _timestamp(ts), _value(value) {}

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
  const auto &staged_changes() const { return _staging; }

  void stage(ObjectNumber obj, Value value);
  void clear_staging();
  void advance_base(Timestamp ts);
  std::optional<Value> get_staged(ObjectNumber obj);

  bool operator==(const LocalVersionStore& other) const;

  friend std::ostream& operator<<(std::ostream& os, const LocalVersionStore& store) {
    os << "LocalVersionStore{"
      << "base=" << store._base_timestamp
      << ", staged={";

    bool first = true;
    for (const auto& [obj, val] : store._staging) {
      if (!first) os << ", ";
      first = false;
      os << obj << "->" << val;
    }

    os << "}}";
    return os;
  }
};

// -----------------------------
// GlobalVersionStore
// -----------------------------

class GlobalVersionStore {
  Timestamp _timestamp{};
  ObjectNumber _next_object{0};
  std::unordered_map<ObjectNumber, VersionHistory> _history;
  std::unordered_map<std::string, ObjectNumber> _object_numbers;

public:
  Timestamp current_timestamp() const { return _timestamp; }

  ObjectNumber get_object_number(std::string);
  std::string get_object_name(ObjectNumber);

  std::optional<Value> get_version_for_timestamp(ObjectNumber, Timestamp) const;

  std::optional<Conflict>
  check_conflicts(Timestamp base,
                  const std::unordered_map<ObjectNumber, Value> &changes) const;

  Timestamp
  apply_changes(Timestamp base,
                const std::unordered_map<ObjectNumber, Value> &changes);
};

} // namespace linear

} // namespace gitmem