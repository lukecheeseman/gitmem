#include "base_version_store.hh"
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

ReadResult LocalVersionStore::read(ObjectNumber obj) const {
  auto it = staging.find(obj);
  if (it != staging.end())
    return it->second;

  return get_committed(obj);
}

void LocalVersionStore::adopt_history(const LocalVersionStore& other) {
  // Inherit the DAG head
  head = other.head;

  // Inherit the last_writer cache so the child sees all latest commits
  last_writer = other.last_writer;
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