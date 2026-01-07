#pragma once

#include "branching/base_sync_protocol.hh"
#include "branching/lazy/version_store.hh"

namespace gitmem {

namespace branching {

class BranchingLazySyncProtocol final : public BranchingSyncProtocolBase {
public:
  ~BranchingLazySyncProtocol() = default;

  SyncKind kind() const override { return SyncKind::BranchingLazy; };

  std::unique_ptr<ThreadSyncState> make_thread_state(ThreadID tid) const override {
    return std::make_unique<LazyLocalVersionStore>(tid);
  }
};

}

}