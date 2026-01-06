#include "linear/sync_protocol.hh"
#include "debug.hh"
#include <iostream>

namespace gitmem {

namespace linear {

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

std::optional<size_t> LinearSyncProtocol::read(ThreadContext &ctx,
                                               const std::string &var) {
  ObjectNumber number = _global_store.get_object_number(var);

  auto& store = std::get<ThreadContext::LinearData>(ctx.sync).store;

  if (auto result = store.get_staged(number))
    return result;

  std::optional<size_t> value = _global_store.get_version_for_timestamp(
      number, store.base_timestamp());
  if (!value)
    return std::nullopt;

  // we do not need to record the staged value for correctness
  // TODO: there is something about working out if a value has changed vs been
  // written

  return *value;
}

void LinearSyncProtocol::write(ThreadContext &ctx, const std::string &var,
                               size_t value) {
  // write into the staging area of the thread
  auto& store = std::get<ThreadContext::LinearData>(ctx.sync).store;
  store.stage(_global_store.get_object_number(var), value);
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_spawn(ThreadContext &parent, ThreadContext &child,
                             GlobalContext &gctx) {
  // TODO: i think we can drop the globalcontext but check after branching is
  // added

  // push parent to global history
  auto& store = std::get<ThreadContext::LinearData>(parent.sync).store;
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  // pull into the child
  store = std::get<ThreadContext::LinearData>(child.sync).store;
  if (auto conflict = pull(store)) {
    throw std::logic_error("This code path should never be reached");
  }

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_join(ThreadContext &joiner, ThreadContext &joinee,
                            GlobalContext &gctx) {
  // we assume the joinee has already terminated and pushed

  // pull changes into parent
  auto& store = std::get<ThreadContext::LinearData>(joiner.sync).store;
  if (auto conflict = pull(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_start(ThreadContext &thread, GlobalContext &gctx) {
  // pull state from global history
  auto& store = std::get<ThreadContext::LinearData>(thread.sync).store;
  auto conflict = pull(store);
  assert(!conflict && "cannot conflict from starting state");

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_end(ThreadContext &thread, GlobalContext &gctx) {
  // push changes to global history
  auto& store = std::get<ThreadContext::LinearData>(thread.sync).store;
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_lock(ThreadContext &thread, Lock &lock,
                            GlobalContext &gctx) {

  auto& store = std::get<ThreadContext::LinearData>(thread.sync).store;
  if (auto conflict = pull(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
LinearSyncProtocol::on_unlock(ThreadContext &thread, Lock &,
                              GlobalContext &gctx) {

  // push changes to global history
  auto& store = std::get<ThreadContext::LinearData>(thread.sync).store;
  if (auto conflict = push(store))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

} // namespace linear

} // namespace gitmem
