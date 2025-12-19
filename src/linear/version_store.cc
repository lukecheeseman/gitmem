#include <stdexcept>
#include <iostream>

#include "version_store.hh"
#include "sync_protocol.hh"

namespace gitmem {

namespace linear {

// -----------------------------
// LocalVersionStore
// -----------------------------

void LocalVersionStore::stage(ObjectNumber obj, Value value) {
  _staging[obj] = value;
}

void LocalVersionStore::clear_staging() {
  _staging.clear();
}

void LocalVersionStore::advance_base(Timestamp ts) {
  _base_timestamp = ts;
}

std::optional<Value> LocalVersionStore::get_staged(ObjectNumber obj) {
  auto it = _staging.find(obj);
  return it != _staging.end() ? std::make_optional(it->second) : std::nullopt;
}

// -----------------------------
// GlobalVersionStore
// -----------------------------

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
  for (const auto& [name, number] : _object_numbers) {
    if (number == find)
      return name;
  }
  assert(false && "failed to find object name for object number");
  return "";
}

std::optional<Value> GlobalVersionStore::get_version_for_timestamp(ObjectNumber obj, Timestamp ts) const {
  const auto it = _history.find(obj);

  if (it == _history.end())
    return std::nullopt;

  const VersionHistory& history = it->second;
  for (VersionHistory::const_reverse_iterator riter = history.rbegin(); riter != history.rend(); ++riter) {
    if (riter->timestamp() <= ts)
      return riter->value();
  }

  return std::nullopt;
}

std::optional<Conflict> GlobalVersionStore::check_conflicts(
  Timestamp base,
  const std::unordered_map<ObjectNumber, Value>& changes
) const {
  for (const auto& [obj, _] : changes) {
    auto it = _history.find(obj);
    if (it == _history.end()) {
      continue;
    }

    const Version& latest = it->second.back();
    if (latest.timestamp() > base) {
      return Conflict{
        .object = obj,
        .local_base = base,
        .global_head = latest.timestamp()
      };
    }
  }
  return std::nullopt;
}

Timestamp GlobalVersionStore::apply_changes(
  Timestamp base,
  const std::unordered_map<ObjectNumber, Value>& changes
) {
  if (auto conflict = check_conflicts(base, changes)) {
    throw std::logic_error("apply_changes called with conflicts");
  }

  Timestamp new_ts = ++_timestamp;
  for (const auto& [obj, value] : changes) {
    _history[obj].emplace_back(new_ts, value);
  }

  _timestamp = new_ts;
  return new_ts;
}

} // namespace linear

} // namespace gitmem