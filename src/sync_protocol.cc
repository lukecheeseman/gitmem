#include <iostream>
#include "sync_protocol.hh"
#include "debug.hh"

namespace gitmem {

template<typename T>
std::ostream& Conflict<T>::print(std::ostream& os) const {
  os << "conflict on " << var << " { " << versions.first << ", " << versions.second << " }";
  return os;
}

// --------------------
// LinearSyncProtocol
// --------------------

std::optional<LinearConflict> LinearSyncProtocol::push(linear::LocalVersionStore& local) {
  if (auto conflict = _global_store.check_conflicts(
        local.base_timestamp(),
        local.staged_changes())) {

    // reshape the conflict
    return std::make_optional<LinearConflict>(
      _global_store.get_object_name(conflict->object),
      std::make_pair(conflict->local_base, conflict->global_head)
    );

  }

  linear::Timestamp new_base = _global_store.apply_changes(
    local.base_timestamp(),
    local.staged_changes()
  );

  local.clear_staging();
  local.advance_base(new_base);
  return std::nullopt;
}

std::optional<LinearConflict> LinearSyncProtocol::pull(linear::LocalVersionStore& local) {
  if (auto conflict = _global_store.check_conflicts(
        local.base_timestamp(),
        local.staged_changes())) {

    return std::make_optional<LinearConflict>(
      _global_store.get_object_name(conflict->object),
      std::make_pair(conflict->local_base, conflict->global_head)
    );

  }

  local.advance_base(_global_store.current_timestamp());
  return std::nullopt;
}

LinearSyncProtocol::~LinearSyncProtocol() = default;

std::optional<size_t> LinearSyncProtocol::read(ThreadContext& ctx, const std::string& var) {
  linear::ObjectNumber number = _global_store.get_object_number(var);

  if (auto result = store(ctx).get_staged(number))
    return result;

  std::optional<size_t> value = _global_store.get_version_for_timestamp(number, store(ctx).base_timestamp());
  if (!value)
    return std::nullopt;

  // we do not need to record the staged value for correctness
  // TODO: there is something about working out if a value has changed vs been written

  return *value;
}

void LinearSyncProtocol::write(ThreadContext& ctx, const std::string& var, size_t value) {
  // write into the staging area of the thread
  store(ctx).stage(_global_store.get_object_number(var), value);
}

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_spawn(
  ThreadContext& parent,
  ThreadContext& child,
  GlobalContext& gctx
) {
  // TODO: i think we can drop the globalcontext but check after branching is added
  verbose << "on_spawn" << std::endl;

  // push parent to global history
  if (auto conflict = push(store(parent)))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_join(
  ThreadContext& joiner,
  ThreadContext& joinee,
  GlobalContext& gctx
) {
  verbose << "on_join" << std::endl;
  // we assume the joinee has already terminated and pushed

  // pull changes into parent
  if (auto conflict = pull(store(joiner)))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_start(
    ThreadContext& thread,
    GlobalContext& gctx
) {
  verbose << "on_start" << std::endl;

  // pull state from global history
  auto conflict = pull(store(thread));
  assert(!conflict && "cannot conflict from starting state");

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_end(
    ThreadContext& thread,
    GlobalContext& gctx
  ) {
  verbose << "on_end" << std::endl;

  // push changes to global history
  if (auto conflict = push(store(thread)))
    return std::make_unique<LinearConflict>(std::move(*conflict));

  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_lock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext& gctx
) {
  assert(false && "todo lock");
  // push thread, pull from global
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> LinearSyncProtocol::on_unlock(
  ThreadContext& thread,
  Lock&,
  GlobalContext& gctx
) {
  assert(false && "todo unlock");
  // push thread
  return std::nullopt;
}

// --------------------
// BranchingSyncProtocol
// --------------------

BranchingSyncProtocol::~BranchingSyncProtocol() = default;

// /* At a commit point, walk through all the versioned variables and see if
//   * they have a pending commit, if so commit the value by appending to
//   * the variables history.
//   */
// void commit(Globals &globals) {
//     for (auto& [var, global] : globals) {
//         if (global.commit)
//         {
//             global.history.push_back(*global.commit);
//             verbose << "Committed global '" << var << "' with id " << *global.commit << std::endl;
//             global.commit.reset();
//         }
//     }
// }


// /* A versioned value can be fastforwarded to another version, if one
//   * version's history is a prefix of another version's history.
//   * A conflict between two commit histories exists if neither history is a
//   * prefix of the other.
//   */
// std::optional<std::pair<Commit, Commit>> has_conflict(CommitHistory& h1, CommitHistory& h2)
// {
//     size_t length = std::min(h1.size(), h2.size());

//     for (size_t i = 0; i < length; i++)
//     {
//         if (h1[i] != h2[i]) return std::pair<Commit, Commit>{h1[i], h2[i]};
//     }

//     return std::nullopt;
// }

// /* Walk through all the global versions from source and update the versions
//   * in destination to be the most up-to-date version (this could come from
//   * either source or destination). This means destination will now also
//   * include variables it previously did not know about.
//   */
// std::optional<std::unique_ptr<ConflictBase>> pull(Globals &dst, Globals &src) {
//     for (auto& [var, global] : src) {
//         if (dst.contains(var))
//         {
//             auto& src_var = src[var];
//             auto& dst_var = dst[var];
//             if (auto conflict = has_conflict(src_var.history, dst_var.history))
//             {
//                 auto [s1, s2] = *conflict;
//                 verbose << "A data race on '" << var << "' was detected from commits " << s1 << " and " << s2 << std::endl;
//                 return Conflict(var, *conflict);
//             }
//             else if (src_var.history.size() > dst_var.history.size())
//             {
//                 verbose << "Fast-forward '" << var << "' to id " << src_var.val << std::endl;
//                 dst_var.val = src_var.val;
//                 dst_var.history = src_var.history;
//             }
//         }
//         else
//         {
//             dst[var].val = src[var].val;
//             dst[var].history = src[var].history;
//         }
//     }
//     return std::nullopt;
// }

std::optional<size_t> BranchingSyncProtocol::read(ThreadContext& ctx, const std::string& var) {
  assert(false && "Todo read");
  return std::nullopt;
}

void BranchingSyncProtocol::write(ThreadContext& ctx, const std::string& var, size_t value) {
  assert(false && "Todo write");
}

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_spawn(
  ThreadContext& parent,
  ThreadContext& child,
  GlobalContext&
) {
  assert(false && "Todo on_spawn");
  // commit(parent.globals);
  // child.globals = parent.globals;
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_join(
  ThreadContext& joiner,
  ThreadContext& joinee,
  GlobalContext&
) {
  assert(false && "Todo on_join");
  // commit(joiner.globals);
  // commit(joinee.globals);
  // return pull(joiner.globals, joinee.globals);
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_start(
    ThreadContext& thread,
    GlobalContext& gctx
) {
  assert(false && "Todo on_start");
  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_end(
    ThreadContext& thread,
    GlobalContext& gctx
  ) {
  assert(false && "Todo on_end");
  return std::nullopt;
};

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_lock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext&
) {
  assert(false && "Todo on_lock");
  // commit(thread.globals);
  // return pull(thread.globals, lock.globals);
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>> BranchingSyncProtocol::on_unlock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext&
) {
  assert(false && "Todo on_unlock");
  // commit(thread.globals);
  // lock.globals = thread.globals;
  return std::nullopt;
}

} // namespace gitmem
