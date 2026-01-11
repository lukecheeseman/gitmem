#include "branching/lazy/version_store.hh"
#include "debug.hh"
#include <unordered_set>
#include <functional>

namespace gitmem {

namespace branching {

std::optional<Conflict> LazyLocalVersionStore::merge_with_commit(const std::shared_ptr<const Commit>& commit) {
  assert(staging.empty());
  assert(commit != nullptr);

  // trivial case: same history
  if (head == commit)
    return std::nullopt;

  // Create merge commit (no changes itself)
  auto merge_commit = std::make_shared<const Commit>(
    Commit{
      .id = base_timestamp++,
      .parents = {head, commit},
      .changes = {}  // merge commit does not write anything
    }
  );

  // don't check for conflicts, we do that when later read a variable
  head = merge_commit;

  // whenever we merge, we loose all the information about the last writer
  // last_writer.clear();

  return std::nullopt;
}

// Thought, if we merge two paths that conflict on a variable, but we never read it
// and just right to it, is that okay ?
// if (auto it = last_writer.find(number); it != last_writer.end()) {
//   return it->second->changes.at(number);
// }

BranchingReadResult LazyLocalVersionStore::get_committed(std::string var) const {
  std::vector<std::shared_ptr<const Commit>> writers;

  std::function<void(std::shared_ptr<const Commit>)> dfs;
  dfs = [&](std::shared_ptr<const Commit> c) {
    if (!c) return;

    // Check if c is an ancestor of any existing writer
    {
      std::unordered_map<std::shared_ptr<const Commit>, bool> reach_memo;
      for (const auto& writer : writers) {
        if (can_reach(writer, c, reach_memo)) {
          // c is ancestor of existing writer, ignore this path
          return;
        }
      }
    }

    // Remove any existing writers that are ancestors of c
    {
      std::unordered_map<std::shared_ptr<const Commit>, bool> reach_memo;
      writers.erase(
        std::remove_if(writers.begin(), writers.end(),
          [&](const auto& writer) { return can_reach(c, writer, reach_memo); }),
        writers.end()
      );
    }

    if (c->changes.contains(var)) {
      writers.push_back(c);
      return;
    }

    for (auto& p : c->parents)
      dfs(p);
  };

  dfs(head);

  if (writers.empty()) return std::monostate{};
  if (writers.size() == 1) {
    // last_writer[var] = writers[0];
    return writers[0]->changes.at(var);
  }

  // conflict
  auto a = writers[0]->id;
  auto b = writers[1]->id;
  return Conflict(var, a, b);
}


}

}