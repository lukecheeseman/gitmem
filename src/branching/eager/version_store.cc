#include "branching/eager/version_store.hh"
#include "debug.hh"

#include <unordered_set>

namespace gitmem {

namespace branching {

bool can_reach_lca(
    const std::shared_ptr<const Commit>& commit,
    const std::shared_ptr<const Commit>& lca,
    std::unordered_map<std::shared_ptr<const Commit>, bool>& memo)
{
  if (!commit)
    return false;

  if (commit == lca)
    return true;

  auto it = memo.find(commit);
  if (it != memo.end())
    return it->second;

  for (const auto& parent : commit->parents) {
    if (can_reach_lca(parent, lca, memo)) {
      memo[commit] = true;
      return true;
    }
  }

  memo[commit] = false;
  return false;
}

bool traverse_until_lca(
  const std::shared_ptr<const Commit>& commit,
  const std::shared_ptr<const Commit>& lca,
  std::unordered_map<ObjectNumber, std::shared_ptr<const Commit>>& out_map,
  std::unordered_set<std::shared_ptr<const Commit>>& visited,
  std::unordered_map<std::shared_ptr<const Commit>, bool>& reach_memo)
{
  if (!commit || commit == lca || !visited.insert(commit).second)
    return true;

  if (!can_reach_lca(commit, lca, reach_memo))
    return true;

  for (const auto& [obj, _] : commit->changes) {
    // first write seen dominates
    if (out_map.find(obj) == out_map.end())
      out_map[obj] = commit;
  }

  for (auto& parent : commit->parents) {
    if (!traverse_until_lca(parent, lca, out_map, visited, reach_memo))
      return false;
  }

  return true;
}

std::shared_ptr<const Commit>
find_lowest_common_ancestor(std::shared_ptr<const Commit> a,
                            std::shared_ptr<const Commit> b)
{
  if (!a || !b) return nullptr;

  // Step 1: collect all ancestors of 'a'
  std::unordered_set<std::shared_ptr<const Commit>> ancestors_a;
  std::queue<std::shared_ptr<const Commit>> q;
  q.push(a);

  while (!q.empty()) {
    auto c = q.front(); q.pop();
    if (!c) continue;
    if (!ancestors_a.insert(c).second) continue; // already visited

    for (auto& p : c->parents)
      q.push(p);
  }

  // Step 2: BFS from 'b' to find first common ancestor
  std::unordered_set<std::shared_ptr<const Commit>> visited_b;
  q.push(b);

  while (!q.empty()) {
    auto c = q.front(); q.pop();
    if (!c) continue;
    if (!visited_b.insert(c).second) continue;

    if (ancestors_a.count(c))
      return c;  // first common ancestor seen

    for (auto& p : c->parents)
      q.push(p);
  }

  return nullptr;  // disjoint histories (shouldn’t happen)
}

std::optional<Conflict> EagerLocalVersionStore::merge_with_commit(const std::shared_ptr<const Commit>& commit) {
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

  // Find lowest common ancestor of the two heads
  std::shared_ptr<const Commit> lca = find_lowest_common_ancestor(head, commit);
  verbose << "found lca of " << head->id << " and " << commit->id << " to be " << lca->id << std::endl;

  // Collect all writes after LCA for each branch
  std::unordered_map<ObjectNumber, std::shared_ptr<const Commit>> branch_a, branch_b;
  std::unordered_set<std::shared_ptr<const Commit>> visited;

  std::unordered_map<std::shared_ptr<const Commit>, bool> reach_memo;
  traverse_until_lca(head, lca, branch_a, visited, reach_memo);
  visited.clear();
  traverse_until_lca(commit, lca, branch_b, visited, reach_memo);

  // 1. Eager conflict detection
  for (const auto& [obj, commit_a] : branch_a) {
    auto it = branch_b.find(obj);
    if (it != branch_b.end() && it->second != commit_a) {
      return Conflict{
        .obj = obj,
        .timestamp_a = commit_a->id,
        .timestamp_b = it->second->id
      };
    }
  }

  // 2. Update thread-local last_writer incrementally
  // Only overwrite variables that were touched along either branch after LCA
  for (const auto& [obj, commit] : branch_a)
    last_writer[obj] = commit;

  for (const auto& [obj, commit] : branch_b)
    last_writer[obj] = commit;

  // 3. Variables not touched in either branch remain unchanged (from before LCA)

  // 4. Update head
  head = merge_commit;

  return std::nullopt;
}

std::optional<Value> EagerLocalVersionStore::get_committed(ObjectNumber number) const {
  if (auto it = last_writer.find(number); it != last_writer.end())
    return it->second->changes.at(number);

  return std::nullopt;
}

} // end branching

} // end gitmem