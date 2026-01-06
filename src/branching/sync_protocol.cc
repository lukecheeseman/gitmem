#include "branching/sync_protocol.hh"

namespace gitmem {

namespace branching {

// --------------------
// BranchingSyncProtocol
// --------------------

BranchingSyncProtocol::~BranchingSyncProtocol() = default;

std::ostream &BranchingSyncProtocol::print(std::ostream &os) const {
  assert(false && "TODO");
  return os;
}

// /* At a commit point, walk through all the versioned variables and see if
//   * they have a pending commit, if so commit the value by appending to
//   * the variables history.
//   */
// void commit(Globals &globals) {
//     for (auto& [var, global] : globals) {
//         if (global.commit)
//         {
//             global.history.push_back(*global.commit);
//             verbose << "Committed global '" << var << "' with id " <<
//             *global.commit << std::endl; global.commit.reset();
//         }
//     }
// }

// /* A versioned value can be fastforwarded to another version, if one
//   * version's history is a prefix of another version's history.
//   * A conflict between two commit histories exists if neither history is a
//   * prefix of the other.
//   */
// std::optional<std::pair<Commit, Commit>> has_conflict(CommitHistory& h1,
// CommitHistory& h2)
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
// std::optional<std::unique_ptr<ConflictBase>> pull(Globals &dst, Globals &src)
// {
//     for (auto& [var, global] : src) {
//         if (dst.contains(var))
//         {
//             auto& src_var = src[var];
//             auto& dst_var = dst[var];
//             if (auto conflict = has_conflict(src_var.history,
//             dst_var.history))
//             {
//                 auto [s1, s2] = *conflict;
//                 verbose << "A data race on '" << var << "' was detected from
//                 commits " << s1 << " and " << s2 << std::endl; return
//                 Conflict(var, *conflict);
//             }
//             else if (src_var.history.size() > dst_var.history.size())
//             {
//                 verbose << "Fast-forward '" << var << "' to id " <<
//                 src_var.val << std::endl; dst_var.val = src_var.val;
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

// Spawning is a sync point, commit local pending commits, and
// copy the global state to the spawned thread
// commit(ctx.globals);

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_spawn(ThreadContext &parent, ThreadContext &child,
                                GlobalContext &) {
  auto& parent_store = std::get<ThreadContext::BranchingData>(parent.sync).store;
  parent_store.commit_staging();

  auto& child_store = std::get<ThreadContext::BranchingData>(child.sync).store;
  child_store.adopt_history(parent_store.exported_head());

  // a conflict cannot occur here
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_join(ThreadContext &joiner, ThreadContext &joinee,
                               GlobalContext &) {
  assert(false && "Todo on_join");
  // commit(joiner.globals);
  // commit(joinee.globals);
  // return pull(joiner.globals, joinee.globals);
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
  assert(false && "Todo on_lock");
  // commit(thread.globals);
  // return pull(thread.globals, lock.globals);
  return std::nullopt;
}

std::optional<std::unique_ptr<ConflictBase>>
BranchingSyncProtocol::on_unlock(ThreadContext &thread, Lock &lock,
                                 GlobalContext &) {
  assert(false && "Todo on_unlock");
  // commit(thread.globals);
  // lock.globals = thread.globals;
  return std::nullopt;
}

} // end branching

} // end gitmem