#pragma once

#include "../sync_protocol.hh"
#include "version_store.hh"

namespace gitmem {

using BranchingConflict = Conflict<branching::Timestamp>;

namespace branching {

class BranchingSyncProtocolBase : public SyncProtocol {
protected:
  GlobalVersionStore _global_store;

public:
  ~BranchingSyncProtocolBase() override;

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

  std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const override {
    return std::make_unique<LocalVersionStore>(tid);
  }

  std::unique_ptr<LockSyncState> make_lock_state() const override {
    return std::make_unique<LockState>();
  }
};

} // end branching

} // end gitmem