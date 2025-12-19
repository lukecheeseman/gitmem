#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace gitmem {

namespace branching {

/* A 'Global' is a structure to capture the current synchronising objects
 * representation of a global variable. The structure is the current value,
 * the current commit id for the variable, and the history of commited ids.
 */

using Commit = size_t;
using CommitHistory = std::vector<Commit>;

struct Global {
  size_t val;
  std::optional<Commit> commit;
  CommitHistory history;
};

using Globals = std::unordered_map<std::string, Global>;

struct Conflict {
  std::string var;
  std::pair<Commit, Commit> commits;
};

struct LocalVersionStore {};

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