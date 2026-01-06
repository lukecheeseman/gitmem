#include "version_store.hh"
#include <iostream>
#include <unordered_set>

namespace gitmem {

namespace branching {

void LocalVersionStore::stage(ObjectNumber obj, Value value) {
  staging[obj] = value;
}

void LocalVersionStore::commit_staging() {
  // No-op commit does nothing
  if (staging.empty()) {
    return;
  }

  auto new_commit = std::make_shared<Commit>(base_timestamp++, std::move(staging));
  staging.clear();

  if (head)
    new_commit->parents.push_back(head);

  head = new_commit;
}

std::optional<Value> LocalVersionStore::get_staged(ObjectNumber obj) const {
  auto it = staging.find(obj);
  return it != staging.end() ? std::make_optional(it->second) : std::nullopt;
}

std::optional<Value> get_committed_recursive(
    std::shared_ptr<const Commit> commit,
    ObjectNumber number,
    std::unordered_set<std::shared_ptr<const Commit>>& visited)
{
  if (!commit || !visited.insert(commit).second) return std::nullopt;

  // check if commit explicitly wrote the variable
  auto it = commit->changes.find(number);
  if (it != commit->changes.end()) return it->second;

  std::optional<Value> found;
  for (const auto& parent : commit->parents) {
    auto val = get_committed_recursive(parent, number, visited);
    if (!val.has_value()) continue;

    if (!found.has_value())
      found = val;              // first value found
    else if (found.value() != val.value())
      assert(false && "Conflict detected on read should have been detected earlier"); // lazy conflict
  }

  return found;
}

std::optional<Value> LocalVersionStore::get_committed(ObjectNumber number) const {
  // for now assume early resolution of conflicts
  std::unordered_set<std::shared_ptr<const Commit>> visited;
  return get_committed_recursive(head, number, visited);
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

// Late resolution

// ReadResult get_committed_recursive(
//     std::shared_ptr<const Commit> commit,
//     ObjectNumber number,
//     std::unordered_set<std::shared_ptr<const Commit>>& visited) {
//   if (!commit || !visited.insert(commit).second)
//     return { ReadKind::NotFound, std::nullopt };

//   // Explicit write dominates
//   auto it = commit->changes.find(number);
//   if (it != commit->changes.end())
//     return { ReadKind::Value, it->second };

//   std::optional<Value> found;

//   for (const auto& parent : commit->parents) {
//     auto res = get_committed_recursive(parent, number, visited);

//     if (res.kind == ReadKind::Conflict)
//       return res;

//     if (res.kind == ReadKind::Value) {
//       if (!found)
//         found = res.value;
//       else if (*found != *res.value)
//         return { ReadKind::Conflict, std::nullopt };
//     }
//   }

//   if (found)
//     return { ReadKind::Value, *found };

//   return { ReadKind::NotFound, std::nullopt };
// }

// ReadResult LocalVersionStore::get_committed(ObjectNumber number) const {
//   // for now allow for late resolution of read conflict

//   std::unordered_set<std::shared_ptr<const Commit>> visited;
//   return get_committed_recursive(head, number, visited);
// }

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