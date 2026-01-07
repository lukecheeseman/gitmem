#include "version_store.hh"
#include <iostream>
#include <unordered_set>
#include "debug.hh"

namespace gitmem {

namespace branching {

// Helper: recursive print with 2-space indentation and cycle protection
void print_commit_recursive(std::ostream& os,
                            const std::shared_ptr<const Commit>& commit,
                            std::unordered_set<const Commit*>& visited,
                            int depth = 0)
{
  if (!commit) return;
  if (!visited.insert(commit.get()).second) {
    os << std::string(depth * 2, ' ') << "(already printed commit " << commit->id << ")\n";
    return;
  }

  os << std::string(depth * 2, ' ') << "Commit " << commit->id << " {\n";

  // Print changes
  for (const auto& [obj, val] : commit->changes) {
    os << std::string((depth + 1) * 2, ' ') << obj << " -> " << val << "\n";
  }

  // Print parents
  if (!commit->parents.empty()) {
    os << std::string((depth + 1) * 2, ' ') << "Parents: ";
    for (size_t i = 0; i < commit->parents.size(); ++i) {
      os << commit->parents[i]->id;
      if (i + 1 < commit->parents.size()) os << ", ";
    }
    os << "\n";
  }

  os << std::string(depth * 2, ' ') << "}\n";

  // Recursively print parents
  for (auto& parent : commit->parents) {
    print_commit_recursive(os, parent, visited, depth + 1);
  }
}

// operator<< for Commit
std::ostream& operator<<(std::ostream& os, const Commit& commit) {
  std::unordered_set<const Commit*> visited;
  // Wrap the commit in a shared_ptr to reuse the recursive helper
  print_commit_recursive(os, std::make_shared<const Commit>(commit), visited);
  return os;
}

void LocalVersionStore::stage(ObjectNumber obj, Value value) {
  staging[obj] = value;
}

void LocalVersionStore::commit_staging() {
  // No-op commit does nothing
  if (staging.empty()) {
    return;
  }

  // Create the new commit with the staged changes
  auto new_commit = std::make_shared<Commit>(base_timestamp++, std::move(staging));

  // Update last_writer for each staged variable
  for (const auto& [obj, _] : new_commit->changes) {
    last_writer[obj] = new_commit;
  }

  // Clear staging
  staging.clear();

  // Set parent to previous head if it exists
  if (head)
    new_commit->parents.push_back(head);

  // Update head
  head = new_commit;
}

std::optional<Value> LocalVersionStore::get_staged(ObjectNumber obj) const {
  auto it = staging.find(obj);
  return it != staging.end() ? std::make_optional(it->second) : std::nullopt;
}

std::optional<std::shared_ptr<const Commit>>
get_committed_recursive(
  const std::shared_ptr<const Commit>& commit,
  ObjectNumber number,
  std::unordered_set<std::shared_ptr<const Commit>>& visited) {

  if (!commit || !visited.insert(commit).second)
    return std::nullopt;

  std::optional<std::shared_ptr<const Commit>> found;

  // Recurse into all parents first
  for (auto& parent : commit->parents) {
    auto parent_commit = get_committed_recursive(parent, number, visited);
    if (parent_commit) {
      if (!found.has_value())
        found = parent_commit;
      else if (found.value()->changes.at(number) != parent_commit.value()->changes.at(number))
        assert(false && "Conflict detected (should be impossible in conflict-free DAG)");
    }
  }

  // If this commit wrote the variable, it overrides any parent
  if (commit->changes.contains(number))
    return commit;

  return found;
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

std::optional<Value> LocalVersionStore::get_committed(ObjectNumber number) const {
  if (auto it = last_writer.find(number); it != last_writer.end())
    return it->second->changes.at(number);

  return std::nullopt;
}

void LocalVersionStore::adopt_history(const LocalVersionStore& other) {
  // Inherit the DAG head
  head = other.head;

  // Inherit the last_writer cache so the child sees all latest commits
  last_writer = other.last_writer;
}

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

std::optional<Conflict> LocalVersionStore::merge_with(const LocalVersionStore& other) {
  assert(staging.empty());
  assert(other.staging.empty());

  // trivial case: same history
  if (head == other.head)
    return std::nullopt;

  // Create merge commit (no changes itself)
  auto merge_commit = std::make_shared<const Commit>(
    Commit{
      .id = base_timestamp++,
      .parents = {head, other.head},
      .changes = {}  // merge commit does not write anything
    }
  );

  // Find lowest common ancestor of the two heads
  std::shared_ptr<const Commit> lca = find_lowest_common_ancestor(head, other.head);
  verbose << "found lca of " << head->id << " and " << other.head->id << " to be " << lca->id << std::endl;

  // Collect all writes after LCA for each branch
  std::unordered_map<ObjectNumber, std::shared_ptr<const Commit>> branch_a, branch_b;
  std::unordered_set<std::shared_ptr<const Commit>> visited;

  std::unordered_map<std::shared_ptr<const Commit>, bool> reach_memo;
  traverse_until_lca(head, lca, branch_a, visited, reach_memo);
  visited.clear();
  traverse_until_lca(other.head, lca, branch_b, visited, reach_memo);

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

std::ostream& operator<<(std::ostream& os, const LocalVersionStore& store) {
  os << "LocalVersionStore{"
     << "base=" << store.base_timestamp
     << ", head=";

  if (store.head)
    os << store.head->id;
  else
    os << "null";

  os << ", staged={";

  bool first = true;
  for (const auto& [obj, val] : store.staging) {
    if (!first) os << ", ";
    first = false;
    os << obj << "->" << val;
  }

  os << "}}";
  return os;
}

bool LocalVersionStore::operator==(const LocalVersionStore& other) const {
  return base_timestamp == other.base_timestamp &&
         head == other.head &&
         staging == other.staging;
}

ObjectNumber GlobalVersionStore::get_object_number(std::string var) {
  auto it = _object_numbers.find(var);
  if (it != _object_numbers.end()) {
    return it->second;
  } else {
    ObjectNumber number = _next_object++;
    _object_numbers[var] = number;
    return number;
  }
}

std::string GlobalVersionStore::get_object_name(ObjectNumber find) {
  for (const auto &[name, number] : _object_numbers) {
    if (number == find)
      return name;
  }
  assert(false && "failed to find object name for object number");
  return "";
}


std::ostream& operator<<(std::ostream& os, const GlobalVersionStore& store) {
  os << "GlobalVersionStore(next_object=" << store._next_object << ")" << std::endl;
  return os;
}

} // branching

} // gitmem