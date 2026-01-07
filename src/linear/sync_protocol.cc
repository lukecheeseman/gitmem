#include "linear/sync_protocol.hh"
#include "debug.hh"
#include <iostream>

namespace gitmem {

namespace linear {

LocalVersionStore& get_store(ThreadContext& ctx) {
  return static_cast<LocalVersionStore&>(*ctx.sync);
}

// --------------------
// LinearSyncProtocol
// --------------------

std::ostream &LinearSyncProtocol::print(std::ostream &os) const {
  os << _global_store << std::endl;
  return os;
}

std::optional<LinearConflict>
LinearSyncProtocol::push(LocalVersionStore &local) {
  if (auto conflict = _global_store.check_conflicts(local.base_timestamp(),
                                                    local.staged_changes())) {

    // reshape the conflict
    return std::make_optional<LinearConflict>(
        _global_store.get_object_name(conflict->object),
        std::make_pair(conflict->local_base, conflict->global_head));
  }

  Timestamp new_base = _global_store.apply_changes(
      local.base_timestamp(), local.staged_changes());

  local.clear_staging();
  local.advance_base(new_base);
  return std::nullopt;
}

std::optional<LinearConflict>
LinearSyncProtocol::pull(LocalVersionStore &local) {
  if (auto conflict = _global_store.check_conflicts(local.base_timestamp(),
                                                    local.staged_changes())) {

    return std::make_optional<LinearConflict>(
        _global_store.get_object_name(conflict->object),
        std::make_pair(conflict->local_base, conflict->global_head));
  }

  local.advance_base(_global_store.current_timestamp());
  return std::nullopt;
}

LinearSyncProtocol::~LinearSyncProtocol() = default;

ReadResult LinearSyncProtocol::read(ThreadContext &ctx,
                                               const std::string &var) {
  ObjectNumber number = _global_store.get_object_number(var);

  auto& store = get_store(ctx);

  if (auto result = store.get_staged(number))
    return *result;

  std::optional<size_t> value = _global_store.get_version_for_timestamp(
      number, store.base_timestamp());
  if (value)
    return *value;

  // we do not need to record the staged value for correctness
  // TODO: there is something about working out if a value has changed vs been
  // written

  return std::monostate{};
}

void LinearSyncProtocol::write(ThreadContext &ctx, const std::string &var,
                               size_t value) {
  // write into the staging area of the thread
  auto& store = get_store(ctx);
  store.stage(_global_store.get_object_number(var), value);
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_spawn(ThreadContext &parent, ThreadContext &child) {
  // TODO: i think we can drop the globalcontext but check after branching is
  // added

  // push parent to global history
  auto& store = get_store(parent);
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  // pull into the child
  store = get_store(child);
  if (auto conflict = pull(store)) {
    throw std::logic_error("This code path should never be reached");
  }

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_join(ThreadContext &joiner, ThreadContext &joinee) {
  // we assume the joinee has already terminated and pushed

  // pull changes into parent
  auto& store = get_store(joiner);
  if (auto conflict = pull(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_start(ThreadContext &thread) {
  // pull state from global history
  auto& store = get_store(thread);
  auto conflict = pull(store);
  assert(!conflict && "cannot conflict from starting state");

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_end(ThreadContext &thread) {
  // push changes to global history
  auto& store = get_store(thread);
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_lock(ThreadContext &thread, Lock &lock) {

  auto& store = get_store(thread);
  if (auto conflict = pull(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_unlock(ThreadContext &thread, Lock &) {
  // push changes to global history
  auto& store = get_store(thread);
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

} // namespace linear

} // namespace gitmem
