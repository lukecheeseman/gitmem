#include "branching/sync_protocol.hh"

namespace gitmem {

namespace branching {

// --------------------
// BranchingSyncProtocol
// --------------------

BranchingSyncProtocol::~BranchingSyncProtocol() = default;

std::ostream &BranchingSyncProtocol::print(std::ostream &os) const {
  os << _global_store << std::endl;
  return os;
}

std::optional<size_t> BranchingSyncProtocol::read(ThreadContext &ctx,
                                                  const std::string &var) {
  ObjectNumber number = _global_store.get_object_number(var);

  auto& store = std::get<ThreadContext::BranchingData>(ctx.sync).store;

  if (auto result = store.get_staged(number))
    return result;

  // look in commit history
  return store.get_committed(number);
}

void BranchingSyncProtocol::write(ThreadContext &ctx, const std::string &var,
                                  size_t value) {
  auto& store = std::get<ThreadContext::BranchingData>(ctx.sync).store;
  store.stage(_global_store.get_object_number(var), value);
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_spawn(ThreadContext &parent, ThreadContext &child,
                                GlobalContext &) {
  auto& parent_store = std::get<ThreadContext::BranchingData>(parent.sync).store;
  parent_store.commit_staging();

  auto& child_store = std::get<ThreadContext::BranchingData>(child.sync).store;
  child_store.adopt_history(parent_store);

  // a conflict cannot occur here
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_join(ThreadContext &joiner, ThreadContext &joinee,
                               GlobalContext &) {
  auto& joiner_store = std::get<ThreadContext::BranchingData>(joiner.sync).store;
  auto& joinee_store = std::get<ThreadContext::BranchingData>(joinee.sync).store;

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
BranchingSyncProtocol::on_start(ThreadContext &thread, GlobalContext &gctx) {
  // nothing to do, the thread will have inhereted the parent commit on spawn
  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_end(ThreadContext &thread, GlobalContext &gctx) {
  auto& store = std::get<ThreadContext::BranchingData>(thread.sync).store;
  store.commit_staging();

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_lock(ThreadContext &thread, Lock &lock,
                               GlobalContext &) {
  auto& thread_store = std::get<ThreadContext::BranchingData>(thread.sync).store;
  thread_store.commit_staging();

  std::shared_ptr<const Commit> lock_commit = lock.branching.commit;

  if (lock_commit != nullptr) {
    std::optional<Conflict> conflict = thread_store.merge_with_commit(lock_commit);
    if (conflict) {
      return std::make_unique<BranchingConflict>(
        _global_store.get_object_name(conflict->obj),
        std::make_pair(conflict->timestamp_a, conflict->timestamp_b));
    }
  }

  lock.branching.commit = thread_store.get_head();

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_unlock(ThreadContext &thread, Lock &lock,
                                 GlobalContext &) {
  auto& thread_store = std::get<ThreadContext::BranchingData>(thread.sync).store;
  thread_store.commit_staging();

  std::shared_ptr<const Commit> lock_commit = lock.branching.commit;

  // we don't need to check for conflicts
  if (lock_commit != nullptr) {
    std::optional<Conflict> conflict = thread_store.merge_with_commit(lock_commit);
    assert (!conflict);
  }

  lock.branching.commit = thread_store.get_head();

  return std::nullopt;
}

} // end branching

} // end gitmem