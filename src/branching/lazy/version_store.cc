#include "branching/lazy/version_store.hh"
#include "debug.hh"

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


std::optional<Conflict> LazyLocalVersionStore::merge_with_commit(const std::shared_ptr<const Commit>&) {
  assert(false && "todo");
  return std::nullopt;

}

std::optional<Value> LazyLocalVersionStore::get_committed(ObjectNumber number) const {
  assert(false && "todo");
  return std::nullopt;
}

}

}