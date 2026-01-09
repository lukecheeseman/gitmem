#include "branching/lazy/version_store.hh"
#include "debug.hh"
#include <unordered_set>
#include <functional>

namespace gitmem {

namespace branching {

// std::optional<Value> traverse_to_lca(
//     const std::shared_ptr<const Commit>& commit,
//     ObjectNumber var,
//     const std::shared_ptr<const Commit>& lca)
// {
//     if (!commit || commit == lca) return std::nullopt;

//     auto it = commit->changes.find(var);
//     if (it != commit->changes.end()) return it->second;

//     if (commit->parents.size() > 1)
//         assert(false && "should never encounter multi-parent commit before LCA");

//     return traverse_to_lca(commit->parents[0], var, lca);
// }

// std::optional<Value> get_committed_recursive(
//     const std::shared_ptr<const Commit>& commit,
//     ObjectNumber var) {
//   if (!commit) return std::nullopt;

//   // 1. If this commit wrote the variable, return it
//   auto it = commit->changes.find(var);
//   if (it != commit->changes.end()) return it->second;

//   // 2. If single parent, recurse
//   if (commit->parents.size() == 1)
//       return get_committed_recursive(commit->parents[0], var);

//   // 3. Merge commit
//   assert(commit->parents.size() == 2); // merge commit

//   auto& p1 = commit->parents[0];
//   auto& p2 = commit->parents[1];

//   auto lca = find_lowest_common_ancestor(p1, p2);

//   // Explore both paths from merge commit to LCA
//   std::optional<Value> v1 = traverse_to_lca(p1, var, lca);
//   std::optional<Value> v2 = traverse_to_lca(p2, var, lca);

//   assert(!v1 || !v2 || v1 == v2); // conflict-free invariant

//   if (v1) return v1;          // found in one of the merge branches
//   if (v2) return v2;

//   // 4. Not found yet → continue recursively from the LCA downward
//   return get_committed_recursive(lca, var);
// }

// std::optional<std::shared_ptr<const Commit>>
// get_committed_recursive(
//   const std::shared_ptr<const Commit>& commit,
//   ObjectNumber number,
//   std::unordered_set<std::shared_ptr<const Commit>>& visited) {

//   if (!commit || !visited.insert(commit).second)
//     return std::nullopt;

//   std::optional<std::shared_ptr<const Commit>> found;

//   // Recurse into all parents first
//   for (auto& parent : commit->parents) {
//     auto parent_commit = get_committed_recursive(parent, number, visited);
//     if (parent_commit) {
//       if (!found.has_value())
//         found = parent_commit;
//       else if (found.value()->changes.at(number) != parent_commit.value()->changes.at(number))
//         assert(false && "Conflict detected (should be impossible in conflict-free DAG)");
//     }
//   }

//   // If this commit wrote the variable, it overrides any parent
//   if (commit->changes.contains(number))
//     return commit;

//   return found;
// }


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
  // Thought, if we merge two paths that conflict on a variable, but we never read it
  // if (auto it = last_writer.find(var); it != last_writer.end()) {
    // return it->second->changes.at(var);
  // }

  std::vector<std::shared_ptr<const Commit>> writers;
  std::unordered_map<std::shared_ptr<const Commit>, bool> reach_memo;

  // std::cout << "Get committed for " << var << std::endl;

  std::function<void(std::shared_ptr<const Commit>)> dfs;
  dfs = [&](std::shared_ptr<const Commit> c) {
    if (!c) return;
    // std::cout << c->id << " changes: {";
    // for (const auto& [k, v] : c->changes) {
      // std::cout << k << "->"  << v << ",";
    // }
    // std::cout << "}" << std::endl;

    // If we've already found a writer that is an ancestor of c, skip
    for (auto it = writers.begin(); it != writers.end(); ) {
      if (can_reach(c, *it, reach_memo)) {
        // std::cout << c->id << " can reach " << (*it)->id << std::endl;
        // existing writer is ancestor of this commit, remove it
        it = writers.erase(it);
      } else if (can_reach(*it, c, reach_memo)) {
        // this commit is ancestor of existing writer, ignore this path
        return;
      } else {
          ++it;
      }
    }

    if (c->changes.contains(var)) {
      writers.push_back(c);
      return;
    }

    for (auto& p : c->parents)
      dfs(p);
  };

  dfs(head);

  // std::cout << "=====================================" << std::endl;

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