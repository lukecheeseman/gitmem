#include "base_memory_model.hh"
#include "overloaded.hh"
#include <stdexcept>
#include "branching/eager/memory_model.hh"
#include "branching/lazy/memory_model.hh"

namespace gitmem {

namespace branching {

LocalVersionStore& get_store(ThreadContext& ctx) {
  return static_cast<LocalVersionStore&>(*ctx.sync);
}

LockState& get_store(Lock& ctx) {
  return static_cast<LockState&>(*ctx.sync);
}

// --------------------
// BranchingMemoryModelBase
// --------------------

BranchingMemoryModelBase::~BranchingMemoryModelBase() = default;

std::ostream &BranchingMemoryModelBase::print(std::ostream &os) const {
  return os;
}

ReadResult BranchingMemoryModelBase::read(ThreadContext &ctx,
                                           const std::string &var) {
  auto& store = get_store(ctx);

  return std::visit(overloaded{
    [](std::monostate) -> ReadResult { return std::monostate{}; },
    [](const ValueWithSource& v) -> ReadResult { return v; },
    [&](const Conflict& c) -> ReadResult {
      return std::make_shared<BranchingConflict>(c);
    },

  }, store.read(var));
}

void BranchingMemoryModelBase::write(ThreadContext &ctx, const std::string &var,
                                  ValueWithSource value) {
  auto& store = get_store(ctx);
  store.stage(var, value);
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_spawn(ThreadContext &parent, ThreadContext &child) {
  auto& parent_store = get_store(parent);
  parent_store.commit_staging();

  auto& child_store = get_store(child);
  child_store.adopt_history(parent_store);

  // a conflict cannot occur here
  return std::nullopt;
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_join(ThreadContext &joiner, ThreadContext &joinee) {
  auto& joiner_store = get_store(joiner);
  auto& joinee_store = get_store(joinee);

  joiner_store.commit_staging();
  assert(joinee_store.has_commited() && "joinee has staged changes");

  std::optional<Conflict> conflict = joiner_store.merge_with_commit(joinee_store.get_head());
  if (conflict) {
    return std::make_shared<BranchingConflict>(*conflict);
  }

  return std::nullopt;
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_start(ThreadContext &thread) {
  // nothing to do, the thread will have inhereted the parent commit on spawn
  return std::nullopt;
};

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_end(ThreadContext &thread) {
  auto& store = get_store(thread);
  store.commit_staging();

  return std::nullopt;
};

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_lock(ThreadContext &thread, Lock &lock) {
  auto& store = get_store(thread);
  store.commit_staging();

  LockState& lock_state = get_store(lock);
  std::shared_ptr<const Commit> lock_commit = lock_state.commit;

  if (lock_commit != nullptr) {
    std::optional<Conflict> conflict = store.merge_with_commit(lock_commit);
    if (conflict) {
      return std::make_shared<BranchingConflict>(*conflict);
    }

    lock_state.commit = store.get_head();
  }

  return std::nullopt;
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_unlock(ThreadContext &thread, Lock &lock) {
  auto& store = get_store(thread);
  store.commit_staging();

  // we know that the last committer was this thread, so no need to merge
  // this sort of mixes protocol logic and lock state, i am unsure if this is ideal
  LockState& lock_state = get_store(lock);
  lock_state.commit = store.get_head();

  return std::nullopt;
}

VolatileState& get_store(Volatile& v) {
  return static_cast<VolatileState&>(*v.sync);
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_volatile_read(ThreadContext &thread, Volatile &v) {
  auto& store = get_store(thread);
  store.commit_staging();

  // Acquire: merge the current release so we observe it and everything that
  // happened-before it. May conflict on piggybacked non-volatile state.
  VolatileState& vstate = get_store(v);
  if (vstate.commit != nullptr) {
    if (std::optional<Conflict> conflict = store.merge_with_commit(vstate.commit)) {
      return std::make_shared<BranchingConflict>(*conflict);
    }
  }

  // The value (with its writer as provenance) lives on the Volatile object and
  // is read there by the interpreter -- @v is not versioned.
  return std::nullopt;
}

std::optional<std::shared_ptr<ConflictBase>>
BranchingMemoryModelBase::on_volatile_write(ThreadContext &thread, Volatile &v,
                                            ValueWithSource value) {
  auto& store = get_store(thread);
  store.commit_staging();

  // Release: absorb the previous release first (so the ordinary state is a
  // linear chain, never diverging on @v); this merge may still conflict on
  // piggybacked non-volatile state.
  VolatileState& vstate = get_store(v);
  if (vstate.commit != nullptr) {
    if (std::optional<Conflict> conflict = store.merge_with_commit(vstate.commit)) {
      return std::make_shared<BranchingConflict>(*conflict);
    }
  }

  // The thread's current head is the release point a later acquire merges. The
  // volatile's value lives on the object, not in a commit -- @v is not
  // versioned; `value` carries the write event as provenance for reads.
  vstate.commit = store.get_head();
  v.value = value;

  return std::nullopt;
}

std::string BranchingMemoryModelBase::build_revision_graph_dot(
    const std::vector<const ThreadSyncState*>& thread_states) const {

  std::vector<std::shared_ptr<const Commit>> heads;

  for (const ThreadSyncState* state_ptr : thread_states) {
    const auto* local_store = dynamic_cast<const LocalVersionStore*>(state_ptr);
    if (local_store && local_store->get_head()) {
      heads.push_back(local_store->get_head());
    }
  }

  return build_commit_graph_dot(heads);
}

bool BranchingMemoryModelBase::is_scheduling_point(SyncOperation op) const {
  // For branching protocol, only operations that actually synchronize state
  // (lock/unlock) or require waiting (join) are scheduling points
  switch (op) {
    case SyncOperation::Lock:
    case SyncOperation::Unlock:
    case SyncOperation::Join:
    case SyncOperation::VolatileRead:
    case SyncOperation::VolatileWrite:
      return true;
    case SyncOperation::Spawn:
    case SyncOperation::Start:
    case SyncOperation::End:
      // These just inherit/commit locally - no scheduling decision needed
      return false;
  }
  return false;
}

} // end branching

} // end gitmem