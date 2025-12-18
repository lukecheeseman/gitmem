#include "version_store.hh"
#include <stdexcept>

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

// -----------------------------
// GlobalVersionStore
// -----------------------------

ObjectNumber GlobalVersionStore::allocate_object() {
  return _next_object++;
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

    const Version& head = it->second.back();
    if (head.timestamp() > base) {
      return Conflict{
        .object = obj,
        .local_base = base,
        .global_head = head.timestamp()
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

  Timestamp new_ts = _timestamp++;
  for (const auto& [obj, value] : changes) {
    _history[obj].emplace_back(new_ts, value);
  }

  _timestamp = new_ts;
  return new_ts;
}

// -----------------------------
// GlobalVersionHistory (protocol)
// -----------------------------

std::optional<Conflict> GlobalVersionHistory::push(LocalVersionStore& local) {
  if (auto conflict = _global.check_conflicts(
        local.base_timestamp(),
        local.staged_changes())) {
    return conflict;
  }

  Timestamp new_base = _global.apply_changes(
    local.base_timestamp(),
    local.staged_changes()
  );

  local.clear_staging();
  local.advance_base(new_base);
  return std::nullopt;
}

std::optional<Conflict> GlobalVersionHistory::pull(LocalVersionStore& local) {
  if (auto conflict = _global.check_conflicts(
        local.base_timestamp(),
        local.staged_changes())) {
    return conflict;
  }

  local.advance_base(_global.current_timestamp());
  return std::nullopt;
}

} // namespace linear

} // namespace gitmem