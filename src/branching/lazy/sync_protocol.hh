#pragma once

#include "branching/base_sync_protocol.hh"
#include "branching/lazy/version_store.hh"

namespace gitmem {

namespace branching {

class BranchingLazySyncProtocol final : public BranchingSyncProtocolBase {
private:
  bool raise_early_conflicts; // currently not used

public:
  explicit BranchingLazySyncProtocol(bool verbose = false, bool raise_early_conflicts = false)
      : BranchingSyncProtocolBase(verbose), raise_early_conflicts(raise_early_conflicts) {}

  ~BranchingLazySyncProtocol() = default;

  std::unique_ptr<SyncProtocol> clone() const override {
    return std::make_unique<BranchingLazySyncProtocol>(verbose_commits, raise_early_conflicts);
  }

  std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const override {
    return std::make_unique<LazyLocalVersionStore>(tid, verbose_commits, raise_early_conflicts);
  }
};

}

}