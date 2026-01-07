#pragma once

#include "base_sync_protocol.hh"

namespace gitmem {

namespace branching {

class BranchingLazySyncProtocol final : public BranchingSyncProtocolBase {
public:
  SyncKind kind() const override { return SyncKind::BranchingLazy; };
};

}

}