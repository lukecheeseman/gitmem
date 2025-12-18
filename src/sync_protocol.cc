#include "sync_protocol.hh"

namespace gitmem {

// --------------------
// LinearSyncProtocol
// --------------------

std::optional<size_t> LinearSyncProtocol::read(ThreadContext& ctx, const std::string& var) {
  return std::nullopt;
}

void LinearSyncProtocol::write(ThreadContext& ctx, const std::string& var, size_t value) {

}

std::optional<Conflict> LinearSyncProtocol::on_spawn(
  ThreadContext& parent,
  ThreadContext& child,
  GlobalContext& gctx
) {
  // push parent to global history
  // child inherits parent view
  // (implementation uses your linear history abstraction)
}

std::optional<Conflict> LinearSyncProtocol::on_join(
  ThreadContext& joiner,
  ThreadContext& joinee,
  GlobalContext& gctx
) {
  // push both, pull into joiner
  return std::nullopt;
}

std::optional<Conflict> LinearSyncProtocol::on_lock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext& gctx
) {
  // push thread, pull from global
  return std::nullopt;
}

std::optional<Conflict> LinearSyncProtocol::on_unlock(
  ThreadContext& thread,
  Lock&,
  GlobalContext& gctx
) {
  // push thread
}

// --------------------
// BranchingSyncProtocol
// --------------------

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
// std::optional<Conflict> pull(Globals &dst, Globals &src) {
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
  return std::nullopt;
}

void BranchingSyncProtocol::write(ThreadContext& ctx, const std::string& var, size_t value) {

}

std::optional<Conflict> BranchingSyncProtocol::on_spawn(
  ThreadContext& parent,
  ThreadContext& child,
  GlobalContext&
) {
  // commit(parent.globals);
  // child.globals = parent.globals;
}

std::optional<Conflict> BranchingSyncProtocol::on_join(
  ThreadContext& joiner,
  ThreadContext& joinee,
  GlobalContext&
) {
  // commit(joiner.globals);
  // commit(joinee.globals);
  // return pull(joiner.globals, joinee.globals);
  return std::nullopt;
}

std::optional<Conflict> BranchingSyncProtocol::on_lock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext&
) {
  // commit(thread.globals);
  // return pull(thread.globals, lock.globals);
  return std::nullopt;
}

std::optional<Conflict> BranchingSyncProtocol::on_unlock(
  ThreadContext& thread,
  Lock& lock,
  GlobalContext&
) {
  // commit(thread.globals);
  // lock.globals = thread.globals;
}

} // namespace gitmem
