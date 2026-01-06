#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include "thread_id.hh"

namespace gitmem {

namespace branching {

/* A 'Global' is a structure to capture the current synchronising objects
 * representation of a global variable. The structure is the current value,
 * the current commit id for the variable, and the history of commited ids.
 */

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

using Value = size_t;
using ObjectNumber = uint64_t;

struct Commit {
  Timestamp id;
  std::unordered_map<ObjectNumber, Value> changes;
  std::vector<std::shared_ptr<const Commit>> parents;
};

// operator<< for Commit
std::ostream& operator<<(std::ostream& os, const Commit& commit);

struct Conflict {
  ObjectNumber obj;
  Timestamp timestamp_a;
  Timestamp timestamp_b;
};

// Initial plumbing for fail late
// enum class ReadKind {
//   NotFound,
//   Value,
//   Conflict
// };

// struct ReadResult {
//   ReadKind kind;
//   std::optional<Value> value; // only valid if kind == Value
// };


class LocalVersionStore {
  Timestamp base_timestamp;
  std::shared_ptr<const Commit> head;
  std::unordered_map<ObjectNumber, Value> staging;

  std::unordered_map<ObjectNumber, std::shared_ptr<const Commit>> last_writer; // cached

public:
  LocalVersionStore(ThreadID tid): base_timestamp(tid, 0) {}

  void stage(ObjectNumber obj, Value value);
  void commit_staging();

  bool has_commited() { return staging.empty(); }

  std::optional<Value> get_staged(ObjectNumber obj) const;
  std::optional<Value> get_committed(ObjectNumber number) const;

  void adopt_history(const LocalVersionStore& other);
  std::optional<Conflict> merge_with(const LocalVersionStore& other);

  friend std::ostream& operator<<(std::ostream& os, const LocalVersionStore& store);
  bool operator==(const LocalVersionStore& other) const;

  void dump() { if (head) std::cout << *head << "\n============\n" << std::endl; }
};

class GlobalVersionStore {
  ObjectNumber _next_object{0};
  std::unordered_map<std::string, ObjectNumber> _object_numbers;

public:

  ObjectNumber get_object_number(std::string);
  std::string get_object_name(ObjectNumber);

  friend std::ostream& operator<<(std::ostream&, const GlobalVersionStore&);
};

// using CommitHistory = std::vector<Commit>;

// struct Global {
//   size_t val;
//   std::optional<Commit> commit;
//   CommitHistory history;
// };

// using Globals = std::unordered_map<std::string, Global>;

// struct Conflict {
//   std::string var;
//   std::pair<Commit, Commit> commits;
// };

// Join logic
// commit(ctx.globals);
// commit(thread->ctx.globals);
// verbose << "Pulling from thread " <<  result << std::endl;
// if(auto conflict = pull(ctx.globals, thread->ctx.globals))
// {
//     using graph::Node;
//     auto [s1, s2] = conflict->commits;
//     auto sources = std::pair<std::shared_ptr<Node>,
//     std::shared_ptr<Node>>{gctx.commit_map[s1], gctx.commit_map[s2]}; auto
//     graph_conflict = graph::Conflict(conflict->var, sources);
//     thread_append_node<graph::Join>(ctx, result, thread->ctx.tail,
//     graph_conflict); return TerminationStatus::datarace_exception;
// }

} // namespace branching

} // namespace gitmem