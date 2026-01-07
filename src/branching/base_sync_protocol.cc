#include "branching/base_sync_protocol.hh"

namespace gitmem {

namespace branching {

LocalVersionStore& get_store(ThreadContext& ctx) {
  return static_cast<LocalVersionStore&>(*ctx.sync);
}

LockState& get_store(Lock& ctx) {
  return static_cast<LockState&>(*ctx.sync);
}

// --------------------
// BranchingSyncProtocolBase
// --------------------

BranchingSyncProtocolBase::~BranchingSyncProtocolBase() = default;

std::ostream &BranchingSyncProtocolBase::print(std::ostream &os) const {
  os << _global_store << std::endl;
  return os;
}

ReadResult BranchingSyncProtocolBase::read(ThreadContext &ctx,
                                           const std::string &var) {
  ObjectNumber number = _global_store.get_object_number(var);

  auto& store = get_store(ctx);

  if (auto result = store.get_staged(number))
    return *result;

  // look in commit history
  if (auto result = store.get_committed(number))
    return *result;

  return std::monostate{};
}

void BranchingSyncProtocolBase::write(ThreadContext &ctx, const std::string &var,
                                  size_t value) {
  auto& store = get_store(ctx);
  store.stage(_global_store.get_object_number(var), value);
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_spawn(ThreadContext &parent, ThreadContext &child) {
  auto& parent_store = get_store(parent);
  parent_store.commit_staging();

  auto& child_store = get_store(child);
  child_store.adopt_history(parent_store);

  // a conflict cannot occur here
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_join(ThreadContext &joiner, ThreadContext &joinee) {
  auto& joiner_store = get_store(joiner);
  auto& joinee_store = get_store(joinee);

  joiner_store.commit_staging();
  assert(joinee_store.has_commited() && "joinee has staged changes");

  std::optional<Conflict> conflict = joiner_store.merge_with_commit(joinee_store.get_head());
  if (conflict) {
    return std::make_unique<BranchingConflict>(
      _global_store.get_object_name(conflict->obj),
      std::make_pair(conflict->timestamp_a, conflict->timestamp_b));
  }

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_start(ThreadContext &thread) {
  // nothing to do, the thread will have inhereted the parent commit on spawn
  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_end(ThreadContext &thread) {
  auto& store = get_store(thread);
  store.commit_staging();

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_lock(ThreadContext &thread, Lock &lock) {
  auto& store = get_store(thread);
  store.commit_staging();

  LockState& lock_state = get_store(lock);
  std::shared_ptr<const Commit> lock_commit = lock_state.commit;

  if (lock_commit != nullptr) {
    std::optional<Conflict> conflict = store.merge_with_commit(lock_commit);
    if (conflict) {
      return std::make_unique<BranchingConflict>(
        _global_store.get_object_name(conflict->obj),
        std::make_pair(conflict->timestamp_a, conflict->timestamp_b));
    }
  }

  lock_state.commit = store.get_head();

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocolBase::on_unlock(ThreadContext &thread, Lock &lock) {
  auto& store = get_store(thread);
  store.commit_staging();

  LockState& lock_state = get_store(lock);
  std::shared_ptr<const Commit> lock_commit = lock_state.commit;

  // we don't need to check for conflicts
  if (lock_commit != nullptr) {
    std::optional<Conflict> conflict = store.merge_with_commit(lock_commit);
    assert (!conflict);
  }

  lock_state.commit = store.get_head();

  return std::nullopt;
}

} // end branching

} // end gitmem