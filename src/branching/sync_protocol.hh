#pragma once

#include "../sync_protocol.hh"
#include "version_store.hh"

namespace gitmem {

using BranchingConflict = Conflict<branching::Timestamp>;

namespace branching {

class BranchingSyncProtocol final : public SyncProtocol {
  GlobalVersionStore _global_store;

  // std::unordered_map<Commit, std::shared_ptr<graph::Node>> commit_nodes;

public:
  ~BranchingSyncProtocol() override;
  SyncKind kind() const override { return SyncKind::Branching; };

  std::optional<size_t> read(ThreadContext &ctx,
                             const std::string &var) override;

  void write(ThreadContext &ctx, const std::string &var, size_t value) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_spawn(ThreadContext &parent, ThreadContext &child,
           GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_join(ThreadContext &joiner, ThreadContext &joinee,
          GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_start(ThreadContext &thread, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_end(ThreadContext &thread, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_lock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) override;

  std::optional<std::unique_ptr<ConflictBase>>
  on_unlock(ThreadContext &thread, Lock &lock, GlobalContext &gctx) override;

  std::ostream &print(std::ostream &os) const override;
};

} // end branching

} // end gitmem