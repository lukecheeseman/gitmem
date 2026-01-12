#pragma once

#include "branching/base_sync_protocol.hh"
#include "branching/eager/version_store.hh"

namespace gitmem {

namespace branching {

class BranchingEagerSyncProtocol final : public BranchingSyncProtocolBase {
public:
  explicit BranchingEagerSyncProtocol(bool verbose = false)
      : BranchingSyncProtocolBase(verbose) {}

  ~BranchingEagerSyncProtocol() = default;

  std::unique_ptr<SyncProtocol> clone() const override {
    return std::make_unique<BranchingEagerSyncProtocol>(verbose_commits);
  }

  std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const override {
    return std::make_unique<EagerLocalVersionStore>(tid, verbose_commits);
  }
};

}

}