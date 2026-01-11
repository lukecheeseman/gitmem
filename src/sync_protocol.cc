#include "sync_protocol.hh"
#include "linear/sync_protocol.hh"
#include "branching/base_sync_protocol.hh"
#include "debug.hh"

namespace gitmem {

std::unique_ptr<SyncProtocol> make_protocol(SyncKind sync_kind) {
  switch (sync_kind) {
    case SyncKind::Linear:
      return std::make_unique<linear::LinearSyncProtocol>();

    case SyncKind::BranchingEager: {
      auto builder = branching::BranchingSyncProtocolBuilder().eager();
      if (verbose::out.include_empty_commits) {
        builder.with_verbose_commits();
      }
      return builder.build();
    }

    case SyncKind::BranchingLazy: {
      auto builder = branching::BranchingSyncProtocolBuilder().lazy();
      if (verbose::out.include_empty_commits) {
        builder.with_verbose_commits();
      }
      return builder.build();
    }
  }
  std::unreachable();
}

} // namespace gitmem
