#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include "thread_id.hh"
#include "sync_state.hh"
#include "read_result.hh"

namespace gitmem {

namespace branching {

struct Timestamp {
  ThreadID thread;
  size_t counter;

  auto operator<=>(const Timestamp &) const = default;

  // pre-increment
  Timestamp& operator++() {
    ++counter;
    return *this;
  }

  // post-increment
  Timestamp operator++(int) {
    Timestamp old = *this;
    ++(*this);
    return old;
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  const Timestamp &ts) {
    os << ts.thread << ":" << ts.counter;
    return os;
  }
};

using ObjectNumber = uint64_t;

struct Commit {
  Timestamp id;
  std::unordered_map<ObjectNumber, Value> changes;
  std::vector<std::shared_ptr<const Commit>> parents;
};

std::ostream& operator<<(std::ostream& os, const Commit& commit);

struct Conflict {
  ObjectNumber obj;
  Timestamp timestamp_a;
  Timestamp timestamp_b;
};

inline std::ostream& operator<<(std::ostream& os, const Conflict& c) {
  return os << "Conflict{obj=" << c.obj
            << ", timestamp_a=" << c.timestamp_a
            << ", timestamp_b=" << c.timestamp_b << "}";
}

class LocalVersionStore : public ThreadSyncState {
protected:
  Timestamp base_timestamp;
  std::shared_ptr<const Commit> head;
  std::unordered_map<ObjectNumber, Value> staging;

  std::unordered_map<ObjectNumber, std::shared_ptr<const Commit>> last_writer; // cached

public:
  ~LocalVersionStore() = default;

  LocalVersionStore(ThreadID tid): base_timestamp(tid, 0) {}

  void stage(ObjectNumber obj, Value value);
  void commit_staging();

  bool has_commited() { return staging.empty(); }

  std::shared_ptr<const Commit> get_head() const { return head; }

private:
  virtual ReadResult get_committed(ObjectNumber number) const = 0;

public:
  ReadResult read(ObjectNumber number) const;

  void adopt_history(const LocalVersionStore& other);
  virtual std::optional<Conflict> merge_with_commit(const std::shared_ptr<const Commit>& other_head) = 0;

  friend std::ostream& operator<<(std::ostream& os, const LocalVersionStore& store);
  std::ostream &print(std::ostream &os) const override {
    os << *dynamic_cast<const LocalVersionStore*>(this);
    return os;
  }

  bool operator==(const LocalVersionStore& other) const;
  bool operator==(const ThreadSyncState& other) const override {
    auto* o = dynamic_cast<const LocalVersionStore*>(&other);
    if (!o)
      return false;
    return *this == *o;
  }
};

class GlobalVersionStore {
  ObjectNumber _next_object{0};
  std::unordered_map<std::string, ObjectNumber> _object_numbers;

public:

  ObjectNumber get_object_number(std::string);
  std::string get_object_name(ObjectNumber);

  friend std::ostream& operator<<(std::ostream&, const GlobalVersionStore&);
};

class LockState : public LockSyncState {
public:
  std::shared_ptr<const branching::Commit> commit;
};

} // namespace branching

} // namespace gitmem